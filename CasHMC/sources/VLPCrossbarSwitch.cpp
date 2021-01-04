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

#include "VLPCrossbarSwitch.h"
#include "VLPVaultController.h"

namespace CasHMC
{

  // Ram & Jiayi, 03/13/17
  VLPCrossbarSwitch::VLPCrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_,unsigned id_, RoutingFunction *rf_):
    CrossbarSwitch(debugOut_, stateOut_, id_, rf_),
    numAdds(0), numMults(0), multVault(0), numFlows(0)
  {
    dispatchPolicy = ROUND_ROBIN;
  }

  VLPCrossbarSwitch::VLPCrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_):
    CrossbarSwitch(debugOut_, stateOut_),
    numAdds(0), numMults(0), multVault(0), numFlows(0)
  {
    dispatchPolicy = ROUND_ROBIN;
  }

  VLPCrossbarSwitch::~VLPCrossbarSwitch()
  {
    // Debugging Vault-Level Parallelism:
    if (numAdds > 0)
      cout << "CUBE " << cubeID << ", " << numAdds << " ADDs: "
        << numUpdates << " Updates and " << numFlows << " Flows" << endl;

    if (numMults > 0)
      cout << "CUBE " << cubeID << ", " << numMults << " MULTs: "
        << numUpdates << " Updates and " << numFlows << " Flows" << endl;

    for (int i = 0; i < operandBuffers.size(); i++)
      if (operandBuffers[i].flowID != 0)
        cout << "Error: For CUBE " << cubeID << " operand entry " << i << "still in use" << endl;

    map<FlowID, FlowEntry>::iterator iter = flowTable.begin();
    while (iter != flowTable.end()) {
      FlowID flowID = iter->first;
      cout << "Error: For CUBE " << cubeID << " found flow table still has req_count " << iter->second.req_count << " and rep_count " << iter->second.rep_count << " and g_flag " << iter->second.g_flag << endl;
      iter++;
    }
  }

  //
  //Update the state of crossbar switch
  //
  void VLPCrossbarSwitch::Update()
  {

    //Upstream buffer state, RESPONSE have higher priority
    for (int l = 0; l < inputBuffers.size(); l++) {
      int ll = (upLink + l) % inputBuffers.size();
      vector<Packet *> & curUpBuffers = inputBuffers[ll]->upBuffers;
      for(int i=0; i<curUpBuffers.size(); i++) {
        if(curUpBuffers[i] != NULL) {
          assert(curUpBuffers[i]->packetType == RESPONSE);
          //If the segment packet is arrived
          if(curUpBuffers[i]->segment) {
            //Check a stored segment packet tag
            bool foundSeg = false;
            bool foundLastSeg = true;
            for(int j=0; j<pendingSegTag.size(); j++) {
              if(curUpBuffers[i]->TAG == pendingSegTag[j]) {
                pendingSegTag.erase(pendingSegTag.begin()+j);
                foundSeg = true;
                //Check whether curUpBuffers[i] packet is the last segment packet or not
                for(int k=j; k<pendingSegTag.size(); k++) {
                  if(curUpBuffers[i]->TAG == pendingSegTag[k]) {
                    DEBUG(ALI(18)<<header<<ALI(15)<<*curUpBuffers[i]<<"Up)   Segment packet is WAITING for the others");
                    foundLastSeg = false;
                    break;
                  }
                }
                if(foundLastSeg) {
                  DEBUG(ALI(18)<<header<<ALI(15)<<*curUpBuffers[i]<<"Up)   The LAST segment packet is arrived");
                }
                break;
              }
            }
            if(!foundSeg) {
              ERROR(header<<"  == Error - pendingSegTag doesn't have segment packet tag ["<<*curUpBuffers[i]<<"]");
              exit(0);
            }

            //Segment packets are combined together
            foundSeg = false;
            for(int j=0; j<pendingSegPacket.size(); j++) {
              if(curUpBuffers[i]->TAG == pendingSegPacket[j]->TAG) {
                if(curUpBuffers[i]->LNG > 1)  pendingSegPacket[j]->LNG += ADDRESS_MAPPING/16;
                if(curUpBuffers[i]->trace != NULL)  pendingSegPacket[j]->trace = curUpBuffers[i]->trace;
                //Delete a segment packet
                int packetLNG = curUpBuffers[i]->LNG;
                delete curUpBuffers[i];
                curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+packetLNG);
                foundSeg = true;
                //All segment packets are combined
                if(foundLastSeg) {
                  Packet *combPacket = new Packet(*pendingSegPacket[j]);
                  delete pendingSegPacket[j];
                  pendingSegPacket.erase(pendingSegPacket.begin()+j);
                  combPacket->segment = false;
                  curUpBuffers.insert(curUpBuffers.begin(), combPacket);
                  for(int k=1; k<combPacket->LNG; k++) {  //Virtual tail packet
                    curUpBuffers.insert(curUpBuffers.begin()+1, NULL);
                  }
                }
                else {
                  i--;
                }
                break;
              }
            }
            //Thr first arrived segment packet
            if(!foundSeg) {
              pendingSegPacket.push_back(curUpBuffers[i]);
              curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+curUpBuffers[i]->LNG);
              i--;
            }
          }
          else {
            // XXX: ACT_ADD and ACT_DOT will be sent to vault for VLP and are processed in the else case
            if (curUpBuffers[i]->CMD == ACT_MULT && curUpBuffers[i]->DESTCUB == cubeID) { // Jiayi, 03/24/17
              int computeVault = curUpBuffers[i]->computeVault;
#ifdef DEBUG_VAULT
              cout << "CUBE " << cubeID << " FW RESPONSE FLOW " << hex << curUpBuffers[i]->DESTADRS << " (ADRS " << curUpBuffers[i]->ADRS << " SRCADRS1 " << curUpBuffers[i]->SRCADRS1 << " SRCADRS2 " << curUpBuffers[i]->SRCADRS2 << dec << ") to COMPUTE VAULT " << computeVault << endl;
#endif
              if (downBufferDest[computeVault]->ReceiveArt(curUpBuffers[i])) { // XXX: use ReceiveArt to avoid protocol deadlock
                int pktLNG = curUpBuffers[i]->LNG;
                curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+pktLNG);
                --i;
              }
            } else if (curUpBuffers[i]->CMD == ACT_GET && curUpBuffers[i]->DESTCUB == cubeID && curUpBuffers[i]->SRCCUB != cubeID) {
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
              if(it == flowTable.end()) cout << "HMC " << cubeID <<" assert for flow " << hex << dest_addr << dec << endl;
              assert(it != flowTable.end());
              int child_link = rf->findNextLink(inServiceLink, cubeID, curUpBuffers[i]->SRCCUB); // used to update childfield
              uint64_t child_count = flowTable[dest_addr].children_count[child_link];
              flowTable[dest_addr].rep_count += child_count;
              flowTable[dest_addr].children_count[child_link] = 0;
              int pktLNG = curUpBuffers[i]->LNG;
              delete curUpBuffers[i]->trace;
              delete curUpBuffers[i];
              curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+pktLNG);
              --i;
            } else if (curUpBuffers[i]->CMD == ACT_GET && curUpBuffers[i]->DESTCUB == cubeID && curUpBuffers[i]->SRCCUB == cubeID) {  // AKA: It's a vault reply
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
              if(it == flowTable.end()) cout << "HMC " << cubeID <<" assert for flow " << hex << dest_addr << dec << endl;
              assert(it != flowTable.end());
              unsigned vault = curUpBuffers[i]->SRCADRS1;
#ifdef DEBUG_VAULT
              cout << "CUBE " << cubeID << " received VC " << vault << " GATHER for FLOW " << hex << dest_addr << dec << endl;
#endif

              int vault_count = flowTable[dest_addr].vault_count[vault];
              assert(flowTable[dest_addr].vault_count[vault] > 0);
              flowTable[dest_addr].rep_count += vault_count;
              flowTable[dest_addr].vault_count[vault] = 0;
              int pktLNG = curUpBuffers[i]->LNG;
              delete curUpBuffers[i]->trace;
              delete curUpBuffers[i];
              curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+pktLNG);
              --i;
            } else {
              int link = rf->findNextLink(inServiceLink, cubeID, curUpBuffers[i]->DESTCUB);
              assert(link != -1);
              if (upBufferDest[link]->currentState != LINK_RETRY) {
                if (upBufferDest[link]->ReceiveUp(curUpBuffers[i])) {
                  curUpBuffers[i]->RTC = 0;
                  curUpBuffers[i]->URTC = 0;
                  curUpBuffers[i]->DRTC = 0;
                  curUpBuffers[i]->chkCRC = false;
                  curUpBuffers[i]->RRP = 0;
                  curUpBuffers[i]->chkRRP = false;
#ifdef DEBUG_GATHER
                  if (curUpBuffers[i]->CMD == ACT_GET) {
                    cout << CYCLE() << "cube " << cubeID << " is sending GET response (flow: " << hex << curUpBuffers[i]->DESTADRS << dec
                      << ") to parent " << neighborCubeID[link] << endl;
                  }
#endif
                  curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + curUpBuffers[i]->LNG);
                  --i;
                } else {
                  // no buffers, do nothing, try later
                }
              } else {
                // link conflict or LINK_RETRY, try next cycle
              }
            }
          }
        }
      }
    }
    upLink = (upLink + 1) % inputBuffers.size();

    vector<bool> vault_mask(NUM_VAULTS, false);
    //Downstream buffer state, only for REQUEST
    for (int l = 0; l < inputBuffers.size() - 1; l++) {
      int ll = (downLink + l) % (inputBuffers.size() - 1);
      vector<Packet *> & curDownBuffers = inputBuffers[ll]->downBuffers;
      if (inputBuffers[ll]->bufPopDelay == 0) {
        for (int i=0; i<curDownBuffers.size(); i++) {
          if (curDownBuffers[i] != NULL) {
            assert(curDownBuffers[i]->packetType == REQUEST);
            if (curDownBuffers[i]->DESTCUB == cubeID ||
                (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->halfPkt1 &&
                 curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->DESTCUB1 == cubeID ) ||
                (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->halfPkt2 &&
                 curDownBuffers[i]->SRCADRS2 && curDownBuffers[i]->DESTCUB2 == cubeID ) ||
                (((curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->DESTCUB1 == cubeID) ||
                  (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->SRCADRS2 && curDownBuffers[i]->DESTCUB2 == cubeID)) &&
                  !curDownBuffers[i]->halfPkt1 && !curDownBuffers[i]->halfPkt2)) {
              //Check request size and the maximum block size
              if (curDownBuffers[i]->reqDataSize > ADDRESS_MAPPING) {
                int segPacket = ceil((double)curDownBuffers[i]->reqDataSize/ADDRESS_MAPPING);
                curDownBuffers[i]->reqDataSize = ADDRESS_MAPPING;
                DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) Packet is DIVIDED into "<<segPacket<<" segment packets by max block size");

                //the packet is divided into segment packets.
                Packet *tempPacket = curDownBuffers[i];
                curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                if(tempPacket->LNG > 1) tempPacket->LNG = 1 + ADDRESS_MAPPING/16; //one flit is 16 bytes
                for(int j=0; j<segPacket; j++) {
                  Packet *vaultPacket = new Packet(*tempPacket);
                  vaultPacket->ADRS += j*ADDRESS_MAPPING;
                  if(j>1) vaultPacket->trace = NULL;
                  curDownBuffers.insert(curDownBuffers.begin()+i, vaultPacket);
                  for(int k=1; k<vaultPacket->LNG; k++) {   //Virtual tail packet
                    curDownBuffers.insert(curDownBuffers.begin()+i+1, NULL);
                  }
                  i += vaultPacket->LNG;
                  vaultPacket->segment = true;
                  pendingSegTag.push_back(vaultPacket->TAG);
                }
                delete tempPacket;
              }
              else {
                unsigned vaultMap = (curDownBuffers[i]->ADRS >> _log2(ADDRESS_MAPPING)) & (NUM_VAULTS-1);
                if (curDownBuffers[i]->CMD == ACT_ADD ||
                    curDownBuffers[i]->CMD == ACT_DOT) {
                  uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                  map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                  curDownBuffers[i]->computeVault = vaultMap;
                  if (vault_mask[vaultMap] == false && downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
#ifdef DEBUG_ROUTING
                    cout << "CUBE " << cubeID << ": Route packet " << *curDownBuffers[i] << " to my VaultCtrl " << vaultMap << endl;
#endif
                    numOperands++;
                    numUpdates++;
                    if (curDownBuffers[i]->CMD == ACT_ADD) {
                      numAdds++;
                    }
                    uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                    map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                    int link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                    int parent_cube = neighborCubeID[link];
                    if (it == flowTable.end()) {
                      flowTable.insert(make_pair(dest_addr, FlowEntry(ADD)));
                      numFlows++;
                      flowTable[dest_addr].parent = parent_cube;
                      flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                      cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                    } else {
                      assert(it->second.parent == parent_cube);
                      it->second.req_count++;
                    }
#ifdef DEBUG_VAULT
                    cout << "CUBE " << cubeID << " sending REQUEST to VC " << vaultMap << " for flow " << hex << dest_addr << dec << endl;
#endif
                    flowTable[dest_addr].vault_count[vaultMap]++;

                    Packet *pkt = curDownBuffers[i];
                    if (pkt->LINES > 1) {
                      curDownBuffers[i] = new Packet(*pkt);
                      curDownBuffers[i]->trace = new TranTrace(*(pkt->trace));
                    }
                    pkt->SRCCUB = cubeID;
                    pkt->DESTCUB = cubeID;
                    DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");
                    if (curDownBuffers[i]->LINES == 1) {
                      curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                    } else {
                      curDownBuffers[i]->SRCADRS1 += 64;
                      curDownBuffers[i]->ADRS += 64;
                      curDownBuffers[i]->LINES--;
                      assert(curDownBuffers[i]->ADRS == ((curDownBuffers[i]->SRCADRS1 << 30) >> 30));
                    }
                    i--;
                  }
                }
                else if (curDownBuffers[i]->CMD == ACT_MULT) {  // 03/24/17
                  uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                  bool is_full_pkt = (!curDownBuffers[i]->halfPkt1 && !curDownBuffers[i]->halfPkt2);
                  map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                  if (is_full_pkt) {
                    assert(vaultControllers[multVault]);
                    int operandBufID = vaultControllers[multVault]->OperandBufferStatus(curDownBuffers[i]);
                    if (operandBufID >= 0) {
                      curDownBuffers[i]->computeVault = multVault;
                      // make sure there is free operand buffer
                      Packet *pkt = new Packet(*curDownBuffers[i]);
                      if (pkt->SRCADRS1 && pkt->DESTCUB1 == cubeID) {
                        pkt->SRCADRS2 = 0;
                        vaultMap = (curDownBuffers[i]->SRCADRS1 >> _log2(ADDRESS_MAPPING)) & (NUM_VAULTS-1);
#ifdef DEBUG_VAULT
                        cout << "CUBE " << cubeID << " REQUEST FLOW " << hex << dest_addr << " (SRCADRS1 " << pkt->SRCADRS1 << " SRCADRS2 " << pkt->SRCADRS2 << dec << ") COMPUTE VAULT " << pkt->computeVault << " src1-> VC " << vaultMap << endl;
                        cout << "\tAlready reserved for operand entry " << pkt->vaultOperandBufID << endl;
#endif
                      } else {
                        assert(pkt->SRCADRS2 && pkt->DESTCUB2 == cubeID);
                        pkt->SRCADRS1 = 0;
                        vaultMap = (curDownBuffers[i]->SRCADRS2 >> _log2(ADDRESS_MAPPING)) & (NUM_VAULTS-1);
#ifdef DEBUG_VAULT
                        cout << "CUBE " << cubeID << " REQUEST FLOW " << hex << dest_addr << " (SRCADRS1 " << pkt->SRCADRS1 << " SRCADRS2 " << pkt->SRCADRS2 << dec << ") COMPUTE VAULT " << pkt->computeVault << " src2-> VC " << vaultMap << endl;
                        cout << "\tAlready reserved for operand entry " << pkt->vaultOperandBufID << endl;
#endif
                      }
                      // Only update this when the packet is sent down:
                      if (downBufferDest[vaultMap]->ReceiveDown(pkt)) {
                        flowTable[dest_addr].vault_count[multVault]++;
                        int parent_link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                        int parent_cube = neighborCubeID[parent_link];
                        if (it == flowTable.end()) {
                          flowTable.insert(make_pair(dest_addr, FlowEntry(MAC)));
                          numFlows++;
                          flowTable[dest_addr].parent = parent_cube;
                          flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                        } else {
                          assert(it->second.parent == parent_cube);
                          it->second.req_count++;
                        }
                        UpdateDispatch(curDownBuffers[i]);
                        numOperands++;
                        numUpdates++;
                        numMults++;
#ifdef DEBUG_ROUTING
                        cout << "CUBE " << cubeID << ": Route MULT (" << (pkt->DESTCUB1 == cubeID ? "first" : "second")
                          << ") packet " << *curDownBuffers[i] << " to my VaultCtrl" << endl;
#endif
#ifdef DEBUG_UPDATE
                        cout << "(0) CUBE " << cubeID << ": split MULT packet " << *curDownBuffers[i]
                          << ", dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
                        //cout << "(0) CUBE " << cubeID << ": split MULT packet " << *curDownBuffers[i] << " (" << curDownBuffers[i]
                        //  << "), dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
#endif
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                        cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): register an entry at cube " << cubeID << endl;
#endif
                        if (curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->DESTCUB1 == cubeID) {
                          //curDownBuffers[i]->SRCADRS1 = 0;
                          curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS2 << 30) >> 30;
                          curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB2;
                          curDownBuffers[i]->halfPkt2 = true;
                          pkt->ADRS = (pkt->SRCADRS1 << 30) >> 30;
                          pkt->halfPkt1 = true;
                        } else {
                          assert(curDownBuffers[i]->SRCADRS2 && curDownBuffers[i]->DESTCUB2 == cubeID);
                          //curDownBuffers[i]->SRCADRS2 = 0;
                          curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS1 << 30) >> 30;
                          curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB1;
                          curDownBuffers[i]->halfPkt1 = true;
                          pkt->ADRS = (pkt->SRCADRS2 << 30) >> 30;
                          pkt->halfPkt2 = true;
                        }
                        if (curDownBuffers[i]->LINES == 1) {
                          curDownBuffers[i]->SRCCUB = cubeID;
                        }
                        pkt->SRCCUB = cubeID;
                        pkt->DESTCUB = cubeID;
                        i--;
                      } else {
                        /*#ifdef DEBUG_UPDATE
                          cout << "CUBE " << cubeID << " ReciveDown fails for packet " << *curDownBuffers[i]
                          << " to vault " << vaultMap << endl;
#endif*/
                        //pkt->ReductGlobalTAG();
                        vaultControllers[multVault]->FreeOperandBuffer(operandBufID);
                        delete pkt;
                      }
                    }
                    else {
#ifdef DEBUG_VAULT
                      // Should we call UpdateDispatcher() here? If we could not get an operand from the current multVault, should we try
                      // the next one? Maybe we need a new function for this situation to tell the dispatcher that a vault doesn't have
                      // any operand buffers available?
                      cout << "CUBE " << cubeID << " REQUEST FLOW " << hex << dest_addr << dec << " COMPUTE VAULT " << vaultMap << " has no buffers available... multVault = " << multVault << endl;
#endif
                      UpdateDispatch(curDownBuffers[i]);
                    }
                  } else {
                    // Half packet coming back around for this cube - already have an operand entry for this one (don't do parent lookup)
#ifdef DEBUG_VAULT
                    cout << "CUBE " << cubeID << " REQUEST FLOW " << hex << dest_addr << " (SRCADRS1 " << curDownBuffers[i]->SRCADRS1 << " SRCADRS2 " << curDownBuffers[i]->SRCADRS2 << dec << ") COMPUTE VAULT " << curDownBuffers[i]->computeVault << " src2-> VC " << vaultMap << endl;
#endif
                    Packet *pkt = curDownBuffers[i]->LINES > 1 ? new Packet(*curDownBuffers[i]): curDownBuffers[i];
                    if (downBufferDest[vaultMap]->ReceiveDown(pkt)) {
                      assert(pkt->halfPkt1 ^ pkt->halfPkt2);
                      numOperands++;
#ifdef DEBUG_ROUTING
                      cout << "CUBE " << cubeID << ": Route MULT (second) packet " << *curDownBuffers[i] << " to my VaultCtrl" << endl;
#endif
                      if (pkt->halfPkt1) {
                        assert(pkt->ADRS == ((pkt->SRCADRS1 << 30) >> 30));
                        pkt->SRCADRS2 = 0;
                      } else {
                        assert(pkt->ADRS == ((pkt->SRCADRS2 << 30) >> 30));
                        pkt->SRCADRS1 = 0;
                      }
                      pkt->LINES = 1;
                      if (curDownBuffers[i]->LINES == 1) {
#ifdef DEBUG_PAGE_OFFLOADING
                        cout << CYCLE() << "Cube " << cubeID << " Page-Offloading: Last half packet "
                          << *pkt << " (" << pkt << ") and send to vault " << vaultMap << endl;
#endif
                        curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                      } else {
                        pkt->SRCCUB = cubeID;
                        pkt->DESTCUB = cubeID;
                        assert(pkt->DESTCUB1 == pkt->DESTCUB2);
                        curDownBuffers[i]->halfPkt1 = false;
                        curDownBuffers[i]->halfPkt2 = false;
                        curDownBuffers[i]->SRCADRS1 += 64;
                        curDownBuffers[i]->SRCADRS2 += 64;
                        curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS1 << 30) >> 30;
                        curDownBuffers[i]->LINES--;
                        curDownBuffers[i]->vaultOperandBufID = -1;
                        curDownBuffers[i]->trace = new TranTrace(*(curDownBuffers[i]->trace));
#ifdef DEBUG_PAGE_OFFLOADING
                        cout << CYCLE() << "Cube " << cubeID << " Page-Offloading: Replicate half packet "
                          << *pkt  << " (" << pkt << ") and send to vault " << vaultMap
                          << ", remaining packets: " << curDownBuffers[i]->LINES << endl;
#endif
                        assert(curDownBuffers[i]->ADRS == ((curDownBuffers[i]->SRCADRS1 << 30) >> 30) ||
                            curDownBuffers[i]->ADRS == ((curDownBuffers[i]->SRCADRS2 << 30) >> 30));
                      }
                      i--;
                    } else {
                      // do nothing, try next cycle
                      if (pkt != curDownBuffers[i]) delete pkt;
                    }
                  }
                }
                else if (curDownBuffers[i]->CMD == ACT_GET) {
                  uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                  // Jiayi, force the ordering for gather after update, 07/02/17
                  bool is_inorder = true;
                  for (int j = 0; j < i; j++) {
                    if (curDownBuffers[j] != NULL &&
                        ((curDownBuffers[j]->CMD == ACT_ADD ||
                          curDownBuffers[j]->CMD == ACT_DOT ||
                          curDownBuffers[j]->CMD == ACT_MULT) &&
                          curDownBuffers[j]->DESTADRS == dest_addr)) {
                      is_inorder = false;
                      break;
                    }
                  }
#ifdef DEBUG_VERIFY
                  for (int j = i + 1; j < curDownBuffers.size(); j++) {
                    if (curDownBuffers[j] != NULL &&
                        (curDownBuffers[j]->CMD == ACT_ADD ||
                         curDownBuffers[j]->CMD == ACT_DOT ||
                         curDownBuffers[j]->CMD == ACT_MULT))
                      assert(curDownBuffers[j]->DESTADRS != dest_addr);
                  }
#endif
                  if (!is_inorder) continue;
                  map<FlowID, FlowEntry>::iterator it  = flowTable.find(dest_addr);
                  if(it == flowTable.end()) cout << "HMC " << cubeID <<" assert for flow " << hex << dest_addr << dec << endl;
                  assert(it != flowTable.end());
                  vector<int> child_links;
                  for (int c = 0; c < NUM_LINKS; ++c) {
                    if (flowTable[dest_addr].children_count[c] > 0 && flowTable[dest_addr].children_gflag[c] == false)
                      child_links.push_back(c);
                  }
                  // send GET to children if any
                  if (child_links.size() > 0) {
                    assert(flowTable[dest_addr].req_count - flowTable[dest_addr].rep_count > 0);
                    // replicate and sent for each child, every cycle can only send one replicate
                    for (int c = 0; c < child_links.size(); ++c) {
                      int child_cube = neighborCubeID[child_links[c]];
                      int link = rf->findNextLink(inServiceLink, cubeID, child_cube);
                      assert(link == child_links[c]);
                      Packet *child_pkt = new Packet(*curDownBuffers[i]);
                      child_pkt->SRCCUB = cubeID;
                      child_pkt->DESTCUB = child_cube;
                      child_pkt->RTC = 0;
                      child_pkt->URTC = 0;
                      child_pkt->DRTC = 0;
                      child_pkt->chkCRC = false;
                      child_pkt->RRP = 0;  // Jiayi, 03/18/17, for retry pointer
                      child_pkt->chkRRP = false;
                      if (inputBuffers[ll]->ReceiveDown(child_pkt)) {
#ifdef DEBUG_GATHER
                        cout << CYCLE() << "Active-Routing: Replicate GET (flow: " << hex << dest_addr << dec
                          << ") from parent " << cubeID << " to child " << child_cube << endl;
#endif
                        flowTable[dest_addr].children_gflag[link] = true;
                        --i;
                        break;
                      } else {
                        //child_pkt->ReductGlobalTAG();
                        delete child_pkt;
                      }
                    }
                  } else {
#ifdef DEBUG_GATHER
                    cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): receive GET request at cube " << cubeID << endl;
#endif
                    // mark the g flag to indicate gather request arrives
                    flowTable[dest_addr].g_flag = true;
                    bool all_vc_received = true;
                    for (int j = 0; j < NUM_VAULTS; j++) {
                      if (flowTable[dest_addr].vault_gflag[j] == false && flowTable[dest_addr].vault_count[j] > 0) {
                        all_vc_received = false;
                        Packet *vault_pkt = new Packet(*curDownBuffers[i]);
                        if (downBufferDest[j]->ReceiveDown(vault_pkt)) {
#ifdef DEBUG_VAULT
                          cout << "CUBE " << cubeID << " sending GATHER for " << hex << dest_addr << dec << " to VC " << j << endl;
#endif
                          flowTable[dest_addr].vault_gflag[j] = true;
                          break;
                        } else {
                          delete vault_pkt;
                        }
                      }
                    }
                    if (all_vc_received) {
                      int pktLNG = curDownBuffers[i]->LNG;
                      delete curDownBuffers[i];
                      curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                      --i;
                    }
                  }
                }
                else if (downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
#ifdef DEBUG_ROUTING
                  cout << "CUBE#" << cubeID << ": Route packet " << *curDownBuffers[i] << " to my VaultCtrl" << endl;
#endif
                  DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");
                  int pktLNG = curDownBuffers[i]->LNG;
                  curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                  i--;
                }
                else {
                  //DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) Vault controller buffer FULL");
                }
              }
            } else {  // not destined for this cube
              int link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->DESTCUB);
              assert(link != -1);
              if (curDownBuffers[i]->CMD == ACT_MULT) {
                bool is_full_pkt = (!curDownBuffers[i]->halfPkt1 && !curDownBuffers[i]->halfPkt2);
                uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                if (is_full_pkt) {
                  assert(curDownBuffers[i]->SRCADRS1 != 0 && curDownBuffers[i]->SRCADRS2 != 0);
                  int link1 = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->DESTCUB1);
                  int link2 = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->DESTCUB2);
                  bool should_split = (link1 != link2);
                  if (should_split) {
#ifdef DEBUG_VAULT
                    cout << "SPLIT CUBE " << cubeID << " REQUEST FLOW " << hex << dest_addr << " (SRCADRS1 " << curDownBuffers[i]->SRCADRS1 << " SRCADRS2 " << curDownBuffers[i]->SRCADRS2 << dec << ") to COMPUTE VAULT " << multVault << " received..." << endl;
                    cout << "\tSRCCUB " << curDownBuffers[i]->SRCCUB << " DESTCUB " << curDownBuffers[i]->DESTCUB << " ADRS " << hex << curDownBuffers[i]->ADRS << dec << endl;
#endif
                    assert(vaultControllers[multVault]);
                    int operandBufID = vaultControllers[multVault]->OperandBufferStatus(curDownBuffers[i]);
                    if (operandBufID >= 0) {
                      curDownBuffers[i]->computeVault = multVault;
                      Packet *pkt = new Packet(*curDownBuffers[i]);
                      if (upBufferDest[link1]->currentState != LINK_RETRY && upBufferDest[link1]->ReceiveDown(pkt)) {
                        numUpdates++;
                        numMults++;
                        pkt->RTC = 0;
                        pkt->URTC = 0;
                        pkt->DRTC = 0;
                        pkt->chkCRC = false;
                        pkt->RRP = 0;
                        pkt->chkRRP = false;
                        pkt->SRCADRS2 = 0;
#ifdef DEBUG_UPDATE
                        cout << "(1) CUBE " << cubeID << ": split MULT packet " << *curDownBuffers[i]
                          << ", dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
                        //cout << "(1) CUBE " << cubeID << ": split MULT packet " << *curDownBuffers[i] << " (" << curDownBuffers[i]
                        //  << "), dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
#endif
                        map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                        int parent_link = rf->findNextLink(inServiceLink, cubeID, pkt->SRCCUB, true);
                        int parent_cube = neighborCubeID[parent_link];
                        if (it == flowTable.end()) {
                          flowTable.insert(make_pair(dest_addr, FlowEntry(MAC)));
                          numFlows++;
                          flowTable[dest_addr].parent = parent_cube;
                          flowTable[dest_addr].req_count = 1;
                          flowTable[dest_addr].vault_count[multVault] = 1;
                          //flowTable[dest_addr].computeVault = vault_to_send;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                        } else {
                          assert(it->second.parent == parent_cube);
                          it->second.req_count++;//no need to increment counter, has increased in curDownBuffers
                          it->second.vault_count[multVault]++;
                        }
                        //curDownBuffers[i]->SRCADRS1 = 0;
                        curDownBuffers[i]->halfPkt2 = true;
                        curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS2 << 30) >> 30;
                        if (curDownBuffers[i]->LINES == 1) {
                          curDownBuffers[i]->SRCCUB = cubeID;
                        }
                        curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB2;
                        pkt->SRCCUB = cubeID;
                        pkt->ADRS = (pkt->SRCADRS1 << 30) >> 30;
                        pkt->halfPkt1 = true;
                        pkt->LINES = 1;
                        i--;
                        UpdateDispatch(curDownBuffers[i]);
                      } else if (upBufferDest[link2]->currentState != LINK_RETRY && upBufferDest[link2]->ReceiveDown(pkt)) {
                        numUpdates++;
                        numMults++;
                        pkt->RTC = 0;
                        pkt->URTC = 0;
                        pkt->DRTC = 0;
                        pkt->chkCRC = false;
                        pkt->RRP = 0;
                        pkt->chkRRP = false;
                        pkt->SRCADRS1 = 0;
                        pkt->DESTCUB = pkt->DESTCUB2;
#ifdef DEBUG_UPDATE
                        cout << "(2) CUBE " << cubeID << ": split MULT packet " << *curDownBuffers[i]
                          << ", dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
                        //cout << "(2) CUBE " << cubeID << ": split MULT packet " << *curDownBuffers[i] << " (" << curDownBuffers[i]
                        //  << "), dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
#endif
                        map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                        int link = rf->findNextLink(inServiceLink, cubeID, pkt->SRCCUB, true);
                        int parent_cube = neighborCubeID[link];
                        if (it == flowTable.end()) {
                          flowTable.insert(make_pair(dest_addr, FlowEntry(MAC)));
                          numFlows++;
                          flowTable[dest_addr].parent = parent_cube;
                          flowTable[dest_addr].req_count = 1;
                          flowTable[dest_addr].vault_count[multVault] = 1;
                          //flowTable[dest_addr].computeVault = vault_to_send;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                        } else {
                          assert(it->second.parent == parent_cube);
                          it->second.req_count++;//no need to increment counter, has increased in curDownBuffers
                          it->second.vault_count[multVault]++;
                        }
                        //curDownBuffers[i]->SRCADRS2 = 0;
                        curDownBuffers[i]->halfPkt1 = true;
                        curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS1 << 30) >> 30;
                        curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB1;
                        if (curDownBuffers[i]->LINES == 1) {
                          curDownBuffers[i]->SRCCUB = cubeID;
                        }
                        pkt->ADRS = (pkt->SRCADRS2 << 30) >> 30;
                        pkt->SRCCUB = cubeID;
                        pkt->halfPkt2 = true;
                        pkt->LINES = 1;
                        i--;
                        UpdateDispatch(curDownBuffers[i]);
                      } else {
/*#ifdef DEBUG_UPDATE
                        cout << "CUBE " << cubeID << " downBufferDest Receive fails for packet " << *pkt
                          << " to either link1 " << link1 << " (cube " << neighborCubeID[link1]
                          << ") or link2 " << link2 << " (cube " << neighborCubeID[link2] << ")" << endl;
#endif*/
                        vaultControllers[multVault]->FreeOperandBuffer(operandBufID);
                        delete pkt; // try next time
                      }
                    }
