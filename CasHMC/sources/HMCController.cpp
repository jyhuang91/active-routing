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

#include <algorithm>

#include "HMCController.h"
#include "CrossbarSwitch.h" // print active routing tree

using namespace std;

namespace CasHMC
{

  HMCController::HMCController(ofstream &debugOut_, ofstream &stateOut_):
    DualVectorObject<Transaction, Packet>(debugOut_, stateOut_, MAX_REQ_BUF, MAX_LINK_BUF)
  {
    header = " (HC)";

    inServiceLink = -1;

    switch (SIM_TOPOLOGY) {
      case DUAL_HMC:
      case MESH:
      case DFLY:
        CPU_LINKS = 1;
        break;
      case DEFAULT:
        CPU_LINKS = 4;
        break;
      default:
        ERROR(header << "topology " << SIM_TOPOLOGY << " not defined");
        exit(0);
    }

    //Make class objects
    downLinkMasters.reserve(CPU_LINKS);
    upLinkSlaves.reserve(CPU_LINKS);
    for(int l=0; l<CPU_LINKS; l++) {
      downLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, true));
      upLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, false));
    }

    responseQ.clear();
    gatherBarrier.clear();
  }

  HMCController::HMCController(ofstream &debugOut_, ofstream &stateOut_, unsigned id):
    DualVectorObject<Transaction, Packet>(debugOut_, stateOut_, MAX_REQ_BUF, MAX_LINK_BUF),
    hcID(id)
  {
    classID << hcID;
    header = " (HC";
    header += classID.str() + ")";

    inServiceLink = -1;

    switch (SIM_TOPOLOGY) {
      case DUAL_HMC:
      case MESH:
      case DFLY:
        CPU_LINKS = 1;
        break;
      case DEFAULT:
        CPU_LINKS = 4;
        break;
      default:
        ERROR(header << "topology " << SIM_TOPOLOGY << " not defined");
        exit(0);
    }

    //Make class objects
    downLinkMasters.reserve(CPU_LINKS);
    upLinkSlaves.reserve(CPU_LINKS);
    classID.str("");
    classID << "HC" << hcID;
    for(int l=0; l<CPU_LINKS; l++) {
      downLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, true, classID.str()));
      upLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, false, classID.str()));
      downLinkMasters[l]->linkMasterID = 64 + hcID * CPU_LINKS + l;
      upLinkSlaves[l]->linkSlaveID = 64 + hcID * CPU_LINKS + l;
    }

    responseQ.clear();
    gatherBarrier.clear();
  }

  HMCController::~HMCController()
  {
    for(int l=0; l<CPU_LINKS; l++) {
      delete downLinkMasters[l];
      delete upLinkSlaves[l];
    }
    downLinkMasters.clear();
    upLinkSlaves.clear();
    responseQ.clear();  // Jiayi, 02/07
    gatherBarrier.clear();  // Jiayi, 03/21/17
  }

  //
  //Callback receiving packet result
  //
  void HMCController::CallbackReceiveDown(Transaction *downEle, bool chkReceive)
  {
    if(chkReceive) {
      downEle->trace->tranTransmitTime = currentClockCycle;
      if(downEle->transactionType == DATA_WRITE)	downEle->trace->statis->hmcTransmitSize += downEle->dataSize;
      //DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING transaction");
    }
    else {
      //DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) Transaction buffer FULL");
    }
  }

  void HMCController::CallbackReceiveUp(Packet *upEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   PUSHING packet into buffer["<<upBuffers.size()<<"/"<<MAX_REQ_BUF-1<<"]");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
        }*/
  }

  //
  //Update the state of HMC controller
  // Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
  //
  void HMCController::Update()
  {
    //Downstream buffer state
    if(bufPopDelay==0 && downBuffers.size() > 0) {
      //Check packet dependency
      int link = -1;
      for(int l=0; l<CPU_LINKS; l++) {
        for(int i=0; i<downLinkMasters[l]->downBuffers.size(); i++) {
          if(downLinkMasters[l]->downBuffers[i] != NULL) {
            unsigned maxBlockBit = _log2(ADDRESS_MAPPING);
            if((downLinkMasters[l]->downBuffers[i]->ADRS >> maxBlockBit) == (downBuffers[0]->address >> maxBlockBit)) {
              link = l;
              DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) This transaction has a DEPENDENCY with "<<*downLinkMasters[l]->downBuffers[i]);
              break;
            }
          }
        }
      }

      if(link == -1) {
        for(int l=0; l<CPU_LINKS; l++) {
          link = FindAvailableLink(inServiceLink, downLinkMasters);
          if (SIM_TOPOLOGY == DFLY || SIM_TOPOLOGY == MESH) {
            assert(CPU_LINKS == 1);
            assert(link == 0);
          }
          if(link == -1) {
            //DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) all link buffer FULL");
          }
          else if(downLinkMasters[link]->currentState != LINK_RETRY) {
            Packet *packet = ConvTranIntoPacket(downBuffers[0]);
            if (packet->CMD == ACT_GET) { // Jiayi, 03/21/17
              map<uint64_t, pair<int, int> >::iterator it = gatherBarrier.find(packet->DESTADRS);
              int nthreads = downBuffers[0]->nthreads;
              if (it != gatherBarrier.end()) {
                assert(nthreads == it->second.first);
                assert(gatherBarrier[packet->DESTADRS].second < nthreads);
                gatherBarrier[packet->DESTADRS].second++;
#ifdef DEBUG_GATHER
                cout << CYCLE() << "Active-Routing (flow: " << hex << packet->DESTADRS << dec
                  << "): receive a Gather command, nthreads: " << nthreads
                  << ", <nthreads, count>: <" << gatherBarrier[packet->DESTADRS].first
                  << ", " << gatherBarrier[packet->DESTADRS].second << ">" << endl;
#endif
              } else {
                gatherBarrier.insert(make_pair(packet->DESTADRS, make_pair(nthreads, 1)));
#ifdef DEBUG_GATHER
                cout << CYCLE() << "Active-Routing (flow: " << hex << packet->DESTADRS << dec
                  << "): receive a Gather command, nthreads: " << nthreads
                  << ", <nthreads, count>: <" << gatherBarrier[packet->DESTADRS].first
                  << ", " << gatherBarrier[packet->DESTADRS].second << ">" << endl;
#endif
              }
              if (gatherBarrier[packet->DESTADRS].second != nthreads) {
                packet->ReductGlobalTAG();
                delete packet;
                delete downBuffers[0]->trace;
                delete downBuffers[0];
                downBuffers.erase(downBuffers.begin());
                //--l;
                //continue;
                break;
              }
            }
            if(downLinkMasters[link]->ReceiveDown(packet)) {
              if (packet->CMD == ACT_GET) {
#ifdef DEBUG_GATHER
                cout << CYCLE() << "Active-Routing (flow:" << hex << packet->DESTADRS << dec << "): sending Gather command" << endl;
#endif
                map<uint64_t, pair<int, int> >::iterator it = gatherBarrier.find(packet->DESTADRS);
                assert(it != gatherBarrier.end());
                gatherBarrier.erase(it);
#ifdef DEBUG_FLOW
                PrintARTree(packet->DESTADRS);
                //PrintARTree2(packet->DESTADRS);
#endif
                delete packet->trace;
              }
              DE_CR(ALI(18)<<header<<ALI(15)<<*packet<<"Down) SENDING packet to link mater "<<link<<" (LM_D"<<link<<")");
              delete downBuffers[0];
              downBuffers.erase(downBuffers.begin());
              break;
            }
            else {
              if (packet->CMD == ACT_GET) {
                gatherBarrier[packet->DESTADRS].second--;
              }
              packet->ReductGlobalTAG();
              delete packet;
              //DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<"Down) Link "<<link<<" buffer FULL");	
            }
          }
        }
      }
      else {
        Packet *packet = ConvTranIntoPacket(downBuffers[0]);
        bool can_send = true;
        if (packet->CMD == ACT_GET) {
          map<uint64_t, pair<int, int> >::iterator it = gatherBarrier.find(packet->DESTADRS);
          int nthreads = downBuffers[0]->nthreads;
          if (it != gatherBarrier.end()) {
            assert(nthreads == it->second.first);
            assert(gatherBarrier[packet->DESTADRS].second < nthreads);
            gatherBarrier[packet->DESTADRS].second++;
#ifdef DEBUG_GATHER
            cout << CYCLE() << "Active-Routing (flow: " << hex << packet->DESTADRS << dec
              << "): receive a Gather command, nthreads: " << nthreads
              << ", <nthreads, count>: <" << gatherBarrier[packet->DESTADRS].first
              << ", " << gatherBarrier[packet->DESTADRS].second << ">" << endl;
#endif
          } else {
            gatherBarrier.insert(make_pair(packet->DESTADRS, make_pair(nthreads, 1)));
#ifdef DEBUG_GATHER
            cout << CYCLE() << "Active-Routing (flow: " << hex << packet->DESTADRS << dec
              << "): receive a Gather command, nthreads: " << nthreads
              << ", <nthreads, count>: <" << gatherBarrier[packet->DESTADRS].first
              << ", " << gatherBarrier[packet->DESTADRS].second << ">" << endl;
#endif
          }
          if (gatherBarrier[packet->DESTADRS].second != nthreads) {
            can_send = false;
            //packet->ReductGlobalTAG();
            //delete packet;
            delete downBuffers[0]->trace;
            delete downBuffers[0];
            downBuffers.erase(downBuffers.begin());
          }
        }
        if(can_send && downLinkMasters[link]->ReceiveDown(packet)) {
          if (packet->CMD == ACT_GET) {
#ifdef DEBUG_GATHER
            cout << CYCLE() << "Active-Routing (flow:" << hex << packet->DESTADRS << dec << "): sending Gather command" << endl;
#endif
            map<uint64_t, pair<int, int> >::iterator it = gatherBarrier.find(packet->DESTADRS);
            assert(it != gatherBarrier.end());
            gatherBarrier.erase(it);
#ifdef DEBUG_FLOW
            PrintARTree(packet->DESTADRS);
            //PrintARTree2(packet->DESTADRS);
#endif
            delete packet->trace;
          }
          DE_CR(ALI(18)<<header<<ALI(15)<<*packet<<"Down) SENDING packet to link mater "<<link<<" (LM_D"<<link<<")");
          delete downBuffers[0];
          downBuffers.erase(downBuffers.begin());
        }
        else {
          if (packet->CMD == ACT_GET) {
            gatherBarrier[packet->DESTADRS].second--;
          }
          packet->ReductGlobalTAG();
          delete packet;
          //DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<"Down) Link "<<link<<" buffer FULL");	
        }
      }
    }

    //Upstream buffer state
    if(upBuffers.size() > 0) {
      //Make sure that buffer[0] is not virtual tail packet.
      if(upBuffers[0] == NULL) {
        ERROR(header<<"  == Error - HMC controller up buffer[0] is NULL (It could be one of virtual tail packet occupying packet length  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      else {
        DE_CR(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   RETURNING transaction to system bus");
        if (upBuffers[0]->CMD == ACT_GET) { // Jiayi, 02/07
#ifdef DEBUG_GATHER
          cout << "PUT GATHER RESPONSE (flow: " << hex << upBuffers[0]->DESTADRS << dec << ") to Q " << *upBuffers[0] << endl;
#endif
          responseQ.push_back(upBuffers[0]->DESTADRS);
        }
        upBuffers[0]->trace->tranFullLat = currentClockCycle - upBuffers[0]->trace->tranTransmitTime;
        if(upBuffers[0]->CMD == RD_RS) {
          upBuffers[0]->trace->statis->hmcTransmitSize += (upBuffers[0]->LNG - 1)*16;
        }
#ifndef CASHMC
        if (upBuffers[0]->CMD == ACT_GET) {
          //serv_trans.push_back(make_pair(upBuffers[0]->tran_tag, ACT_GET));
          serv_trans.push_back(make_pair(upBuffers[0]->DESTADRS, ACT_GET));
        } else {
          serv_trans.push_back(make_pair(upBuffers[0]->tran_tag, upBuffers[0]->CMD));
          //serv_trans.push_back(make_pair(upBuffers[0]->orig_addr, upBuffers[0]->CMD));
          //serv_trans.push_back(make_pair(upBuffers[0]->ADRS, upBuffers[0]->CMD));
        }
#endif
        int packetLNG = upBuffers[0]->LNG;
        assert(upBuffers[0]->packetType == RESPONSE);
        delete upBuffers[0]->trace;
        delete upBuffers[0];
        upBuffers.erase(upBuffers.begin(), upBuffers.begin()+packetLNG);
      }
    }

    Step();
  }

//this method is called from the CasHMCWrapper 
vector<pair<uint64_t, PacketCommandType> > &HMCController::get_serv_trans(){//pritam added this method
    return serv_trans;
}

  //
  //Convert transaction into packet-based protocol (FLITs) where the packets consist of 128-bit flow units
  //
  Packet *HMCController::ConvTranIntoPacket(Transaction *tran)
  {
    unsigned packetLength;
    unsigned reqDataSize=16;
    PacketCommandType cmdtype;

    switch(tran->transactionType) {
      case DATA_READ:
        packetLength = 1; //header + tail
        reqDataSize = tran->dataSize;
        switch(tran->dataSize) {
          case 16:	cmdtype = RD16;		break;
          case 32:	cmdtype = RD32;		break;
          case 48:	cmdtype = RD48;		break;
          case 64:	cmdtype = RD64;		break;
          case 80:	cmdtype = RD80;		break;
          case 96:	cmdtype = RD96;		break;
          case 112:	cmdtype = RD112;	break;
          case 128:	cmdtype = RD128;	break;
          case 256:	cmdtype = RD256;	break;
          default:
                    ERROR(header<<"  == Error - WRONG transaction data size  (CurrentClock : "<<currentClockCycle<<")");
                    exit(0);
        }
        break;
      case DATA_WRITE:		
        packetLength = tran->dataSize /*[byte] Size of data*/ / 16 /*packet 16-byte*/ + 1 /*header + tail*/;
        reqDataSize = tran->dataSize;
        switch(tran->dataSize) {
          case 16:	cmdtype = WR16;		break;
          case 32:	cmdtype = WR32;		break;
          case 48:	cmdtype = WR48;		break;
          case 64:	cmdtype = WR64;		break;
          case 80:	cmdtype = WR80;		break;
          case 96:	cmdtype = WR96;		break;
          case 112:	cmdtype = WR112;	break;
          case 128:	cmdtype = WR128;	break;
          case 256:	cmdtype = WR256;	break;
          default:
                    ERROR(header<<"  == Error - WRONG transaction data size  (CurrentClock : "<<currentClockCycle<<")");
                    exit(0);
        }
        break;
        //Arithmetic atomic
      case ATM_2ADD8:		cmdtype = _2ADD8;	packetLength = 2;	break;
      case ATM_ADD16:		cmdtype = ADD16;	packetLength = 2;	break;
      case ATM_P_2ADD8:	cmdtype = P_2ADD8;	packetLength = 2;	break;
      case ATM_P_ADD16:	cmdtype = P_ADD16;	packetLength = 2;	break;
      case ATM_2ADDS8R:	cmdtype = _2ADDS8R;	packetLength = 2;	break;
      case ATM_ADDS16R:	cmdtype = ADDS16R;	packetLength = 2;	break;
      case ATM_INC8:		cmdtype = INC8;		packetLength = 1;	break;
      case ATM_P_INC8:	cmdtype = P_INC8;	packetLength = 1;	break;
                        //Boolean atomic
      case ATM_XOR16:		cmdtype = XOR16;	packetLength = 2;	break;
      case ATM_OR16:		cmdtype = OR16;		packetLength = 2;	break;
      case ATM_NOR16:		cmdtype = NOR16;	packetLength = 2;	break;
      case ATM_AND16:		cmdtype = AND16;	packetLength = 2;	break;
      case ATM_NAND16:	cmdtype = NAND16;	packetLength = 2;	break;
                        //Comparison atomic
      case ATM_CASGT8:	cmdtype = CASGT8;	packetLength = 2;	break;
      case ATM_CASLT8:	cmdtype = CASLT8;	packetLength = 2;	break;
      case ATM_CASGT16:	cmdtype = CASGT16;	packetLength = 2;	break;
      case ATM_CASLT16:	cmdtype = CASLT16;	packetLength = 2;	break;
      case ATM_CASEQ8:	cmdtype = CASEQ8;	packetLength = 2;	break;
      case ATM_CASZERO16:	cmdtype = CASZERO16;packetLength = 2;	break;
      case ATM_EQ16:		cmdtype = EQ16;		packetLength = 2;	break;
      case ATM_EQ8:		cmdtype = EQ8;		packetLength = 2;	break;
                      //Bitwise atomic
      case ATM_BWR:		cmdtype = BWR;		packetLength = 2;	break;
      case ATM_P_BWR:		cmdtype = P_BWR;	packetLength = 2;	break;
      case ATM_BWR8R:		cmdtype = BWR8R;	packetLength = 2;	break;
      case ATM_SWAP16:	cmdtype = SWAP16;	packetLength = 2;	break;

      // ACTIVE commands, Jiayi, 01/27
      case ACTIVE_ADD:  reqDataSize = tran->dataSize; cmdtype = ACT_ADD;  packetLength = 1; break;
      case ACTIVE_MULT: reqDataSize = tran->dataSize; cmdtype = ACT_MULT; packetLength = 1; break;
      case ACTIVE_GET:  reqDataSize = tran->dataSize; cmdtype = ACT_GET;  packetLength = 1; break;
      case PIMINS_DOT:  reqDataSize = tran->dataSize; cmdtype = PEI_DOT;  packetLength = 3; break;

      default:
                       ERROR(header<<"   == Error - WRONG transaction type  (CurrentClock : "<<currentClockCycle<<")");
                       ERROR(*tran);
                       exit(0);
                       break;
    }
    //packet, cmd, addr, cub, lng, *lat
    //Packet *newPacket = new Packet(REQUEST, cmdtype, tran->address, 0, packetLength, tran->trace);
    // Jiayi, 01/31
    Packet *newPacket = NULL;
    if (tran->transactionType == ACTIVE_ADD || tran->transactionType == ACTIVE_GET) {
      newPacket = new Packet(REQUEST, cmdtype, tran->dest_address, tran->src_address1, 0, packetLength, tran->trace,
          tran->src_cube, tran->dest_cube1);
      newPacket->active = true;
      if (tran->transactionType == ACTIVE_ADD) {  // Jiayi, 03/15/17
        newPacket->ADRS = (tran->src_address1 << 30) >> 30;
      }
    } else if (tran->transactionType == ACTIVE_MULT) {  // Jiayi, 03/23/17
      newPacket = new Packet(REQUEST, cmdtype, 0, packetLength, tran->trace, tran->dest_address, tran->src_address1,
          tran->src_address2, tran->src_cube, tran->dest_cube1, tran->dest_cube2);
      newPacket->ADRS = (tran->src_address1 << 30) >> 30; // FIXME
      newPacket->active = true;
    } else {
      newPacket = new Packet(REQUEST, cmdtype, tran->address, 0, packetLength, tran->trace, tran->src_cube,
          tran->dest_cube1);
      newPacket->active = false;
    }
    newPacket->reqDataSize = reqDataSize;
    newPacket->orig_addr = tran->address;
    newPacket->tran_tag = tran->transactionID;
    return newPacket;
  }

  //
  //Print current state in state log file
  //
  void HMCController::PrintState()
  {
    if(downBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN("Down ");
      for(int i=0; i<downBufferMax; i++) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(i < downBuffers.size()) {
          STATEN(*downBuffers[i]);
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

    if(upBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN(" Up  ");
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
  }
  
  //
  // Print active routing tree given active flow ID (dest_addr)
  //
  void HMCController::PrintARTree(uint64_t dest_addr)
  {
    for (int l = 0; l < downLinkMasters.size(); ++l) {
      LinkSlave *lsp = dynamic_cast<LinkSlave *> (downLinkMasters[l]->linkP->linkSlaveP);
      assert(lsp);
      InputBuffer *inp_buf = dynamic_cast<InputBuffer *> (lsp->downBufferDest);
      assert(inp_buf);
      CrossbarSwitch * root_xbar = dynamic_cast<CrossbarSwitch *> (inp_buf->xbar);
      assert(root_xbar);
      map<FlowID, FlowEntry>::iterator it;
      it = root_xbar->flowTable.find(dest_addr);
      if (it == root_xbar->flowTable.end()) continue;
      
      // print the routing tree using BFS
      vector<CrossbarSwitch *> xbarQ;
      xbarQ.push_back(root_xbar);
      cout << "Active-Routing tree (flow: " << hex << dest_addr << dec << ") Parent: children ..." << endl;
      while (!xbarQ.empty()) {
        CrossbarSwitch *xbar = xbarQ[0];
        cout << xbar->cubeID << ": ";
        bool children_found = false;
        for (int c = 0; c < NUM_LINKS; ++c) {
          if (xbar->flowTable[dest_addr].children_count[c]) {
            LinkSlave *clsp = dynamic_cast<LinkSlave *> (xbar->upBufferDest[c]->linkP->linkSlaveP);
            assert(clsp);
            InputBuffer *cinp_buf = dynamic_cast<InputBuffer *> (clsp->downBufferDest);
            assert(cinp_buf);
            CrossbarSwitch * c_xbar = dynamic_cast<CrossbarSwitch *> (cinp_buf->xbar);
            assert(c_xbar);
            xbarQ.push_back(c_xbar);
            cout << c_xbar->cubeID << " ";
            children_found = true;
          }
        }
        if (children_found == false) {
          cout << "leaf node" << endl;
        } else {
          cout << endl;
        }
        xbarQ.erase(xbarQ.begin());
      }
    }
  }

  void HMCController::PrintARTree2(uint64_t dest_addr)
  {
    for (int l = 0; l < downLinkMasters.size(); ++l) {
      LinkSlave *lsp = dynamic_cast<LinkSlave *> (downLinkMasters[l]->linkP->linkSlaveP);
      assert(lsp);
      InputBuffer *inp_buf = dynamic_cast<InputBuffer *> (lsp->downBufferDest);
      assert(inp_buf);
      CrossbarSwitch * root_xbar = dynamic_cast<CrossbarSwitch *> (inp_buf->xbar);
      assert(root_xbar);
      map<FlowID, FlowEntry>::iterator it;
      it = root_xbar->flowTable.find(dest_addr);
      if (it == root_xbar->flowTable.end()) continue;

      // print the routing tree using BFS
      vector<unsigned> cubes;
      vector<CrossbarSwitch *> xbarQ;
      cubes.push_back(root_xbar->cubeID);
      xbarQ.push_back(root_xbar);
      cout << "(2) Active-Routing tree (flow: " << hex << dest_addr << dec << ") Parent: children ..." << endl;
      while (!xbarQ.empty()) {
        CrossbarSwitch *xbar = xbarQ[0];
        cout << xbar->cubeID << ": ";
        bool children_found = false;
        for (int c = 0; c < NUM_LINKS; ++c) {
          LinkSlave *clsp = dynamic_cast<LinkSlave *> (xbar->upBufferDest[c]->linkP->linkSlaveP);
          assert(clsp);
          InputBuffer *cinp_buf = dynamic_cast<InputBuffer *> (clsp->downBufferDest);
          assert(cinp_buf);
          CrossbarSwitch * c_xbar = dynamic_cast<CrossbarSwitch *> (cinp_buf->xbar);
          if (c_xbar) {
            map<FlowID, FlowEntry>::iterator c_it;
            c_it = c_xbar->flowTable.find(dest_addr);
            if (c_it == c_xbar->flowTable.end()) continue;
            else if (find(cubes.begin(), cubes.end(), c_xbar->cubeID) == cubes.end()) {
              cubes.push_back(c_xbar->cubeID);
              xbarQ.push_back(c_xbar);
              cout << c_xbar->cubeID << " ";
              children_found = true;
            }
          }
        }
        if (children_found == false) {
          cout << "leaf node" << endl;
        } else {
          cout << endl;
        }
        xbarQ.erase(xbarQ.begin());
      }
    }
  }

} //namespace CasHMC
