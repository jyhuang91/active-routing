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

#include "CrossbarSwitch.h"
#include "VaultController.h"

namespace CasHMC
{

  // Ram & Jiayi, 03/13/17
  CrossbarSwitch::CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_,unsigned id_, RoutingFunction *rf_ = NULL):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_CROSS_BUF, MAX_CROSS_BUF), cubeID(id_), rf(rf_),
    operandBufSize(MAX_OPERAND_BUF), opbufStalls(0), numUpdates(0), numOperands(0)
  {
    classID << cubeID;
    header = "        (CS";
    header += classID.str() + ")";

    inServiceLink = 0; // Jiayi, FIXME: why Ram set it to 0? originally -1
    upLink = 0;
    downLink = 0;

    downBufferDest = vector<DualVectorObject<Packet, Packet> *>(NUM_VAULTS, NULL);
    upBufferDest = vector<LinkMaster *>(NUM_LINKS, NULL);

    // Jiayi, 03/15/17
    neighborCubeID = vector<int>(NUM_LINKS, -1);
    operandBuffers.resize(operandBufSize, OperandEntry());
    for (int i = 0; i < operandBufSize; i++) {
      freeOperandBufIDs.push_back(i);
    }

    inputBuffers.reserve(NUM_LINKS + 1);
    for (int l = 0; l < NUM_LINKS+1; l++) {
      classID.str("");
      classID << "CS" << cubeID;
      inputBuffers.push_back(new InputBuffer(debugOut_, stateOut_, l, classID.str()));
      inputBuffers[l]->xbar = this;
    }
  }
  
  CrossbarSwitch::CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_CROSS_BUF, MAX_CROSS_BUF),
    opbufStalls(0), numUpdates(0), numOperands(0)
  {
    header = "        (CS)";

    inServiceLink = 0; // Jiayi, FIXME: why Ram set it to 0? originally -1

    downBufferDest = vector<DualVectorObject<Packet, Packet> *>(NUM_VAULTS, NULL);
    upBufferDest = vector<LinkMaster *>(NUM_LINKS, NULL);
  }

  CrossbarSwitch::~CrossbarSwitch()
  {
    cout << "CUBE " << cubeID << " had " << opbufStalls << " operand buffer stalls" << endl;
	
    downBufferDest.clear();
    upBufferDest.clear();
    pendingSegTag.clear(); 
    pendingSegPacket.clear();

    flowTable.clear();
    operandBuffers.clear(); // Jiayi, 03/24/17
    freeOperandBufIDs.clear();

    rf = NULL;
    neighborCubeID.clear();

    for (int l = 0; l < NUM_LINKS+1; l++) {
      delete inputBuffers[l];
    }
  }

  //
  //Callback Adding packet
  //
  void CrossbarSwitch::CallbackReceiveDown(Packet *downEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
        }*/
  }
  void CrossbarSwitch::CallbackReceiveUp(Packet *upEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
        }*/
  }

  //
  //Update the state of crossbar switch
  //
  void CrossbarSwitch::Update()
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
                if(curUpBuffers[i]->LNG > 1)	pendingSegPacket[j]->LNG += ADDRESS_MAPPING/16;
                if(curUpBuffers[i]->trace != NULL)	pendingSegPacket[j]->trace = curUpBuffers[i]->trace;
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
                  for(int k=1; k<combPacket->LNG; k++) {	//Virtual tail packet
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
            if (curUpBuffers[i]->CMD == ACT_ADD ||
                curUpBuffers[i]->CMD == ACT_DOT) {
              assert(curUpBuffers[i]->DESTCUB == cubeID);
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              uint64_t src_addr = curUpBuffers[i]->SRCADRS1;
              int operand_buf_id = curUpBuffers[i]->operandBufID;
              map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
              assert(it != flowTable.end());
              OperandEntry &operandEntry = operandBuffers[operand_buf_id];
              assert(operandEntry.flowID == dest_addr && operandEntry.src_addr1 == src_addr);
              assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
              operandEntry.op1_ready = true;
              operandEntry.ready = true;
              int pktLNG = curUpBuffers[i]->LNG;
#ifdef DEBUG_UPDATE
              cout << "AR (flow " << dest_addr << ") update operand buffer " << operand_buf_id << " at cube " << cubeID
                << " by packet " << *curUpBuffers[i] << " for operand add " << hex << src_addr << dec << endl;
#endif
              delete curUpBuffers[i]->trace;
              delete curUpBuffers[i];
              curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + pktLNG);
              --i;
            } else if (curUpBuffers[i]->CMD == ACT_MULT && curUpBuffers[i]->DESTCUB == cubeID) { // Jiayi, 03/24/17
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              uint64_t src_addr1 = curUpBuffers[i]->SRCADRS1;
              uint64_t src_addr2 = curUpBuffers[i]->SRCADRS2;
              int operand_buf_id = curUpBuffers[i]->operandBufID;
              map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
              assert(it != flowTable.end());
              int parent_cube = flowTable[dest_addr].parent;
              int link = rf->findNextLink(inServiceLink, cubeID, parent_cube);
              OperandEntry &operandEntry = operandBuffers[operand_buf_id];
              if (src_addr1 && !src_addr2) {
                assert(operandEntry.src_addr1 == src_addr1 && !operandEntry.op1_ready && !operandEntry.ready);
                operandEntry.op1_ready = true;
              } else if (!src_addr1 && src_addr2) {
                assert(operandEntry.src_addr2 == src_addr2 && !operandEntry.op2_ready && !operandEntry.ready);
                operandEntry.op2_ready = true;
              } else {
                assert("ACT_MULT: both src1 and src2 addr are non zero, ERROR ..." == 0);
              }
              if (operandEntry.op1_ready && operandEntry.op2_ready) {
                operandEntry.ready = true;
                delete curUpBuffers[i]->trace;
              }
              int pktLNG = curUpBuffers[i]->LNG;
              delete curUpBuffers[i];
              curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + pktLNG);
              --i;
            } else if (curUpBuffers[i]->CMD == ACT_GET && curUpBuffers[i]->DESTCUB == cubeID && curUpBuffers[i]->SRCCUB != cubeID) {
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
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

    //Downstream buffer state, only for REQUEST
    for (int l = 0; l < inputBuffers.size() - 1; l++) {
      int ll = (downLink + l) % (inputBuffers.size() - 1);
      vector<Packet *> & curDownBuffers = inputBuffers[ll]->downBuffers;
      if (inputBuffers[ll]->bufPopDelay == 0) {
        for (int i=0; i<curDownBuffers.size(); i++) {
          if (curDownBuffers[i] != NULL) {
            assert(curDownBuffers[i]->packetType == REQUEST);
            if (curDownBuffers[i]->DESTCUB == cubeID ||
                (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->DESTCUB1 == cubeID) ||
                (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->SRCADRS2 && curDownBuffers[i]->DESTCUB2 == cubeID)) {
              //Check request size and the maximum block size
              if (curDownBuffers[i]->reqDataSize > ADDRESS_MAPPING) {
                int segPacket = ceil((double)curDownBuffers[i]->reqDataSize/ADDRESS_MAPPING);
                curDownBuffers[i]->reqDataSize = ADDRESS_MAPPING;
                DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) Packet is DIVIDED into "<<segPacket<<" segment packets by max block size");

                //the packet is divided into segment packets.
                Packet *tempPacket = curDownBuffers[i];
                curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                if(tempPacket->LNG > 1)	tempPacket->LNG = 1 + ADDRESS_MAPPING/16;	//one flit is 16 bytes
                for(int j=0; j<segPacket; j++) {
                  Packet *vaultPacket = new Packet(*tempPacket);
                  vaultPacket->ADRS += j*ADDRESS_MAPPING;
                  if(j>1)	vaultPacket->trace = NULL;
                  curDownBuffers.insert(curDownBuffers.begin()+i, vaultPacket);
                  for(int k=1; k<vaultPacket->LNG; k++) {		//Virtual tail packet
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
                  bool operand_buf_avail = freeOperandBufIDs.empty() ? false : true;
                  if (operand_buf_avail && downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
#ifdef DEBUG_ROUTING
                    cout << "CUBE " << cubeID << ": Route packet " << *curDownBuffers[i] << " to my VaultCtrl " << vaultMap << endl;
#endif
                    numOperands++;
                    numUpdates++;
                    uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                    map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                    int link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                    int parent_cube = neighborCubeID[link];
                    if (it == flowTable.end()) {
                      flowTable.insert(make_pair(dest_addr, FlowEntry(ADD)));
                      flowTable[dest_addr].parent = parent_cube;
                      flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                      cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                    } else {
                      assert(it->second.parent == parent_cube);
                      it->second.req_count++;
                    }
                    int operand_buf_id = freeOperandBufIDs.front();
                    freeOperandBufIDs.pop_front();
                    OperandEntry &operandEntry = operandBuffers[operand_buf_id];
                    assert(operandEntry.src_addr1 == 0 && operandEntry.src_addr2 == 0);
                    assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
                    operandEntry.flowID = dest_addr;
                    operandEntry.src_addr1 = curDownBuffers[i]->SRCADRS1; // only use the first operand
#ifdef DEBUG_UPDATE
                    cout << "Packet " << *curDownBuffers[i] << " reserves operand buffer " << operand_buf_id
                      << " at cube " << cubeID << " with operand addr " << hex << operandEntry.src_addr1 << dec << endl;
#endif
                    curDownBuffers[i]->operandBufID = operand_buf_id;
                    curDownBuffers[i]->SRCCUB = cubeID;
                    curDownBuffers[i]->DESTCUB = cubeID;
                    DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");
                    curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                    i--;
                  }
                  if (!operand_buf_avail)
                    opbufStalls++;
                }
                else if (curDownBuffers[i]->CMD == ACT_MULT) {  // 03/24/17
                  bool operand_buf_avail = false;
                  bool is_full_pkt = false;
                  // make sure there is free operand buffer
                  uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                  is_full_pkt = (curDownBuffers[i]->SRCADRS1 != 0 && curDownBuffers[i]->SRCADRS2 != 0);
                  if (is_full_pkt) {
                    operand_buf_avail = freeOperandBufIDs.empty() ? false : true;
                    if (operand_buf_avail) {
                      assert(curDownBuffers[i]->operandBufID = -1);
                      Packet *pkt = new Packet(*curDownBuffers[i]);
                      if (pkt->SRCADRS1 && pkt->DESTCUB1 == cubeID) {
                        pkt->SRCADRS2 = 0;
                        vaultMap = (curDownBuffers[i]->SRCADRS1 >> _log2(ADDRESS_MAPPING)) & (NUM_VAULTS-1);
                      } else {
                        assert(pkt->SRCADRS2 && pkt->DESTCUB2 == cubeID);
                        pkt->SRCADRS1 = 0;
                        vaultMap = (curDownBuffers[i]->SRCADRS2 >> _log2(ADDRESS_MAPPING)) & (NUM_VAULTS-1);
                      }
                      pkt->SRCCUB = cubeID;
                      pkt->DESTCUB = cubeID;
                      if (downBufferDest[vaultMap]->ReceiveDown(pkt)) {
                        numUpdates++;
                        numOperands++;
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
                        map<uint64_t, FlowEntry>::iterator it;
                        it = flowTable.find(dest_addr);
                        int link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                        int parent_cube = neighborCubeID[link];
                        if (it == flowTable.end()) {
                          flowTable.insert(make_pair(dest_addr, FlowEntry(MAC)));
                          flowTable[dest_addr].parent = parent_cube;
                          flowTable[dest_addr].req_count++;

#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): register an entry at cube " << cubeID << endl;
#endif
                        } else {
                          assert(it->second.parent == parent_cube);
                          it->second.req_count++;
                        }
                        int operand_buf_id = freeOperandBufIDs.front();
                        freeOperandBufIDs.pop_front();
                        OperandEntry &operandEntry = operandBuffers[operand_buf_id];
                        assert(operandEntry.src_addr1 == 0 && operandEntry.src_addr2 == 0);
                        assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
                        operandEntry.flowID = dest_addr;
                        operandEntry.src_addr1 = curDownBuffers[i]->SRCADRS1;
                        operandEntry.src_addr2 = curDownBuffers[i]->SRCADRS2;
#ifdef DEBUG_UPDATE
                        cout << "Packet " << *curDownBuffers[i] << " reserves operand buffer " << operand_buf_id
                          << " in cube " << cubeID << endl;
                        //cout << "Packet " << *curDownBuffers[i] << " reserves operand buffer " << operand_buf_id
                        //  << " in cube " << cubeID << ", src_addr1: " << hex << curDownBuffers[i]->SRCADRS1
                        //  << ", src_addr2: " << curDownBuffers[i]->SRCADRS2 << dec << endl;
#endif
                        if (curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->DESTCUB1 == cubeID) {
                          curDownBuffers[i]->SRCADRS1 = 0;
                          curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS2 << 30) >> 30;
                          curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB2;
                        } else {
                          assert(curDownBuffers[i]->SRCADRS2 && curDownBuffers[i]->DESTCUB2 == cubeID);
                          curDownBuffers[i]->SRCADRS2 = 0;
                          curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS1 << 30) >> 30;
                          curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB1;
                        }
                        curDownBuffers[i]->operandBufID = operand_buf_id;
                        curDownBuffers[i]->SRCCUB = cubeID;
                        pkt->operandBufID = operand_buf_id;
                        pkt->SRCCUB = cubeID;
                        i--;
                      } else {
/*#ifdef DEBUG_UPDATE
                        cout << "CUBE " << cubeID << " ReciveDown fails for packet " << *curDownBuffers[i]
                          << " to vault " << vaultMap << endl;
#endif*/
                        //pkt->ReductGlobalTAG();
                        delete pkt;
                      }
                    } else {
                      opbufStalls++;
/*#ifdef DEBUG_UPDATE
                      cout << "CUBE " << cubeID << ": no operand buffers (full), failed for packet " << *curDownBuffers[i] << endl;
#endif*/
                    }
                  } else {
                    if (downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
                      numOperands++;
#ifdef DEBUG_ROUTING
                      cout << "CUBE " << cubeID << ": Route MULT (second) packet " << *curDownBuffers[i] << " to my VaultCtrl" << endl;
#endif
                      curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
                      i--;
                    } else {
                      // do nothing, try next cycle
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
                    assert(flowTable[dest_addr].req_count -  flowTable[dest_addr].rep_count > 0);
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
                        /*if (child_links.size() == 1) {  // last child
                          int pktLNG = curDownBuffers[i]->LNG;
                          delete curDownBuffers[i];
                          curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                        }*/
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
                    // mark the g flag to indicate gather requtest arrives
                    flowTable[dest_addr].g_flag = true;
                    int pktLNG = curDownBuffers[i]->LNG;
                    delete curDownBuffers[i];
                    curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                    --i;
                  }
                }
                else if (downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
#ifdef DEBUG_ROUTING
                  cout << "CUBE#" << cubeID << ": Route packet " << *curDownBuffers[i] << " to my VaultCtrl" << endl;
#endif
                  DEBUG(ALI(18)<<header<<ALI(15)<<*curDownBuffers[i]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");
                  curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+curDownBuffers[i]->LNG);
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
                bool is_full_pkt = (curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->SRCADRS2);
                uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                if (is_full_pkt) {
                  int link1 = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->DESTCUB1);
                  int link2 = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->DESTCUB2);
                  bool should_split = (link1 != link2);
                  if (should_split) {
                    bool operand_buf_avail = freeOperandBufIDs.empty() ? false : true;
                    if (operand_buf_avail) {
                      assert(curDownBuffers[i]->operandBufID == -1);
                      Packet *pkt = new Packet(*curDownBuffers[i]);
                      if (upBufferDest[link1]->currentState != LINK_RETRY && upBufferDest[link1]->ReceiveDown(pkt)) {
                        numUpdates++;
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
                          flowTable[dest_addr].parent = parent_cube;
                          flowTable[dest_addr].req_count++;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                        } else {
                          assert(it->second.parent == parent_cube);
                          it->second.req_count++;//no need to increment counter, has increased in curDownBuffers
                        }
                        int operand_buf_id = freeOperandBufIDs.front();
                        freeOperandBufIDs.pop_front();
                        OperandEntry &operandEntry = operandBuffers[operand_buf_id];
                        assert(operandEntry.src_addr1 == 0 && operandEntry.src_addr2 == 0);
                        assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
                        operandEntry.flowID = dest_addr;
                        operandEntry.src_addr1 = curDownBuffers[i]->SRCADRS1;
                        operandEntry.src_addr2 = curDownBuffers[i]->SRCADRS2;
#ifdef DEBUG_UPDATE
                        cout << "Packet " << *pkt << " reserves operand buffer " << operand_buf_id << " in cube "
                          << cubeID << endl;
                        //cout << "Packet " << *pkt << " reserves operand buffer " << operand_buf_id << " in cube "
                        //  << cubeID << ", src_addr1: " << hex << curDownBuffers[i]->SRCADRS1 << ", src_addr2: "
                        //  << curDownBuffers[i]->SRCADRS2 << dec << endl;
#endif
                        curDownBuffers[i]->SRCADRS1 = 0;
                        curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS2 << 30) >> 30;
                        curDownBuffers[i]->operandBufID = operand_buf_id;
                        curDownBuffers[i]->SRCCUB = cubeID;
                        curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB2;
                        pkt->operandBufID = operand_buf_id;
                        pkt->SRCCUB = cubeID;
                        i--;
                      } else if (upBufferDest[link2]->currentState != LINK_RETRY && upBufferDest[link2]->ReceiveDown(pkt)) {
                        numUpdates++;
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
                          flowTable[dest_addr].parent = parent_cube;
                          flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << cubeID << endl;
#endif
                        } else {
                          assert(it->second.parent == parent_cube);
                          it->second.req_count++;//no need to increment counter, has increased in curDownBuffers
                        }
                        int operand_buf_id = freeOperandBufIDs.front();
                        freeOperandBufIDs.pop_front();
                        OperandEntry &operandEntry = operandBuffers[operand_buf_id];
                        assert(operandEntry.src_addr1 == 0 && operandEntry.src_addr2 == 0);
                        assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
                        operandEntry.flowID = dest_addr;
                        operandEntry.src_addr1 = curDownBuffers[i]->SRCADRS1;
                        operandEntry.src_addr2 = curDownBuffers[i]->SRCADRS2;
#ifdef DEBUG_UPDATE
                        cout << "Packet " << *pkt << " reserves operand buffer " << operand_buf_id << " in cube "
                          << cubeID << endl;
                        //cout << "Packet " << pkt << " reserves operand buffer " << operand_buf_id << " in cube "
                        //  << cubeID << ", src_addr1: " << hex << curDownBuffers[i]->SRCADRS1 << ", src_addr2: "
                        //  << curDownBuffers[i]->SRCADRS2 << dec << endl;
#endif
                        curDownBuffers[i]->SRCADRS2 = 0;
                        curDownBuffers[i]->ADRS = (curDownBuffers[i]->SRCADRS1 << 30) >> 30;
                        curDownBuffers[i]->operandBufID = operand_buf_id;
                        curDownBuffers[i]->SRCCUB = cubeID;
                        curDownBuffers[i]->DESTCUB = curDownBuffers[i]->DESTCUB1;
                        pkt->operandBufID = operand_buf_id;
                        pkt->SRCCUB = cubeID;
                        i--;
                      } else {
/*#ifdef DEBUG_UPDATE
                        cout << "CUBE " << cubeID << " downBufferDest Receive fails for packet " << *pkt
                          << " to either link1 " << link1 << " (cube " << neighborCubeID[link1]
                          << ") or link2 " << link2 << " (cube " << neighborCubeID[link2] << ")" << endl;
#endif*/
                        delete pkt; // try next time
                      }
                    } else {  // operand buffer not available
                      opbufStalls++;
/*#ifdef DEBUG_UPDATE
                        cout << "CUBE " << cubeID << ": no operand buffers (full), failed for packet " << *curDownBuffers[i] << endl;
#endif*/
                    }
                  } else { // no need for spliting
                    if (upBufferDest[link]->currentState != LINK_RETRY && upBufferDest[link]->ReceiveDown(curDownBuffers[i])) {
                      map<FlowID, FlowEntry>::iterator it = flowTable.find(dest_addr);
                      int parent_link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                      int parent_cube = neighborCubeID[parent_link];
                      if (it == flowTable.end()) {
                        flowTable.insert(make_pair(dest_addr, FlowEntry(MAC)));
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
                  if (upBufferDest[link]->currentState != LINK_RETRY && upBufferDest[link]->ReceiveDown(curDownBuffers[i])) {
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
                    curDownBuffers.erase(curDownBuffers.begin() + i, curDownBuffers.begin() + i + curDownBuffers[i]->LNG);
                    --i;
                    break;
                  } else {
                    // no buffers, do nothing, try later
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
    for (int i = 0; i < operandBuffers.size(); i++) {
      OperandEntry &operandEntry = operandBuffers[i];
      if (operandEntry.ready) {
        FlowID flowID = operandEntry.flowID;
        assert(flowTable.find(flowID) != flowTable.end());
        FlowEntry &flowEntry = flowTable[flowID];
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
        // release the oeprand buffer
        operandEntry.flowID = 0;
        operandEntry.src_addr1 = 0;
        operandEntry.src_addr2 = 0;
        operandEntry.op1_ready = false;
        operandEntry.op2_ready = false;
        operandEntry.ready = false;
        freeOperandBufIDs.push_back(i);
      }
    }
    // 2) reply ready GET response to commit the flow
    map<FlowID, FlowEntry>::iterator iter = flowTable.begin();
    while (iter != flowTable.end()) {
      FlowID flowID = iter->first;
      FlowEntry &flowEntry = iter->second;
      if (flowEntry.req_count == flowEntry.rep_count && flowEntry.g_flag) {
        int parent_cube = flowEntry.parent;
        int link = rf->findNextLink(inServiceLink, cubeID, parent_cube);
        TranTrace *trace = new TranTrace(transtat);
        Packet *gpkt = new Packet(RESPONSE, ACT_GET, flowID, 0, 0, 2, trace, cubeID, parent_cube);
        if (upBufferDest[link]->currentState != LINK_RETRY) {
          if (upBufferDest[link]->ReceiveUp(gpkt)) {
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
            // deallocate flow table entry
            flowTable.erase(iter);
          } else {
            delete gpkt;
          }
        } else {
          delete gpkt;
        }
      }
      if (flowTable.empty())
        break;
      iter++;
    }

    for (int l = 0; l < inputBuffers.size(); l++) {
      inputBuffers[l]->Step();
    }

    Step();
  }

  //
  //Print current state in state log file
  //
  void CrossbarSwitch::PrintState()
  {
    int realInd = 0;
    if(downBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN("Down ");
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

    if(upBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN(" Up  ");
      if(upBuffers.size() < upBufferMax) {
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
      }
      else {
        for(int i=0; i<upBuffers.size(); i++) {
          if(i>0 && i%8==0) {
            STATEN(endl<<"                      ");
          }
          if(upBuffers[i] != NULL)	realInd = i;
          STATEN(*upBuffers[realInd]);
        }
      }
      STATEN(endl);
    }
  }


  //
  //Print buffers
  //
  void CrossbarSwitch::PrintBuffers()
  {
    cout << "Crossbar (HMC) " << cubeID << endl;
    int i = 0;
    for (map<FlowID, FlowEntry>::iterator it = flowTable.begin();
        it != flowTable.end(); it++) {
      cout << " -- flow entry " << i << ":" << endl;
      cout << "    [flowID: " << hex << it->first << dec
        << "] [opcode: " << it->second.opcode
        << "] [result: " << it->second.result
        << "] [req_count: " << it->second.req_count
        << "] [rep_count: " << it->second.rep_count
        << "] [parent: " << it->second.parent
        << "] [children_gflag_count:";
      for (int c = 0; c < NUM_LINKS; c++) {
        if (it->second.children_gflag[c]) {
          cout << " true:" << it->second.children_count[c];
        } else {
          cout << " false:" << it->second.children_count[c];
        }
      }
      cout << "] [gflag: " << it->second.g_flag << "]" << endl;
      i++;
    }
    for (int i = 0; i < inputBuffers.size(); i++) {
      vector<Packet *> downBuffer = inputBuffers[i]->downBuffers;
      vector<Packet *> upBuffer = inputBuffers[i]->upBuffers;
      cout << " -- input buffer " << i << ":" << endl;
      cout << "    downBuffer (size: " << downBuffer.size() << ")" << endl;
      for (int i = 0; i < downBuffer.size(); i++) {
        if (downBuffer[i] != NULL) {
          int link = rf->findNextLink(inServiceLink, cubeID, downBuffer[i]->DESTCUB);
          int next_cube = neighborCubeID[link];
          cout << (downBuffer[i]->packetType == REQUEST ? "    Request " : "    Response ")
            << *downBuffer[i] << " from current cube " << cubeID << " to next cube " << next_cube
            << " (src_cube: " << downBuffer[i]->SRCCUB << ", dest_cube: " << downBuffer[i]->DESTCUB
            << ", packet length: " << downBuffer[i]->LNG;
          if (downBuffer[i]->CMD == ACT_MULT) {
            if (downBuffer[i]->SRCADRS1 && downBuffer[i]->SRCADRS2) {
              cout << ", full pkt, dest_cube1: " << downBuffer[i]->DESTCUB1 << ", dest_cube: " << downBuffer[i]->DESTCUB2
                << ")" << endl;
            } else if (downBuffer[i]->SRCADRS1 && !downBuffer[i]->SRCADRS2) {
              cout << ", first operand pkt)" << endl;
            } else {
              cout <<", second operand pkt)" << endl;
            }
          } else {
            cout << ")" << endl;
          }
        }
      }
      cout << "    upBuffer (size: " << upBuffer.size() << ")" << endl;
      for (int i = 0; i < upBuffer.size(); i++) {
        if (upBuffer[i] != NULL) {
          int link = rf->findNextLink(inServiceLink, cubeID, upBuffer[i]->DESTCUB);
          int next_cube = neighborCubeID[link];
          cout << (upBuffer[i]->packetType == REQUEST ? "    Request " : "    Response ")
            << *upBuffer[i] << " from current cube " << cubeID << " to next cube " << next_cube
            << " (src_cube: " << upBuffer[i]->SRCCUB << ", dest_cube: " << upBuffer[i]->DESTCUB
            << ", packet length: " << upBuffer[i]->LNG;
          if (upBuffer[i]->CMD == ACT_MULT) {
            if (upBuffer[i]->SRCADRS1 && upBuffer[i]->SRCADRS2) {
              cout << ", full pkt, dest_cube1: " << upBuffer[i]->DESTCUB1 << ", dest_cube2: " << upBuffer[i]->DESTCUB2
                << ")" << endl;
            } else if (upBuffer[i]->SRCADRS1 && !upBuffer[i]->SRCADRS2) {
              cout << ", first operand pkt)" << endl;
            } else {
              cout <<", second operand pkt)" << endl;
            }
          } else {
            cout << ")" << endl;
          }
        }
      }
    }
    cout << " -- LinkMaster token counts:" << endl;
    for (int i = 0; i < upBufferDest.size(); i++) {
      cout << "    linkMaster " << i << ": utk( " << upBufferDest[i]->upTokenCount
        << " ) - dtk( " << upBufferDest[i]->downTokenCount << " ) - linkRxTx( "
        << upBufferDest[i]->linkRxTx.size() << " ) - upBuffer( "
        << upBufferDest[i]->upBuffers.size() << " ) - downBuffer( "
        << upBufferDest[i]->downBuffers.size() << " )"<< endl;
      cout << "      upBuffer size: " << upBufferDest[i]->upBuffers.size() << endl;
      for (int j = 0; j < upBufferDest[i]->upBuffers.size(); j++) {
        if (upBufferDest[i]->upBuffers[j] != NULL) {
          cout << "        ";
          upBufferDest[i]->upBuffers[j]->Display();
        }
      }
      cout << "      downBuffer size: " << upBufferDest[i]->downBuffers.size() << endl;
      for (int j = 0; j < upBufferDest[i]->downBuffers.size(); j++) {
        if (upBufferDest[i]->downBuffers[j] != NULL) {
          cout << "        ";
          upBufferDest[i]->downBuffers[j]->Display();
        }
      }
      cout << "    localLinkSlave " << i << ": downBuffer( "
        << upBufferDest[i]->localLinkSlave->downBuffers.size() << " ) - upBuffer( "
        << upBufferDest[i]->localLinkSlave->upBuffers.size() << " )"<< endl;
      cout << "      upBuffer size: " << upBufferDest[i]->localLinkSlave->upBuffers.size() << endl;
      for (int j = 0; j < upBufferDest[i]->localLinkSlave->upBuffers.size(); j++) {
        if (upBufferDest[i]->localLinkSlave->upBuffers[j] != NULL) {
          cout << "        ";
          upBufferDest[i]->localLinkSlave->upBuffers[j]->Display();
        }
      }
      cout << "      downBuffer size: " << upBufferDest[i]->localLinkSlave->downBuffers.size() << endl;
      for (int j = 0; j < upBufferDest[i]->localLinkSlave->downBuffers.size(); j++) {
        if (upBufferDest[i]->localLinkSlave->downBuffers[j] != NULL) {
          cout << "        ";
          upBufferDest[i]->localLinkSlave->downBuffers[j]->Display();
        }
      }
    }
    for (int i = 0; i < downBufferDest.size(); i++) {
      VaultController *vault = dynamic_cast<VaultController *> (downBufferDest[i]);
      assert(vault);
      vault->PrintBuffers();
    }
  }

} //namespace CasHMC