#ifdef DEBUG_VAULT
                    else {
                      cout << "SPLIT CUBE " << cubeID << " REQUEST FLOW " << hex << dest_addr << dec << " COMPUTE VAULT " << multVault << " has no buffers available...multVault = " << multVault << endl;
                    }
#endif
                  } else { // no need for spliting
                    if (upBufferDest[link]->currentState != LINK_RETRY && upBufferDest[link]->ReceiveDown(curDownBuffers[i])) {
                      map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                      int parent_link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                      int parent_cube = neighborCubeID[parent_link];
                      if (it == flowTable.end()) {
                        flowTable.insert(make_pair(dest_addr, FlowEntry(MAC)));
                        numFlows++;
                        flowTable[dest_addr].parent = parent_cube;
                        flowTable[dest_addr].req_count = 1;
#if defined(EBUG_FLOW) || defined(DEBUG_UPDATE)
                        cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                      } else {
                        assert(it->second.parent == parent_cube);
                        it->second.req_count++;
                      }
                      curDownBuffers[i]->RTC = 0;
                      curDownBuffers[i]->URTC = 0;
                      curDownBuffers[i]->DRTC = 0;
                      curDownBuffers[i]->chkCRC = false;
                      curDownBuffers[i]->RRP = 0;
                      curDownBuffers[i]->chkRRP = false;
#ifdef DEBUG_ROUTING
                      cout << CYCLE() << "Active-Routing: route packet " << *curDownBuffers[i] << " from cube " << cubeID << " to cube " <<
                        curDownBuffers[i]->DESTCUB << ", next hop is cube " << neighborCubeID[link] << endl;
#endif
                      flowTable[dest_addr].children_count[link]++;
                      curDownBuffers.erase(curDownBuffers.begin() + i, curDownBuffers.begin() + i + curDownBuffers[i]->LNG);
                      --i;
                    } else {
                      // no buffers, do nothing, try later
                    }
                  }
                } else { // half packet
                  Packet *pkt = curDownBuffers[i]->LINES > 1 ? new Packet(*curDownBuffers[i]) : curDownBuffers[i];
                  if (upBufferDest[link]->currentState != LINK_RETRY && upBufferDest[link]->ReceiveDown(pkt)) {
                    assert(pkt->halfPkt1 ^ pkt->halfPkt2);
                    pkt->RTC = 0;
                    pkt->URTC = 0;
                    pkt->DRTC = 0;
                    pkt->chkCRC = false;
                    pkt->RRP = 0;
                    pkt->chkRRP = false;
#ifdef DEBUG_ROUTING
                    cout << CYCLE() << "Active-Routing: route packet " << *pkt << " from cube " << cubeID << " to cube " <<
                      pkt->DESTCUB << ", next hop is cube " << neighborCubeID[link] << endl;
#endif
                    if (pkt->halfPkt1) {
                      assert(pkt->ADRS == ((pkt->SRCADRS1 << 30) >> 30));
                      pkt->SRCADRS2 = 0;
                    } else {
                      assert(pkt->ADRS == ((pkt->SRCADRS2 << 30) >> 30));
                      pkt->SRCADRS1 = 0;
                    }
                    pkt->LINES = 1;
                    if (curDownBuffers[i]->LINES == 1) {
#ifdef DEBUG_PAGE_OFFLOADING
                      cout << CYCLE() << "Cube " << cubeID << " Page-Offloading: Last half packet "
                        << *pkt << " (" << pkt << ") and send to cube " << pkt->DESTCUB << endl;
#endif
                      curDownBuffers.erase(curDownBuffers.begin() + i, curDownBuffers.begin() + i + curDownBuffers[i]->LNG);
                    } else {
                      pkt->SRCCUB = cubeID;
                      pkt->DESTCUB = pkt->halfPkt1 ? pkt->DESTCUB1 : pkt->DESTCUB2;
                      curDownBuffers[i]->halfPkt1 = false;
                      curDownBuffers[i]->halfPkt2 = false;
                      curDownBuffers[i]->SRCADRS1 += 64;
                      curDownBuffers[i]->SRCADRS2 += 64;
                      curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS1 << 30) >> 30;
                      curDownBuffers[i]->LINES--;
                      curDownBuffers[i]->operandBufID = -1;
                      curDownBuffers[i]->trace = new TranTrace(*(curDownBuffers[i]->trace));
#ifdef DEBUG_PAGE_OFFLOADING
                      cout << CYCLE() << "Cube " << cubeID << " Page-Offloading: Replicate half packet "
                        << *pkt  << " (" << pkt << ") and send to cube " << pkt->DESTCUB
                        << ", remaining packets: " << curDownBuffers[i]->LINES << endl;
#endif
                      assert(curDownBuffers[i]->ADRS == ((curDownBuffers[i]->SRCADRS1 << 30) >> 30) ||
                          curDownBuffers[i]->ADRS == ((curDownBuffers[i]->SRCADRS2 << 30) >> 30));
                    }
                    --i;
                    break;
                  } else {
                    // no buffers, do nothing, try later
                    if (pkt != curDownBuffers[i]) delete pkt;
                  }
                }
              }
              else if (upBufferDest[link]->currentState != LINK_RETRY) {
                if (upBufferDest[link]->ReceiveDown(curDownBuffers[i])) {
                  if (curDownBuffers[i]->CMD == ACT_ADD ||
                      curDownBuffers[i]->CMD == ACT_DOT) {
                    assert(curDownBuffers[i]->packetType == REQUEST);
                    uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                    map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                    int parent_link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                    int parent_cube = neighborCubeID[parent_link];
                    if (it == flowTable.end()) {
                      flowTable.insert(make_pair(dest_addr, FlowEntry(ADD)));
                      numFlows++;
                      flowTable[dest_addr].parent = parent_cube;
                      flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                      cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                    } else {
                      assert(it->second.parent == parent_cube);
                      it->second.req_count++;
                    }
                    flowTable[dest_addr].children_count[link]++;
                  }
                  curDownBuffers[i]->RRP = 0;  // Jiayi, 03/18/17, for retry pointer
                  curDownBuffers[i]->chkRRP = false;
                  curDownBuffers[i]->RTC = 0; //return Tokens from previous transactions should be cleared, Ideally this should be done at linkMaster
                  curDownBuffers[i]->URTC = 0;
                  curDownBuffers[i]->DRTC = 0;
                  //as soon as the RTC of the master is updated in UpdateToken function.
                  curDownBuffers[i]->chkCRC = false; //Make the packet is ready for further transmision on other links. Not very clear, but it works for now!!
#ifdef DEBUG_ROUTING
                  cout << CYCLE() << "Active-Routing: route packet " << *curDownBuffers[i] << " from cube " << cubeID
                    << " to cube " << curDownBuffers[i]->DESTCUB << ", next hop is cube " << neighborCubeID[link] << endl;
#endif
                  curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                  i--;
                }
              }
            }
          }
        }
      }
    }
    downLink = (downLink + 1) % (inputBuffers.size() - 1);

    // Active-Routing processing
    // 1) consume available operands and free operand buffer
    bool startedMult = false;
    for (int i = 0; i < operandBuffers.size(); i++) {
      OperandEntry &operandEntry = operandBuffers[i];
      if (operandEntry.ready) {
        FlowID flowID = operandEntry.flowID;
        assert(flowTable.find(flowID) != flowTable.end());
        FlowEntry &flowEntry = flowTable[flowID];
        //cout << CYCLE() << "cubeID " << cubeID << " Current Occupancy: " << multPipeOccupancy << endl;
        if (flowEntry.opcode == MAC) {
          if (operandEntry.multStageCounter == numMultStages) {
            if (!startedMult && multPipeOccupancy < numMultStages) {
              //cout << CYCLE() << "cubeID " << cubeID << " Starting operand (incrementing counter)..." << endl;
              startedMult = true;
              multPipeOccupancy++;
              operandEntry.multStageCounter--;
            }
            // Otherwise wait to start until pipeline is not full
            continue; // don't free the buffer
          } else {
            //cout << CYCLE() << "cubeID " << cubeID << " Moving operand in stage " << (int) operandEntry.multStageCounter << endl;
            operandEntry.multStageCounter--;
            if (operandEntry.multStageCounter > 0)
              continue;
            else
              multPipeOccupancy--;
          }
        }
        //cout << CYCLE() << "cubeID " << cubeID << " ...Finised an operand" << endl;
        flowEntry.rep_count++;
#ifdef COMPUTE
        int org_res, new_res;
        if (flowEntry.opcode == ADD) {
          assert(operandEntry.src_addr1 && operandEntry.src_addr2 == 0 &&
              operandEntry.op1_ready && !operandEntry.op2_ready);
          int *value_p = (int *) operandEntry.src_addr1;
          org_res = flowEntry.result;
          new_res = org_res + *value_p;
        } else {
          assert(flowEntry.opcode == MAC);
          assert(operandEntry.src_addr1 && operandEntry.src_addr2 &&
              operandEntry.op1_ready && operandEntry.op2_ready);
          int *op1_p = (int *) operandEntry.src_addr1;
          int *op2_p = (int *) operandEntry.src_addr2;
          org_res = flowEntry.result;
          new_res = org_res + (*op1_p) * (*op2_p);
        }
        flowEntry.result = new_res;
        cout << CYCLE() << "AR (flow " << hex << flowID << dec << ") update partial result at cube " << cubeID
          << " from " << org_res << " to " << new_res << " at cube " << cubeID << ", req_count: "
          << flowEntry.req_count << ", rep_count: " << flowEntry.rep_count << endl;
#endif
#ifdef DEBUG_UPDATE
        cout << CYCLE() << "AR (flow " << hex << flowID << dec << ") releases operand buffer " << i << " at cube " << cubeID;
        if (flowEntry.opcode == ADD) {
          cout << " for operand addr " << hex << operandEntry.src_addr1 << dec << endl;
        } else {
          cout << " for operand addrs " << hex << operandEntry.src_addr1 << " and " << operandEntry.src_addr2 << dec << endl;
        }
#endif
        // release the operand buffer
        operandEntry.flowID = 0;
        operandEntry.src_addr1 = 0;
        operandEntry.src_addr2 = 0;
        operandEntry.op1_ready = false;
        operandEntry.op2_ready = false;
        operandEntry.ready = false;
        operandEntry.multStageCounter = numMultStages;
        freeOperandBufIDs.push_back(i);
#ifdef DEBUG_VAULT
        cout << "CUBE " << cubeID << " FREE OPERAND " << i << endl;
#endif
      }
    }

    // 2) reply ready GET response to commit the flow
    map<FlowID, FlowEntry>::iterator iter = flowTable.begin();
    while (iter != flowTable.end()) {
      FlowID flowID = iter->first;
      FlowEntry &flowEntry = iter->second;
      bool gather_sent = false;
      if (flowEntry.req_count == flowEntry.rep_count && flowEntry.g_flag) {
        int parent_cube = flowEntry.parent;
        int link = rf->findNextLink(inServiceLink, cubeID, parent_cube);
        TranTrace *trace = new TranTrace(transtat);
        Packet *gpkt = new Packet(RESPONSE, ACT_GET, flowID, 0, 0, 2, trace, cubeID, parent_cube);
        if (upBufferDest[link]->currentState != LINK_RETRY) {
          if (upBufferDest[link]->ReceiveUp(gpkt)) {
            gather_sent = true;
#ifdef COMPUTE
        int *dest = (int *) flowID;
        int org_res = *dest;
        *dest += flowEntry.result;
        cout << CYCLE() << "AR (flow " << hex << flowID << dec << ") update result from " << org_res << " to " << *dest
          << " at cube " << cubeID << ", req_count: " << flowEntry.req_count << ", rep_count: " << flowEntry.rep_count << endl;
#endif
#if defined(DEBUG_GATHER) || defined(DEBUG_FLOW)
        cout << CYCLE() << "AR (flow " << hex << flowID << dec << ") deallocates an flow entry at cube " << cubeID << endl;
        cout << "flow table size: " << flowTable.size() << endl;
#endif
          } else {
            delete trace;
            delete gpkt;
          }
        } else {
          delete trace;
          delete gpkt;
        }
      }
#ifdef DEBUG_VAULT
      else if (flowEntry.g_flag) {
        cout << "CUBE " << cubeID << " FLOW " << hex << flowID << dec << " has g_flag but req_count = " << flowEntry.req_count << " && rep_count = " << flowEntry.rep_count << endl;
      }
#endif
      if (flowTable.empty())
        break;
      if (gather_sent) {
        // deallocate flow table entry
        flowTable.erase(iter++);
#ifdef DEBUG_VAULT
        cout << "CUBE " << cubeID << " FREE FLOW " << hex << flowID << dec << endl;
#endif
      } else {
        iter++;
      }
    }

    for (int l = 0; l < inputBuffers.size(); l++) {
      inputBuffers[l]->Step();
    }

    Step();
  }

  //
  //Update multVault to choose which vault to dispatch the job
  //
  void VLPCrossbarSwitch::UpdateDispatch(Packet* p)
  {
    switch (dispatchPolicy) {
    case ROUND_ROBIN:
      multVault = (multVault + 1) % 32;
      break;
    case CONTENT_AWARE:
      break;
    default:
      break;
    }
  }

} //namespace CasHMC