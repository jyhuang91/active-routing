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

#include "LinkMaster.h"

// Jiayi, debug purpose, 03/18/17
#include "CrossbarSwitch.h"
#include "HMCController.h"
#include "InputBuffer.h"

namespace CasHMC
{

  LinkMaster::LinkMaster(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_LINK_BUF, MAX_LINK_BUF),
    localLinkMasterID(id)
  {
    downstream = down;
    is_link = true;
    classID << localLinkMasterID;
    header = "   (LM_";
    header += downstream ? "D" : "U";
    header += classID.str() + ")";

    //tokenCount = MAX_LINK_BUF;
    upTokenCount = MAX_LINK_BUF;
    downTokenCount = MAX_LINK_BUF;
    lastestRRP = 0;
    linkP = NULL;
    localLinkSlave = NULL;
    masterSEQ = 0;
    countdownCRC = 0;
    startCRC = false;
    retryStartPacket = NULL;
    readyStartRetry = false;
    startRetryTimer = false;
    retryTimer = 0;
    retryAttempts = 1;
    retBufReadP = 0;
    retBufWriteP = 0;

    retryBuffers = vector<Packet *>(MAX_RETRY_BUF, NULL);
  }

  LinkMaster::LinkMaster(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down, string headerPrefix):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_LINK_BUF, MAX_LINK_BUF),
    localLinkMasterID(id)
  {
    downstream = down;
    is_link = true;
    classID << localLinkMasterID;
    header = "   (" + headerPrefix;
    header += "_LM_";
    header += downstream ? "D" : "U";
    header += classID.str() + ")";

    //tokenCount = MAX_LINK_BUF;
    upTokenCount = MAX_LINK_BUF;
    downTokenCount = MAX_LINK_BUF;
    lastestRRP = 0;
    linkP = NULL;
    localLinkSlave = NULL;
    masterSEQ = 0;
    countdownCRC = 0;
    startCRC = false;
    retryStartPacket = NULL;
    readyStartRetry = false;
    startRetryTimer = false;
    retryTimer = 0;
    retryAttempts = 1;
    retBufReadP = 0;
    retBufWriteP = 0;

    retryBuffers = vector<Packet *>(MAX_RETRY_BUF, NULL);
  }

  LinkMaster::~LinkMaster()
  {
    backupBuffers.clear(); 
    retryBuffers.clear(); 
  }

  //
  //Callback receiving packet result
  //
  void LinkMaster::CallbackReceiveDown(Packet *downEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
        }*/
  }
  void LinkMaster::CallbackReceiveUp(Packet *upEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
        }*/
  }

  //
  //Retry read pointer is updated as extracted RRP that means successful transmission of the associated packet
  //
  void LinkMaster::UpdateRetryPointer(Packet *packet)
  {
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
    CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (localLinkSlave->downBufferDest);
    LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
    CrossbarSwitch *from_xbar = NULL;
    if (lsp) from_xbar = dynamic_cast<CrossbarSwitch *> (lsp->downBufferDest);
    if (xbar && xbar->cubeID == 0 && from_xbar && from_xbar->cubeID == 1) {
      cout << CYCLE() << "cube#" << xbar->cubeID << " update RRP from cube#" << from_xbar->cubeID
        << ", before: " << retBufReadP;
    }
#endif
    if(retBufReadP != packet->RRP) {
      do {
        if(retryBuffers[retBufReadP] != NULL) {
          delete retryBuffers[retBufReadP];
        }
        retryBuffers[retBufReadP] = NULL;
        retBufReadP++;
        retBufReadP = (retBufReadP < MAX_RETRY_BUF) ? retBufReadP : retBufReadP - MAX_RETRY_BUF;
      } while(retBufReadP != packet->RRP);
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer READ POINTER is updated ("<<retBufReadP<<")");
    }
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
    if (xbar && xbar->cubeID == 0 && from_xbar && from_xbar->cubeID == 1) {
      cout << ", after: " << retBufReadP << ", wrP: " << retBufWriteP << endl;
    }
#endif
  }

  //
  //Extracted FRP is transmitted to the opposite side of the link
  //
  void LinkMaster::ReturnRetryPointer(Packet *packet)
  {
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
    CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (localLinkSlave->downBufferDest);
    LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
    CrossbarSwitch *to_xbar = NULL;
    if (lsp) to_xbar = dynamic_cast<CrossbarSwitch *> (lsp->downBufferDest);
#endif
    lastestRRP = packet->FRP;
    if(upBuffers.size() != 0) {
      upBuffers[0]->RRP = lastestRRP;
      DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<(downstream ? "Down) " : "Up)   ")<<"RRP ("<<lastestRRP<<") is embedded in this packet");
#ifdef DEBUG_FLOW_CONTROL
      if (xbar && xbar->cubeID == 0 && to_xbar && to_xbar->cubeID == 1) {
        cout << CYCLE() << "cube#" << xbar->cubeID << " is returning retry pointer to cube#" << to_xbar->cubeID
          << " (" << ALI(18) << header << ALI(15) << *upBuffers[0] << "UP) RRP (" << lastestRRP
          << ") is embedded in this packet)" << endl;
      }
#endif
    }
    else if(downBuffers.size() != 0) {
      downBuffers[0]->RRP = lastestRRP;
      DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<(downstream ? "Down) " : "Up)   ")<<"RRP ("<<lastestRRP<<") is embedded in this packet");
