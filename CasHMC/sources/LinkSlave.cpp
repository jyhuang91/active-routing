/*********************************************************************************
*  CasHMC v1.2 - 2016.09.27
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright (c) 2016, Dong-Ik Jeon
*                      Ki-Seok Chung
*                      Hanyang University
*                      estwings57 [at] gmail [dot] com
*  All rights reserved.
*********************************************************************************/

#include "LinkSlave.h"
#include "LinkMaster.h"
#include "CrossbarSwitch.h" // Jiayi, 02/06

namespace CasHMC
{

  LinkSlave::LinkSlave(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_LINK_BUF, MAX_LINK_BUF),
    localLinkSlaveID(id)
  {
    downstream = down;
    is_link = true;
    classID << localLinkSlaveID;
    header = "   (LS_";
    header += downstream ? "D" : "U";
    header += classID.str() + ")";

    slaveSEQ = 0;
    countdownCRC = 0;
    startCRC = false;
    downBufferDest = NULL;
    upBufferDest = NULL;
    localLinkMaster = NULL;
  }

  LinkSlave::LinkSlave(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down, string headerPrefix):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_LINK_BUF, MAX_LINK_BUF),
    localLinkSlaveID(id)
  {
    downstream = down;
    is_link = true;
    classID << localLinkSlaveID;
    header = "   (" + headerPrefix;
    header += "_LS_";
    header += downstream ? "D" : "U";
    header += classID.str() + ")";

    slaveSEQ = 0;
    countdownCRC = 0;
    startCRC = false;
    downBufferDest = NULL;
    upBufferDest = NULL;
    localLinkMaster = NULL;
  }

  LinkSlave::~LinkSlave()
  {
    linkRxTx.clear();
  }

  //
  //Callback receiving packet result
  //
  void LinkSlave::CallbackReceiveDown(Packet *downEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
        }*/
    if(downEle->packetType == FLOW) {
      ERROR(header<<"  == Error - flow packet "<<*downEle<<" is not meant to be here");
      exit(0);
    }
    else {
      if(!chkReceive) {
        ERROR(header<<"  == Error - packet "<<*downEle<<" is missed, because link slave buffer is FULL (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
    }
  }
  void LinkSlave::CallbackReceiveUp(Packet *upEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
        }*/
    if(upEle->packetType == FLOW) {
      ERROR(header<<"  == Error - flow packet "<<*upEle<<" is not meant to be here");
      exit(0);
    }
    else {
      if(!chkReceive) {
        ERROR(header<<"  == Error - packet "<<*upEle<<" is missed, because link slave buffer is FULL (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
    }
  }

  //
  //Update the state of link slave
  //
  void LinkSlave::Update()
  {
    //Extracting flow control and checking CRC, SEQ from linkRxTx packet 
    if(linkRxTx.size() > 0) {
      //Make sure that linkRxTx[0] is not virtual tail packet.
      if(linkRxTx[0] == NULL) {
        ERROR(header<<"  == Error - linkRxTx[0] is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      else {
        for(int i=0; i<linkRxTx.size(); i++) {
          if(linkRxTx[i]->bufPopDelay == 0) {
            //Retry control
            if(linkRxTx[i]->CMD == IRTRY) {
              if(linkRxTx[i]->bufPopDelay == 0) {
                linkRxTx[i]->chkRRP = true;
                localLinkMaster->UpdateRetryPointer(linkRxTx[i]);
                if(linkRxTx[i]->FRP == 1) {
                  if(localLinkMaster->currentState != LINK_RETRY) {
                    localLinkMaster->LinkRetry(linkRxTx[i]);
                  }
                }
                else if(linkRxTx[i]->FRP == 2) {
                  if(localLinkMaster->currentState != NORMAL) {
                    localLinkMaster->FinishRetry();
                    currentState = NORMAL;
                    slaveSEQ = 0;
                  }
                }
                delete linkRxTx[i];
                linkRxTx.erase(linkRxTx.begin()+i);
                break;
              }
            }
            else if(currentState == START_RETRY) {
              delete linkRxTx[i];
              linkRxTx.erase(linkRxTx.begin()+i);
              break;
            }

            //Flow control
            if(linkRxTx[i]->chkRRP == false) {
              linkRxTx[i]->chkRRP = true;
              localLinkMaster->UpdateRetryPointer(linkRxTx[i]);

              //PRET packet is not saved in the retry buffer (No need to check CRC)
              if(linkRxTx[i]->CMD == PRET) {
                delete linkRxTx[i];
                linkRxTx.erase(linkRxTx.begin()+i);
                break;
              }
            }

            //Count CRC calculation time
            if(linkRxTx[i]->chkRRP == true && linkRxTx[i]->chkCRC == false) {
              if(CRC_CHECK && !startCRC) {
                startCRC = true;
                countdownCRC = ceil(CRC_CAL_CYCLE * linkRxTx[i]->LNG);
              }

              if(CRC_CHECK && countdownCRC > 0) {
                DEBUG(ALI(18)<<header<<ALI(15)<<*linkRxTx[i]<<(downstream ? "Down) " : "Up)   ")
                    <<"WAITING CRC calculation ("<<countdownCRC<<"/"<<ceil(CRC_CAL_CYCLE*linkRxTx[i]->LNG)<<")");
              }
              else {
                //Error check
                linkRxTx[i]->chkCRC = true;
                if(CheckNoError(linkRxTx[i])) {
                  localLinkMaster->ReturnRetryPointer(linkRxTx[i]);
#ifdef DEBUG_FLOW_CONTROL
                  CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (downBufferDest);
                  LinkSlave *lsp = dynamic_cast<LinkSlave *> (localLinkMaster->linkP->linkSlaveP);
                  CrossbarSwitch *from_xbar = NULL;
                  if (lsp) {
                    from_xbar = dynamic_cast<CrossbarSwitch *> (lsp->downBufferDest);
                  }
                  if (xbar && xbar->cubeID == 0 && from_xbar && from_xbar->cubeID == 1) {
                    cout << CYCLE() << "cube#" << xbar->cubeID << " is updating tokens from cube#" << from_xbar->cubeID << endl;
                  }
#endif
                  localLinkMaster->UpdateToken(linkRxTx[i]);
                  if(linkRxTx[i]->CMD != TRET) {
                    if (linkRxTx[i]->packetType == REQUEST) ReceiveDown(linkRxTx[i]);
                    else ReceiveUp(linkRxTx[i]);
/*                    if (linkRxTx[i]->CMD == ACT_ADD || linkRxTx[i]->CMD == ACT_MULT) {  // Jiayi, 03/15/17
                      uint64_t dest_addr = linkRxTx[i]->DESTADRS;
                      CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (downBufferDest);

                      LinkSlave *linkSlaveP = dynamic_cast<LinkSlave *> (localLinkMaster->linkP->linkSlaveP);
                      assert(linkSlaveP);
                      CrossbarSwitch *neigh_xbar = dynamic_cast<CrossbarSwitch *> (linkSlaveP->downBufferDest);
                      int parent_cube = neigh_xbar ? neigh_xbar->cubeID : xbar->cubeID;
//#ifdef DEBUG_UPDATE
//                      if (parent_cube == 0) {
//                        cout << CYCLE() << (linkRxTx[i]->CMD == ACT_ADD ? "ACT_ADD" : "ACT_MULT")
//                          << (linkRxTx[i]->packetType == REQUEST? " request" : " response") << " arrived at cube#"
//                          << xbar->cubeID << " from cube#" << parent_cube << endl;
//                      }
//#endif
                      Receive(linkRxTx[i]);
                    } else if (linkRxTx[i]->CMD == ACT_GET) {
//#ifdef DEBUG_GATHER
//                      cout << CYCLE() << "Active-Routing: This is an Active GET ";
//                      cout << ((linkRxTx[i]->packetType == REQUEST) ? "Request" : "Response") << endl;
//#endif
                      Receive(linkRxTx[i]);
                    } else {  // normal packet
                      Receive(linkRxTx[i]);
                    }*/
                  }
                  else {
                    delete linkRxTx[i];
                  }
                  linkRxTx.erase(linkRxTx.begin()+i);
                  break;
                }
                //Error abort mode
                else {
                  if(localLinkMaster->retryStartPacket == NULL) {
                    localLinkMaster->retryStartPacket = new Packet(*linkRxTx[i]);
                  }
                  else {
                    ERROR(header<<"  == Error - localLinkMaster->retryStartPacket is NOT NULL  (CurrentClock : "<<currentClockCycle<<")");
                    exit(0);
                  }
                  localLinkMaster->StartRetry(linkRxTx[i]);
                  localLinkMaster->linkP->statis->errorPerLink[linkSlaveID]++;
                  for(int j=0; j<linkRxTx.size(); j++) {
                    if(linkRxTx[j] != NULL) {
                      delete linkRxTx[j];
                    }
                    linkRxTx.erase(linkRxTx.begin()+j);
                    j--;
                  }
                  linkRxTx.clear();
                  currentState = START_RETRY;
                }
                startCRC = false;
              }
              countdownCRC = (countdownCRC>0) ? countdownCRC-1 : 0;
            }
          }
        }
      }
    }

    //Sending response packet
    bool up_sent = false; //FIXME
    if (upBuffers.size() > 0) {
      bool chkRcv = downstream ? downBufferDest->ReceiveUp(upBuffers[0]) : upBufferDest->ReceiveUp(upBuffers[0]);
      if (chkRcv) {
        localLinkMaster->ReturnToken(upBuffers[0]);
        DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<(downstream ? "Down) " : "Up)    ")
            <<"SENDING packet to "<<(downstream ? "crossbar switch (CS)" : "HMC controller (HC)"));
        upBuffers.erase(upBuffers.begin(), upBuffers.begin()+upBuffers[0]->LNG);
        up_sent = true;
      }
      else {
        //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<"(downstream ? "Down) Crossbar switch" : "Up)   HMC controller")<<" buffer FULL");
      }
    }

    //Sending packet
    if(!up_sent && downBuffers.size() > 0) {
      assert(downstream);
      bool chkRcv = downBufferDest->ReceiveDown(downBuffers[0]);
      /*bool chkRcv = (downstream ?
          (Buffers[0]->packetType == REQUEST ? downBufferDest->ReceiveDown(Buffers[0]) :
           downBufferDest->ReceiveUp(Buffers[0])) :
           upBufferDest->ReceiveUp(Buffers[0]));*/
      //bool chkRcv = (downstream ? downBufferDest->ReceiveDown(Buffers[0]) : upBufferDest->ReceiveUp(Buffers[0]));
      if(chkRcv) {
#ifdef DEBUG_FLOW_CONTROL
        CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (downBufferDest);
        if (xbar && xbar->cubeID == 1) {
          cout << "Cube#" << xbar->cubeID << " is returning tokens back to upstream for packet " << downBuffers[0]->TAG
            << endl; 
        }
#endif
        localLinkMaster->ReturnToken(downBuffers[0]);
        /*DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")
            <<"SENDING packet to "<<(downstream ? "crossbar switch (CS)" : "HMC controller (HC)"));*/
        DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) "
            <<"SENDING packet to "<<"crossbar switch (CS)");
        downBuffers.erase(downBuffers.begin(), downBuffers.begin()+downBuffers[0]->LNG);
      }
      else {
        //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<"(downstream ? "Down) Crossbar switch" : "Up)   HMC controller")<<" buffer FULL");	
      }
    }

    Step();
    for(int i=0; i<linkRxTx.size(); i++) {
      if(linkRxTx[i] != NULL) {
        linkRxTx[i]->bufPopDelay = (linkRxTx[i]->bufPopDelay>0) ? linkRxTx[i]->bufPopDelay-1 : 0;
      }
    }
  }

  //
  //Check error in receiving packet by CRC and SEQ
  //
  bool LinkSlave::CheckNoError(Packet *chkPacket)
  {
    unsigned tempSEQ = (slaveSEQ<7) ? slaveSEQ++ : 0;
    if(CRC_CHECK) {
      unsigned tempCRC = chkPacket->GetCRC();
      if(chkPacket->CRC == tempCRC) {
        if(chkPacket->SEQ == tempSEQ) {
          DEBUG(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
              <<"CRC(0x"<<hex<<setw(9)<<setfill('0')<<tempCRC<<dec<<"), SEQ("<<tempSEQ<<") are checked (NO error)");
          return true;
        }
        else {
          DE_CR(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
              <<"= Error abort mode =  (packet SEQ : "<<chkPacket->SEQ<<" / Slave SEQ : "<<tempSEQ<<")");
          return false;
        }
      }
      else {
        DE_CR(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
            <<"= Error abort mode =  (packet CRC : 0x"<<hex<<setw(9)<<setfill('0')<<chkPacket->CRC<<dec
            <<" / Slave CRC : 0x"<<hex<<setw(9)<<setfill('0')<<tempCRC<<dec<<")");
        return false;
      }
    }
    else {
      if(chkPacket->SEQ == tempSEQ) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
            <<"SEQ("<<tempSEQ<<") is checked (NO error)");
        return true;
      }
      else {
        DE_CR("    (LS_"<<linkSlaveID<<ALI(7)<<")"<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
            <<"= Error abort mode =  (packet SEQ : "<<chkPacket->SEQ<<" / Slave SEQ : "<<tempSEQ<<")");
        return false;
      }
    }
    return false;
  }

  //
  //Print current state in state log file
  //
  void LinkSlave::PrintState()
  {
    //Print up buffer
    if(upBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      //STATEN((downstream ? "Down " : " Up  "));
      STATEN("UBUF ");
      int realInd = 0;
      for(int i=0; i<upBufferMax; i++) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(i < upBuffers.size()) {
          if(upBuffers[i] != NULL)	realInd = i;
          STATEN(*upBuffers[realInd]);
        }
        else if(i == upBufferMax-1) {
          STATEN("[ - ]");
        }
        else {
          STATEN("[ - ]...");
          break;
        }
      }
      STATEN(endl);
    }

    //Print down buffer
    if(downBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      //STATEN((downstream ? "Down " : " Up  "));
      STATEN("DBUF ");
      int realInd = 0;
      for(int i=0; i<downBufferMax; i++) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(i < downBuffers.size()) {
          if(downBuffers[i] != NULL)	realInd = i;
          STATEN(*downBuffers[realInd]);
        }
        else if(i == downBufferMax-1) {
          STATEN("[ - ]");
        }
        else {
          STATEN("[ - ]...");
          break;
        }
      }
      STATEN(endl);
    }

    //Print linkRx buffer
    if(linkRxTx.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN("LKRX ");
      int realInd = 0;
      for(int i=0; i<linkRxTx.size(); i++) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(linkRxTx[i] != NULL)	realInd = i;
        STATEN(*linkRxTx[realInd]);
      }
      STATEN(endl);
    }
  }

} //namespace CasHMC