#ifdef DEBUG_FLOW_CONTROL
      if (xbar && xbar->cubeID == 0 && to_xbar && to_xbar->cubeID == 1) {
        cout << CYCLE() << "cube#" << xbar->cubeID << " is returning retry pointer to cube#" << to_xbar->cubeID
          << " (" << ALI(18) << header << ALI(15) << *downBuffers[0] << "UP) RRP (" << lastestRRP
          << ") is embedded in this packet)" << endl;
      }
#endif
    }
    else {
      //packet, cmd, addr, cub, lng, *lat
      Packet *packetPRET = new Packet(FLOW, PRET, 0, 0, 1, NULL, 0, 0);
      packetPRET->TAG = packet->TAG;
      packetPRET->RRP = lastestRRP;
      if(downstream)	packetPRET->bufPopDelay = 0;
      linkRxTx.push_back(packetPRET);
      DEBUG(ALI(18)<<header<<ALI(15)<<*packetPRET<<(downstream ? "Down) " : "Up)   ")<<"MAKING PRET packet to be embedded RRP ("<<lastestRRP<<")");
#ifdef DEBUG_FLOW_CONTROL
      if (xbar && xbar->cubeID == 0 && to_xbar && to_xbar->cubeID == 1) {
        cout << CYCLE() << "cube#" << xbar->cubeID << " is returning retry pointer to cube#" << to_xbar->cubeID
          << " (" << ALI(18) << header << ALI(15) << *packetPRET << "UP) MAKING PRET packet to be embedded RRP ("
          << lastestRRP << ")" << endl;
      }
#endif
    }
  }

  //
  //Extracted RTC is added to the current value of the token count register
  //
  void LinkMaster::UpdateToken(Packet *packet)
  {
    if(packet->DRTC != 0) {
      downTokenCount += packet->DRTC;
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"Extracted DRTC : "<<packet->DRTC<<"  TOKEN COUNT register : "<<downTokenCount);
      if(downTokenCount > MAX_LINK_BUF) {
        ERROR(header<<"  == Error - DOWN TOKEN COUNT register is over the maximum  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
    }
    if (packet->URTC != 0) {
      upTokenCount += packet->URTC;
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"Extracted URTC : "<<packet->URTC<<"  TOKEN COUNT register : "<<upTokenCount);
      if(upTokenCount > MAX_LINK_BUF) {
        ERROR(header<<"  == Error - UP TOKEN COUNT register is over the maximum  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
    }
  }	

  //
  //Token count is returned in the return token count (RTC) field of the next possible packet traveling
  //
  void LinkMaster::ReturnToken(Packet *packet)
  {
    int i, j, extRTC = packet->LNG;
    int extDRTC = 0, extURTC = 0;
    if (packet->packetType == REQUEST) {
      extDRTC = extRTC;
    } else {
      assert(packet->packetType == RESPONSE);
      extURTC = extRTC;
    }
    bool findUpPacket = false;
    bool findDownPacket = false;
    // first find packets upbuffers (response has higher priority, flow packets are sent through upbuffers)
    for (i = 0; i < upBuffers.size(); i++) {
      if (upBuffers[i] != NULL && upBuffers[i]->CMD != PRET && upBuffers[i]->CMD != IRTRY) {
        findUpPacket = true;
      }
      break;
    }
    // second find packets downbuffers
    for (j = 0; j < downBuffers.size() && !findUpPacket; j++) {
      if (downBuffers[j] != NULL && downBuffers[j]->CMD != PRET && downBuffers[j]->CMD != IRTRY) {
        findDownPacket = true;
      }
      break;
    }
    if (findUpPacket) {
      if (extDRTC) {  // REQUEST
        if (upBuffers[i]->DRTC == 0) {
          DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<(downstream ? "Down) " : "Up)  ")<<"DRTC ("<<extDRTC<<") is embedded in tihs packet (from "<<*packet<<")");
        }
        else {
          DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<(downstream ? "Down) " : "Up)   ")<<"DRTC ("<<upBuffers[i]->DRTC<<" + "<<extDRTC<<") is embedded in this packet (from "<<*packet<<")");
        }
        upBuffers[i]->DRTC += extDRTC;
      }
      else {
        assert(extURTC); // RESPONSE
        if (upBuffers[i]->URTC == 0) {
          DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<(downstream ? "Down) " : "Up)  ")<<"URTC ("<<extURTC<<") is embedded in tihs packet (from "<<*packet<<")");
        }
        else {
          DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<(downstream ? "Down) " : "Up)   ")<<"URTC ("<<upBuffers[i]->URTC<<" + "<<extURTC<<") is embedded in this packet (from "<<*packet<<")");
        }
        upBuffers[i]->URTC += extURTC;
      }
    }
    else if (findDownPacket) {
      if (extDRTC) {  // REQUEST
        if (downBuffers[j]->DRTC == 0) {
          DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[j]<<(downstream ? "Down) " : "Up)  ")<<"DRTC ("<<extDRTC<<") is embedded in tihs packet (from "<<*packet<<")");
        }
        else {
          DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[j]<<(downstream ? "Down) " : "Up)   ")<<"DRTC ("<<downBuffers[j]->DRTC<<" + "<<extDRTC<<") is embedded in this packet (from "<<*packet<<")");
        }
        downBuffers[j]->DRTC += extDRTC;
      }
      else {
        assert(extURTC); // RESPONSE
        if (downBuffers[j]->URTC == 0) {
          DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[j]<<(downstream ? "Down) " : "Up)  ")<<"URTC ("<<extURTC<<") is embedded in tihs packet (from "<<*packet<<")");
        }
        else {
          DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[j]<<(downstream ? "Down) " : "Up)   ")<<"URTC ("<<downBuffers[j]->URTC<<" + "<<extURTC<<") is embedded in this packet (from "<<*packet<<")");
        }
        downBuffers[j]->URTC += extURTC;
      }
    }
    else {
      // packet, cmd, addr, cub, lng, *lat
      Packet *packetTRET = new Packet(FLOW, TRET, 0, 0, 1, NULL, 0, 0);
      packetTRET->TAG = packet->TAG;
      packetTRET->URTC = extURTC;
      packetTRET->DRTC = extDRTC;
      if (downstream) packetTRET->bufPopDelay = 0;

      if (currentState == LINK_RETRY) {
        backupBuffers.push_back(packetTRET);
      }
      else {
        if (startCRC == true) {
          upBuffers.insert(upBuffers.begin()+upBuffers[0]->LNG, packetTRET);  // flow packet through upbuffers
        }
        else {
          upBuffers.insert(upBuffers.begin(), packetTRET);
        }
      }
      DEBUG(ALI(18)<<header<<ALI(15)<<*packetTRET<<(downstream ? "Down) " : "Up)    ")<<"MAKEING TRET packet to be embedded RTC ("<<extRTC<<")");
    }
  }
  /*void LinkMaster::ReturnToken(Packet *packet)
  {
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
    CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (localLinkSlave->downBufferDest);
    LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
    CrossbarSwitch *to_xbar = NULL;
    if (lsp) to_xbar = dynamic_cast<CrossbarSwitch *> (lsp->downBufferDest);
#endif
    int i, extRTC = packet->LNG;
    bool findPacket = false;
    for(i=0; i<Buffers.size(); i++) {
      if(Buffers[i] != NULL && Buffers[i]->CMD != PRET && Buffers[i]->CMD != IRTRY) {
        if(Buffers[i]->LNG <= tokenCount) {
          findPacket = true;
        }
        break;
      }
    }
    if(findPacket) {
      if(Buffers[i]->RTC == 0) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[i]<<(downstream ? "Down) " : "Up)   ")<<"RTC ("<<extRTC<<") is embedded in this packet (from "<<*packet<<")");
      }
      else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[i]<<(downstream ? "Down) " : "Up)   ")<<"RTC ("<<Buffers[i]->RTC<<" + "<<extRTC<<") is embedded in this packet (from "<<*packet<<")");
      }
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
      if (xbar && xbar->cubeID == 1 && to_xbar && to_xbar->cubeID == 0) {
        cout << CYCLE() << "cube#" << xbar->cubeID << " is preparing tokens for cube#" << to_xbar->cubeID
          << " ... before: " << Buffers[i]->RTC;
      }
#endif
      Buffers[i]->RTC += extRTC;
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
      if (xbar && xbar->cubeID == 1 && to_xbar && to_xbar->cubeID == 0) {
        cout << ", after: " << Buffers[i]->RTC << ", using buffer#" << i << endl;
      }
#endif
    }
    else {
      //packet, cmd, addr, cub, lng, *lat
      Packet *packetTRET = new Packet(FLOW, TRET, 0, 0, 1, NULL, 0, 0);
      packetTRET->TAG = packet->TAG;
      packetTRET->RTC = extRTC;
      if(downstream)	packetTRET->bufPopDelay = 0;

      if(currentState == LINK_RETRY) {
        backupBuffers.push_back(packetTRET);
      }
      else {
        if(startCRC == true) {
          Buffers.insert(Buffers.begin()+Buffers[0]->LNG, packetTRET);
        }
        else {
          Buffers.insert(Buffers.begin(), packetTRET);
        }
      }
#ifdef DEBUG_FLOW_CONTROL // Jiayi, 03/18/17
      if (xbar && xbar->cubeID == 1 && to_xbar && to_xbar->cubeID == 0) {
        cout << CYCLE() << "cube#" << xbar->cubeID << " is preparing TRET tokens(" << extRTC << ") to cube#"
          << to_xbar->cubeID << endl;
      }
#endif
      DEBUG(ALI(18)<<header<<ALI(15)<<*packetTRET<<(downstream ? "Down) " : "Up)   ")<<"MAKING TRET packet to be embedded RTC ("<<extRTC<<")");
    }
  }*/

  //
  //When local link slave detects an error, StartRetry sequence is initiated
  // (If link master is in LinkRetry state, finish the LinkRetry state and allow all packets to be retransmitted 
  //  before sending a stream of IRTRY packets with the StartRetry flag set.)
  //
  void LinkMaster::StartRetry(Packet *packet)
  {
    startRetryTimer = true;
    retryTimer = 0;
    for(int i=0; i<NUM_OF_IRTRY; i++) {
      if(currentState == LINK_RETRY) {
        readyStartRetry = true;
      }
      else {
        //packet, cmd, addr, cub, lng, *lat
        Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL, 0, 0);
        packetIRTRY->TAG = packet->TAG;
        packetIRTRY->RRP = lastestRRP;
        packetIRTRY->FRP = 1;		//StartRetry flag is set with FRP[0] = 1
        if(downstream)	packetIRTRY->bufPopDelay = 0;
        linkRxTx.insert(linkRxTx.begin(), packetIRTRY);
        currentState = START_RETRY;
        header.erase(header.find(")"));
        header += ") ST_RT";
      }
    }
    if(currentState == LINK_RETRY) {
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"START RETRY sequence is delayed until LINK RETRY");
    }
    else {
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"START RETRY sequence is initiated");
    }
  }

  //
  //It is an indication that the link slave on the other end of the link detected an error
  // and send the packets currently being saved for retransmission.
  //
  void LinkMaster::LinkRetry(Packet *packet)
  {
    //linkRxTx and token count reset (packets in linkRxTx are already saved in retry buffer)
    startCRC = false;
    for(int i=0; i<NUM_OF_IRTRY; i++) {
      //packet, cmd, addr, cub, lng, *lat
      Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL, 0, 0);
      packetIRTRY->TAG = packet->TAG;
      packetIRTRY->RRP = lastestRRP;
      packetIRTRY->FRP = 2;		//ClearErrorAbort flag is set with FRP[1] = 1
      if(downstream)	packetIRTRY->bufPopDelay = 0;
      linkRxTx.push_back(packetIRTRY);
    }

    //Initialize sequence number and backup buffer packets
    masterSEQ = 0;
    /*for(int i=0; i<Buffers.size(); i++) {
      backupBuffers.push_back(Buffers[i]);
    }
    Buffers.clear();*/
    for (int i = 0; i < upBuffers.size(); i++) {
      backupBuffers.push_back(upBuffers[i]);
    }
    upBuffers.clear();
    for (int i = 0; i < downBuffers.size(); i++) {
      backupBuffers.push_back(downBuffers[i]);
    }
    downBuffers.clear();

    //Transmits the packets currently being saved for retransmission
    //unsigned retryToken = 0;
    unsigned retryUpToken = 0;
    unsigned retryDownToken = 0;
    unsigned tempReadP = retBufReadP;
    if(retryBuffers[retBufReadP] == NULL) {
      ERROR(header<<"  == Error - The first retry packet is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }
    bool is_up_pkt = false;
    if(retBufReadP != retBufWriteP) {
      do {
        Packet *retryPacket = NULL;
        if(retryBuffers[tempReadP] != NULL) {
          retryPacket = new Packet(*retryBuffers[tempReadP]);
          if(downstream)	retryPacket->bufPopDelay = 0;
          else			retryPacket->bufPopDelay = 1;
          if(retryPacket->packetType != FLOW) {
            //retryToken += retryPacket->LNG;
            if (retryPacket->packetType == REQUEST) {
              is_up_pkt = false;
              retryDownToken += retryPacket->LNG;
            } else {
              is_up_pkt = true;
              retryUpToken += retryPacket->LNG;
            }
          }
          else {
            is_up_pkt = true;
          }
          delete retryBuffers[tempReadP];
          retryBuffers[tempReadP] = NULL;
        }
        else {
          retryPacket = NULL;
        }
        //Buffers.push_back(retryPacket);
        if (is_up_pkt) upBuffers.push_back(retryPacket);
        else           downBuffers.push_back(retryPacket);
        tempReadP++;
        tempReadP = (tempReadP < MAX_RETRY_BUF) ? tempReadP : tempReadP - MAX_RETRY_BUF;
      } while(tempReadP != retBufWriteP);
      //tokenCount += retryToken;
      upTokenCount += retryUpToken;
      downTokenCount += retryDownToken;
      retBufWriteP = retBufReadP;
      currentState = LINK_RETRY;
      header.erase(header.find(")"));
      header += ") LK_RT";
    }
    else {
      ERROR(header<<"  == Error - Retry buffer has no packets to be transmitted for LINK RETRY sequence  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }
    DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"LINK RETRY sequence is initiated");
  }

  //
  //Finish retry state. It is asserted when local link slave receive IRTRY2 packet.
  //
  void LinkMaster::FinishRetry()
  {
    DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"RETRY sequence is finished");
    currentState = NORMAL;
    header.erase(header.find(")"));
    header += ")";
    delete retryStartPacket;
    retryStartPacket = NULL;
    retryAttempts = 1;
    startRetryTimer = false;
    uint64_t retryTime;
    if(downstream) {
      retryTime = retryTimer;
    }
    else {
      retryTime = ceil(retryTimer * (double)tCK/CPU_CLK_PERIOD);
    }
    linkP->statis->errorRetryLat.push_back(retryTime);
    retryTimer = 0;
  }

  //
  //Update the state of link master
  //
  void LinkMaster::Update()
  {
    // upBuffer first, higher priority for RESPONSE and FLOW
    if (upBuffers.size() > 0) {
      //Make sure that upBuffers[0] is not virtual tail packet.
      if (upBuffers[0] == NULL) {
        ERROR(header<<"  == Error - upBuffers[0] is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      else if(upBuffers[0]->bufPopDelay == 0) {

        //Token count register represents the available space in link slave input buffer
        if (linkRxTx.size() == 0 && (upBuffers[0]->packetType == FLOW || upTokenCount >= upBuffers[0]->LNG)) {
          int tempWriteP = retBufWriteP + upBuffers[0]->LNG;
          if(retBufWriteP	>= retBufReadP) {
            if(tempWriteP - (int)retBufReadP < MAX_RETRY_BUF) {
              CRCCountdown(tempWriteP, upBuffers[0]);
            }
            else {
              //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
            }
          }
          else {
            if((int)retBufReadP - tempWriteP > 0) {
              CRCCountdown(tempWriteP, upBuffers[0]);
            }
            else {
              //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
            }
          }
        }
        else {
          //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"packet length ("<<Buffers[0]->LNG<<") is BIGGER than token count register ("<<tokenCount<<")");
        }
      }
    }

    // process downBuffer for REQUEST
    if(downBuffers.size() > 0) {
      //Make sure that buffer[0] is not virtual tail packet.
      if(downBuffers[0] == NULL) {
        ERROR(header<<"  == Error - downBuffers[0] is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      else if(downBuffers[0]->bufPopDelay == 0) {

        //Token count register represents the available space in link slave input buffer
        if (linkRxTx.size() == 0 && (downBuffers[0]->packetType == FLOW || downTokenCount >= downBuffers[0]->LNG)) {
          int tempWriteP = retBufWriteP + downBuffers[0]->LNG;
          if(retBufWriteP	>= retBufReadP) {
            if(tempWriteP - (int)retBufReadP < MAX_RETRY_BUF) {
              CRCCountdown(tempWriteP, downBuffers[0]);
            }
            else {
              //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
            }
          }
          else {
            if((int)retBufReadP - tempWriteP > 0) {
              CRCCountdown(tempWriteP, downBuffers[0]);
            }
            else {
              //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
            }
          }
        }
        else {
          //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"packet length ("<<Buffers[0]->LNG<<") is BIGGER than token count register ("<<tokenCount<<")");
        }
      }
    }

    //Finish LinkRetry state, and then backup packets return to buffers 
    if(currentState == LINK_RETRY && upBuffers.empty() && downBuffers.empty() && linkRxTx.empty()) {
      //If StartRetry state is waiting, start StartRetry state
      if(readyStartRetry == true) {
        readyStartRetry = false;
        for(int i=0; i<NUM_OF_IRTRY; i++) {
          //packet, cmd, addr, cub, lng, *lat
          Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL, 0, 0);
          packetIRTRY->TAG = retryStartPacket->TAG;
          packetIRTRY->RRP = lastestRRP;
          packetIRTRY->FRP = 1;		//StartRetry flag is set with FRP[0] = 1
          if(downstream)	packetIRTRY->bufPopDelay = 0;
          linkRxTx.push_back(packetIRTRY);
          currentState = START_RETRY;
          header.erase(header.find(")"));
          header += ") ST_RT";
        }
      }
      else {
        currentState = NORMAL;
        header.erase(header.find(")"));
        header += ")";
      }
      //Restore backup packets to Buffers
      bool is_up_pkt = false;
      for(int i=0; i<backupBuffers.size(); i++) {
        //Buffers.push_back(backupBuffers[i]);
        if (backupBuffers[i] != NULL) {
          if (backupBuffers[i]->packetType == REQUEST) is_up_pkt = false;
          else is_up_pkt = true;
        }
        if (is_up_pkt) upBuffers.push_back(backupBuffers[i]);
        else downBuffers.push_back(backupBuffers[i]);
      }
      backupBuffers.clear();
    }

    if(startRetryTimer)		retryTimer++;
    //Retry timer should be set to at least 3 times the retry buffer full period
    if(retryTimer > MAX_RETRY_BUF*4) {
      DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"Retry timer TIME-OUT  (retry attempt : "<<retryAttempts<<")");
      if(retryAttempts == RETRY_ATTEMPT_LIMIT) {
        FinishRetry();
      }
      else {
        retryAttempts++;
        StartRetry(retryStartPacket);
      }
    }

    Step();
  }
  /*void LinkMaster::Update()
  {
    if(Buffers.size() > 0) {
      //Make sure that buffer[0] is not virtual tail packet.
      if(Buffers[0] == NULL) {
        ERROR(header<<"  == Error - Buffers[0] is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      else if(Buffers[0]->bufPopDelay == 0) {
#ifdef DEBUG_FLOW_CONTROL        // Jiayi, 03/18/17
        CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (localLinkSlave->downBufferDest);
        if (xbar && xbar->cubeID == 0 && (Buffers[0]->CMD == ACT_ADD || Buffers[0]->CMD == ACT_MULT)) {
          cout << "[cycle: " << currentClockCycle << "] start to send packet#" << Buffers[0]->TAG
            << " from LM in cube#" << xbar->cubeID << ", linkRxTx.size: " << linkRxTx.size()
            << ", tokenCount: " << tokenCount << ", LNG: " << Buffers[0]->LNG << endl;
        }
        LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
        CrossbarSwitch *to_xbar = NULL;
        if (lsp) to_xbar = dynamic_cast<CrossbarSwitch *> (lsp->downBufferDest);
        if (xbar && xbar->cubeID == 1 && to_xbar && to_xbar->cubeID == 0 && Buffers[0]->packetType == FLOW) {
          if (linkRxTx.size() == 0) {
            cout << CYCLE() << "link idle, free to go from cube#" << xbar->cubeID << " to cube#" << to_xbar->cubeID
              << ", embedded tokens: " << Buffers[0]->RTC << endl;
          } else {
            cout << CYCLE() << "link busy from cube#" << xbar->cubeID << " to cube#" << to_xbar->cubeID
              << " ... LinkRxTx.size(): " << linkRxTx.size() << endl;
          }
        }
#endif
        
        //Token count register represents the available space in link slave input buffer
        if(linkRxTx.size() == 0 && !(Buffers[0]->packetType != FLOW && tokenCount < Buffers[0]->LNG)) {
          int tempWriteP = retBufWriteP + Buffers[0]->LNG;
          if(retBufWriteP	>= retBufReadP) {
            if(tempWriteP - (int)retBufReadP < MAX_RETRY_BUF) {
#ifdef DEBUG_GATHER
              if (Buffers[0]->CMD == ACT_GET && !startCRC) {    // Jiayi, 02/12
                InputBuffer *ib = dynamic_cast<InputBuffer *> (localLinkSlave->downBufferDest);
                LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
                InputBuffer *to_ib = NULL;
                if (lsp) to_ib = dynamic_cast<InputBuffer *> (lsp->downBufferDest);
                cout << CYCLE() << "(1) ACT_GET at ";
                if (ib && to_ib) {
                  cout << "cube#" << ((CrossbarSwitch *) ib->xbar)->cubeID << "'s link master to cube#" <<
                    ((CrossbarSwitch *) to_ib->xbar)->cubeID << endl;
                } else if (ib && !to_ib) {
                  cout << "cube#" << ((CrossbarSwitch *) ib->xbar)->cubeID << "'s link master to hmc_ctrl" << endl;
                } else {
                  assert(!ib && to_ib);
                  cout << "hmc_ctrl" << "'s link master to cube#" << ((CrossbarSwitch *) to_ib->xbar)->cubeID << endl;
                }
              }
#endif
              CRCCountdown(tempWriteP, Buffers[0]);
            }
            else {
#ifdef DEBUG_FLOW_CONTROL
              if (xbar && xbar->cubeID == 1 && to_xbar && to_xbar->cubeID == 0 && Buffers[0]->packetType == FLOW) {
                cout << CYCLE() << "cube#" << xbar->cubeID << " to cube#" << to_xbar->cubeID
                  << " (1) Retry buffer is nearly-full condition, packet length: " << Buffers[0]->LNG
                  << ", ReadP-WriteP: " << retBufReadP << "-" << retBufWriteP << endl;
              }
#endif
              //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
            }
          }
          else {
            if((int)retBufReadP - tempWriteP > 0) {
#ifdef DEBUG_GATHER
              if (Buffers[0]->CMD == ACT_GET && !startCRC) {    // Jiayi, 02/12
                InputBuffer *ib = dynamic_cast<InputBuffer *> (localLinkSlave->downBufferDest);
                LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
                InputBuffer *to_ib = NULL;
                if (lsp) to_ib = dynamic_cast<InputBuffer *> (lsp->downBufferDest);
                cout << CYCLE() << "(2) ACT_GET at ";
                if (ib && to_ib) {
                  cout << "cube#" << ((CrossbarSwitch *) ib->xbar)->cubeID << "'s link master to cube#" <<
                    ((CrossbarSwitch *) to_ib->xbar)->cubeID << endl;
                } else if (ib && !to_ib) {
                  cout << "cube#" << ((CrossbarSwitch *) ib->xbar)->cubeID << "'s link master to hmc_ctrl" << endl;
                } else {
                  assert(!ib && to_ib);
                  cout << "hmc_ctrl" << "'s link master to cube#" << ((CrossbarSwitch *) to_ib->xbar)->cubeID << endl;
                }
              }
#endif
              CRCCountdown(tempWriteP, Buffers[0]);
            }
            else {
#ifdef DEBUG_FLOW_CONTROL
              if (xbar && xbar->cubeID == 1 && to_xbar && to_xbar->cubeID == 0 && Buffers[0]->packetType == FLOW) {
                cout << CYCLE() << "cube#" << xbar->cubeID << " cube#" << to_xbar->cubeID
                 << " (2) Retry buffer is nearly-full condition, packet length: " << Buffers[0]->LNG
                  << ", ReadP-WriteP: " << retBufReadP << "-" << retBufWriteP << endl;
              }
#endif
              //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
            }
          }
        }
        else {
          //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"packet length ("<<Buffers[0]->LNG<<") is BIGGER than token count register ("<<tokenCount<<")");
        }
      }
    }

    //Finish LinkRetry state, and then backup packets return to buffers 
    if(currentState == LINK_RETRY && Buffers.empty() && linkRxTx.empty()) {
      //If StartRetry state is waiting, start StartRetry state
      if(readyStartRetry == true) {
        readyStartRetry = false;
        for(int i=0; i<NUM_OF_IRTRY; i++) {
          //packet, cmd, addr, cub, lng, *lat
          Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL, 0, 0);
          packetIRTRY->TAG = retryStartPacket->TAG;
          packetIRTRY->RRP = lastestRRP;
          packetIRTRY->FRP = 1;		//StartRetry flag is set with FRP[0] = 1
          if(downstream)	packetIRTRY->bufPopDelay = 0;
          linkRxTx.push_back(packetIRTRY);
          currentState = START_RETRY;
          header.erase(header.find(")"));
          header += ") ST_RT";
        }
      }
      else {
        currentState = NORMAL;
        header.erase(header.find(")"));
        header += ")";
      }
      //Restore backup packets to Buffers
      for(int i=0; i<backupBuffers.size(); i++) {
        Buffers.push_back(backupBuffers[i]);
      }
      backupBuffers.clear();
    }

    if(startRetryTimer)		retryTimer++;
    //Retry timer should be set to at least 3 times the retry buffer full period
    if(retryTimer > MAX_RETRY_BUF*4) {
      DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"Retry timer TIME-OUT  (retry attempt : "<<retryAttempts<<")");
      if(retryAttempts == RETRY_ATTEMPT_LIMIT) {
        FinishRetry();
      }
      else {
        retryAttempts++;
        StartRetry(retryStartPacket);
      }
    }

    Step();
  }*/

  //
  //CRC countdown to update packet filed
  //
  void LinkMaster::CRCCountdown(int writeP, Packet *packet)
  {
    if(CRC_CHECK && !startCRC) {
      startCRC = true;
      countdownCRC = ceil(CRC_CAL_CYCLE * packet->LNG);
    }

    if(CRC_CHECK && countdownCRC > 0) {
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
          <<"WAITING CRC calculation ("<<countdownCRC<<"/"<<ceil(CRC_CAL_CYCLE*packet->LNG)<<")");
    }
    else {
      UpdateField(writeP, packet);
    }
    countdownCRC = (countdownCRC>0) ? countdownCRC-1 : 0;
  }

  //
  //packet in link master downstream buffer is embedded SEQ, FRP, CRC and stored to retry buffer.
  // (CRC calculation delay is hidden, because the beginning of the packet may have been forwarded 
  //  before the CRC was performed at the tail to minimize latency.)
  //
  void LinkMaster::UpdateField(int nextWriteP, Packet *packet)
  {
    //Sequence number updated
    packet->SEQ = (masterSEQ<7) ? masterSEQ++ : 0;
    //Update retry buffer pointer field
    packet->RRP = lastestRRP;
    packet->FRP = (nextWriteP < MAX_RETRY_BUF) ? nextWriteP : nextWriteP - MAX_RETRY_BUF;
    //CRC field updated
    startCRC = false;
    if(CRC_CHECK)	packet->CRC = packet->GetCRC();
    //Decrement the token count register by the number of packet in the transmitted packet
    if(packet->packetType != FLOW) {
      //tokenCount -= packet->LNG;
      if (packet->packetType == REQUEST) {
        downTokenCount -= packet->LNG;
        DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
            <<"Decreased DTOKEN : "<<packet->LNG<<" (remaining : "<<downTokenCount<<"/"<<MAX_LINK_BUF<<")");
      }
      else {
        upTokenCount -= packet->LNG;
        DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
            <<"Decreased UTOKEN : "<<packet->LNG<<" (remaining : "<<upTokenCount<<"/"<<MAX_LINK_BUF<<")");
      }
    }
    //Save packet in retry buffer
    Packet *retryPacket = new Packet(*packet);
    if(retryBuffers[retBufWriteP] == NULL) {
      retryBuffers[retBufWriteP] = retryPacket;
    }
    else {
      ERROR(header<<"  == Error - retryBuffers["<<retBufWriteP<<"] is not NULL  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }
    for(int i=1; i<packet->LNG; i++) {
      retryBuffers[(retBufWriteP+i<MAX_RETRY_BUF ? retBufWriteP+i : retBufWriteP+i-MAX_RETRY_BUF)] = NULL;
    }
    DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
        <<"RETRY BUFFER["<<retBufWriteP<<"] (FRP : "<<packet->FRP<<")");
    retBufWriteP = packet->FRP;
    //Send packet to standby buffer where the packet is ready to be transmitted
    packet->bufPopDelay = 1;
    linkRxTx.push_back(packet);
#ifdef DEBUG_GATHER
    if (packet->CMD == ACT_GET) {   // Jiayi, 02/12
      InputBuffer *ib = dynamic_cast<InputBuffer *> (localLinkSlave->downBufferDest);
      LinkSlave *lsp = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
      InputBuffer *to_ib = NULL;
      if (lsp) to_ib = dynamic_cast<InputBuffer *> (lsp->downBufferDest);
      cout << CYCLE() << "push ACT_GET of flow " << hex << packet->DESTADRS << dec << " to link, ";
      if (ib && to_ib) {
        cout << "from cube#" << ((CrossbarSwitch *) ib->xbar)->cubeID << " to cube#" << ((CrossbarSwitch *) to_ib->xbar)->cubeID << endl;
      } else if (ib && !to_ib) {
        cout << "from cube#" << ((CrossbarSwitch *) ib->xbar)->cubeID << " to hmc_ctrl" << endl;
      } else {
        assert(!ib && to_ib);
        cout << "from hmc_ctrl" << " to cube#" << ((CrossbarSwitch *) to_ib->xbar)->cubeID << endl;
      }
    } else if (/*Buffers[0]->CMD == ACT_ADD || Buffers[0]->CMD == ACT_MULT*/ false) {
//      LinkSlave *linkSlaveP = dynamic_cast<LinkSlave *> (linkP->linkSlaveP);
//      assert(linkSlaveP);
//      CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (linkSlaveP->downBufferDest);
//      HMCController *hmcctrl = dynamic_cast<HMCController *> (localLinkSlave->upBufferDest);
//      if (xbar && xbar->cubeID == 0 && hmcctrl) {
//        cout << CYCLE() << (Buffers[0]->CMD == ACT_ADD ? "ACT_ADD" : "ACT_MULT")
//          << " at link master, push to link to cube#" << xbar->cubeID << endl;
//      }
    }
#endif
    //DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")
    //			<<"SENDING packet to link "<<linkMasterID<<" (LK_"<<(downstream ? "D" : "U")<<linkMasterID<<")");
    if (packet->packetType == REQUEST) {
      downBuffers.erase(downBuffers.begin(), downBuffers.begin()+packet->LNG);
    }
    else {
      upBuffers.erase(upBuffers.begin(), upBuffers.begin()+packet->LNG);
    }
  }

  //
  //Print current state in state log file
  //
  void LinkMaster::PrintState()
  {
    //Print up buffer
    if (upBuffers.size() > 0) {
      if (currentState == NORMAL) {
        STATEN(ALI(11)<<header<<"UBUF utk:"<<ALI(3)<<upTokenCount);
      }
      else {
        STATEN(ALI(17)<<header);
      }
      STATEN(" UBUF ");
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

    //Print down buffers
    if(downBuffers.size()>0) {
      if(currentState == NORMAL) {
        STATEN(ALI(11)<<header<<"DBUF dtk:"<<ALI(3)<<downTokenCount);
        //	STATEN(ALI(17)<<header);
      }
      else {
        STATEN(ALI(17)<<header);
      }
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

    //Print linkTx buffer
    if(linkRxTx.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN("LKTX ");
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

    //If retry buffer has something, print it
    if(retBufWriteP != retBufReadP) {
      //	STATEN(ALI(11)<<header<<"tk:"<<ALI(3)<<tokenCount);
      STATEN(ALI(17)<<header);
      STATEN("RTRY ");
      int retryInd = retBufReadP;
      int realInd = retBufReadP, i=0;
      while(retryInd != retBufWriteP) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(retryBuffers[retryInd] != NULL) {
          realInd = retryInd;
          STATEN("("<<retryInd<<")");
        }
        STATEN(*retryBuffers[realInd]);
        retryInd++;
        retryInd = (retryInd < MAX_RETRY_BUF) ? retryInd : retryInd - MAX_RETRY_BUF;
        i++;
      }
      STATEN(endl);
    }

    //Print backup buffer during LinkRetry state
    if(backupBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN("BACK ");
      int realInd = 0;
      for(int i=0; i<backupBuffers.size(); i++) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(backupBuffers[i] != NULL)	realInd = i;
        STATEN(*backupBuffers[realInd]);
      }
      STATEN(endl);
    }
  }


  //
  //Determines which link should be used to receive a request
  //
  int FindAvailableLink(int &link, vector<LinkMaster *> &LM)
  {
    switch(LINK_PRIORITY) {
      case ROUND_ROBIN:
        //if(++link >= NUM_LINKS)
        if (++link >= LM.size())  // Jiayi, 03/16/17
          link=0;
        return link;
        break;
      case BUFFER_AWARE:
        unsigned minBufferSize = MAX_LINK_BUF;
        unsigned minBufferLink = 0;
        //for(int l=0; l<NUM_LINKS; l++) {
        for(int l=0; l<LM.size(); l++) {
          //int bufSizeTemp = LM[l]->Buffers.size();
          int bufSizeTemp = LM[l]->upBuffers.size() + LM[l]->downBuffers.size();
          for(int i=0; i<LM[l]->linkRxTx.size(); i++) {
            if(LM[l]->linkRxTx[i] != NULL) {
              bufSizeTemp += LM[l]->linkRxTx[i]->LNG;
            }
          }
          if(bufSizeTemp < minBufferSize) {
            minBufferSize = bufSizeTemp;
            minBufferLink = l;
          }
        }
        return minBufferLink;
        break;
    }
    return -1;
  }

} //namespace CasHMC
