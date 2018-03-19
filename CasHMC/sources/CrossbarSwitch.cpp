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
    operandBuffers.resize(operandBufSize, make_pair(make_pair(0, false), make_pair(0, false)));
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
    downBufferDest.clear();
    upBufferDest.clear();
    pendingSegTag.clear(); 
    pendingSegPacket.clear();
    // Jiayi, 02/06
    reserveTable.clear();
    childrenTable.clear();
    //activeBuffers.clear();
    activeReturnBuffers.clear();
    operandBuffers.clear(); // Jiayi, 03/24/17
    freeOperandBufIDs.clear();
    // Jiayi, 03/13/17
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
            if (curUpBuffers[i]->CMD == ACT_ADD) {
              assert(curUpBuffers[i]->DESTCUB == cubeID);
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              uint64_t src_addr = curUpBuffers[i]->SRCADRS1;
              map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
              it = reserveTable.find(dest_addr);
              assert(it != reserveTable.end());
              int parent_cube = reserveTable[dest_addr].second.first;
              int link = rf->findNextLink(inServiceLink, cubeID, parent_cube);
              unsigned count = reserveTable[dest_addr].first.first - 1;
              if (count == 0) {
#ifdef DEBUG_VERIFY
                for (int c = 0; c < childrenTable[dest_addr].size(); ++c) {
                  assert(childrenTable[dest_addr][c] == false);
                }
                vector<int> children = reserveTable[dest_addr].second.second;
                for (int c = 0; c < children.size(); ++c) {
                  assert(children[c] == 0);
                }
#endif
                map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                if (ret_it != activeReturnBuffers.end()) {
                  assert(ret_it->second == NULL);
                  Packet *ret_pkt = curUpBuffers[i];
                  if (inputBuffers[ll]->ReceiveUp(ret_pkt)) {
                    ret_pkt->SRCCUB = cubeID;
                    ret_pkt->DESTCUB = parent_cube;
                    ret_pkt->CMD = ACT_GET;
                    ret_pkt->RTC = 0;
                    ret_pkt->URTC = 0;
                    ret_pkt->DRTC = 0;
                    ret_pkt->chkCRC = 0;
                    ret_pkt->RRP = 0;  // Jiayi, 03/18/17, for retry pointer
                    ret_pkt->chkRRP = false;
#ifdef COMPUTE
                    int *src = (int *) src_addr;
                    int *dest = (int *) dest_addr;
                    int org = reserveTable[dest_addr].first.second;
                    int entry = org + *src; // update the entry value
                    int current_dest = *dest;
                    reserveTable[dest_addr].first.second = entry;
                    *dest += entry; // update the entry value
                    cout << "CUBE#" << cubeID << " sends back GET response, update target from " << current_dest
                      << " to " << *dest << endl;
#endif
                    reserveTable[dest_addr].first.first--;  // update the counter
#ifdef DEBUG_UPDATE
#ifdef COMPUTE
                    cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                      << *curUpBuffers[i] << ", update it from " << org << " to " << *dest << ", src is " << *src << " (0x"
                      << src << "), remaining count: " << reserveTable[dest_addr].first.first
                      << " ---- send ACT_GET response to parent#" << parent_cube << " as well" << endl;
#else
                    cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                      << *curUpBuffers[i] << ", remaining count: " << reserveTable[dest_addr].first.first
                      << " ---- send ACT_GET response to parent#" << parent_cube << " as well" << endl;
#endif
#endif
                    reserveTable.erase(it);
#ifdef DEBUG_FLOW
                    cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): clear flow entry at cube#" << cubeID << endl;
#endif
                    map<uint64_t, vector<bool> >::iterator c_it = childrenTable.find(dest_addr);
                    childrenTable.erase(c_it);
                    activeReturnBuffers.erase(ret_it);
                    curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + curUpBuffers[i]->LNG);
                    --i;
                  } else {
                    // no buffers, do nothing, try next cycle 
                    //break;
                  }
                } else {  // GET not ready, reserve an activeReturnBuffer slot, will process when GET comes
                  // update the table entries
#ifdef COMPUTE
                  int *src = (int *) src_addr;
                  int org = reserveTable[dest_addr].first.second;
                  int entry = org + *src; // update the entry value
                  reserveTable[dest_addr].first.second = entry;
#endif
                  reserveTable[dest_addr].first.first--;  // update the counter
#ifdef DEBUG_UPDATE
#ifdef COMPUTE
                  cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                    << *curUpBuffers[i] << ", update it from " << org << " to " << entry << ", src is " << *src << " (0x"
                    << src << "), remaining count: " << reserveTable[dest_addr].first.first
                    << " ---- put into activeReturnBuffers, ";
#else
                  cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                    << *curUpBuffers[i] << ", remaining count: " << reserveTable[dest_addr].first.first
                    << " ---- put into activeReturnBuffers, ";
#endif
                  cout << "add return packet for cube#" << cubeID << " by " << *curUpBuffers[i] << endl;
#endif
                  activeReturnBuffers.insert(make_pair(dest_addr, curUpBuffers[i]));
                  curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + curUpBuffers[i]->LNG);
                  --i;
                }
              } else {  // not last one, update table entry and delete it
#ifdef COMPUTE
                int *src = (int *) src_addr;
                int org = reserveTable[dest_addr].first.second;
                int entry = org + *src; // update the entry value
                reserveTable[dest_addr].first.second = entry;
#endif
                reserveTable[dest_addr].first.first--;
#ifdef DEBUG_UPDATE
#ifdef COMPUTE
                cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                  << *curUpBuffers[i] << ", update it from " << org << " to " << entry << ", src is " << *src << " (0x"
                  << src << "), remaining count: " << reserveTable[dest_addr].first.first << endl;
                reserveTable[dest_addr].first.second = entry;
#else
                cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                  << *curUpBuffers[i] << ", remaining count: " << reserveTable[dest_addr].first.first << endl;
#endif
#endif
                map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                if (ret_it != activeReturnBuffers.end()) {
                  assert(ret_it->second == NULL);
                }
                int pktLNG = curUpBuffers[i]->LNG;
                delete curUpBuffers[i]->trace;
                delete curUpBuffers[i];
                curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + pktLNG);
                --i;
              }
            } else if (curUpBuffers[i]->CMD == ACT_MULT && curUpBuffers[i]->DESTCUB == cubeID) { // Jiayi, 03/24/17
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              uint64_t src_addr1 = curUpBuffers[i]->SRCADRS1;
              uint64_t src_addr2 = curUpBuffers[i]->SRCADRS2;
              int operand_buf_id = curUpBuffers[i]->operandBufID;
              map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
              it = reserveTable.find(dest_addr);
              assert(it != reserveTable.end());
              int parent_cube = reserveTable[dest_addr].second.first;
              int link = rf->findNextLink(inServiceLink, cubeID, parent_cube);
              pair<pair<uint64_t, bool>, pair<uint64_t, bool> > &operandEntry =
                operandBuffers[operand_buf_id];
              if (!operandEntry.first.second && !operandEntry.second.second) {// will consume the packet for sure
                if (src_addr1 && !src_addr2) {
                  //cout << "CUBE#" << cubeID << " - operand buf id: " << operand_buf_id << " in cube#" << cubeID
                  //  << hex << ", src_addr1: " << src_addr1 << ", src_addr2: " << src_addr2 << ", op addr1: "
                  //  << operandEntry.first.first << ", op addr2: " << operandEntry.second.first << dec << endl;
                  assert(operandEntry.first.first == src_addr1);
                  assert(operandEntry.first.second == false);
                  operandEntry.first.second = true;
                } else if (!src_addr1 && src_addr2) {
                  assert(operandEntry.second.first == src_addr2);
                  assert(operandEntry.second.second == false);
                  operandEntry.second.second = true;
                } else {
                  assert("ACT_MULT: both src1 and src2 addr are non zero, ERROR ..." == 0);
                }
                int pktLNG = curUpBuffers[i]->LNG;
                delete curUpBuffers[i];
                curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + pktLNG);
                --i;
              } else {
                unsigned count = reserveTable[dest_addr].first.first - 1;
                if (count == 0) {
#ifdef DEBUG_VERIFY
                  for (int c = 0; c < childrenTable[dest_addr].size(); ++c) {
                    assert(childrenTable[dest_addr][c] == false);
                  }
                  vector<int> children = reserveTable[dest_addr].second.second;
                  for (int c = 0; c < children.size(); ++c) {
                    assert(children[c] == 0);
                  }
#endif
                  map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                  if (ret_it != activeReturnBuffers.end()) {  // last one and GET ready, try to reply
                    assert(ret_it->second == NULL);
                    Packet *ret_pkt = curUpBuffers[i];
                    if (inputBuffers[ll]->ReceiveUp(ret_pkt)) {
                      ret_pkt->SRCCUB = cubeID;
                      ret_pkt->DESTCUB = parent_cube;
                      ret_pkt->CMD = ACT_GET;
                      ret_pkt->RTC = 0;
                      ret_pkt->URTC = 0;
                      ret_pkt->DRTC = 0;
                      ret_pkt->chkCRC = 0;
                      ret_pkt->RRP = 0;  // Jiayi, 03/18/17, for retry pointer
                      ret_pkt->chkRRP = false;
                      if (src_addr1 && !src_addr2) {
                        assert(operandEntry.first.first == src_addr1);
                        assert(operandEntry.first.second == false);
                        operandEntry.first.second = true;
                      } else if (!src_addr1 && src_addr2) {
                        assert(operandEntry.second.first == src_addr2);
                        assert(operandEntry.second.second == false);
                        operandEntry.second.second = true;
                      } else {
                        assert("ACT_MULT: both src1 and src2 addr are non zero, ERROR ..." == 0);
                      }
                      assert(operandEntry.first.second && operandEntry.second.second);
#ifdef COMPUTE
                      int *src1 = (int *) operandEntry.first.first;
                      int *src2 = (int *) operandEntry.second.first;
                      int *dest = (int *) dest_addr;
                      int org = reserveTable[dest_addr].first.second;
                      int entry = org + (*src1) * (*src2);
                      int current_dest = *dest;
                      *dest += entry;
                      reserveTable[dest_addr].first.second = entry;
                      cout << "CUBE#" << cubeID << " sends back GET response, update target from " << current_dest
                        << " to " << *dest << endl;
#endif
                      reserveTable[dest_addr].first.first--;
#ifdef DEBUG_UPDATE
#ifdef COMPUTE
                      cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                        << *curUpBuffers[i] << ", update it from " << org << " to " << entry << ", src1 is " << *src1
                        << " (0x" << src1 << "), src2 is " << *src2 << " (0x" << src2
                        << "), remaining count: " << reserveTable[dest_addr].first.first
                        << " ---- send ACT_GET response to parent#" << parent_cube << " as well" << endl;
#else
                      cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << " by "
                        << *curUpBuffers[i] << ", remaining count: " << reserveTable[dest_addr].first.first
                        << " ---- send ACT_GET response to parent#" << parent_cube << " as well" << endl;
#endif
#endif
                      reserveTable.erase(it);
#ifdef DEBUG_FLOW
                      cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): clear flow entry at cube#" << cubeID << endl;
#endif
                      map<uint64_t, vector<bool> >::iterator c_it = childrenTable.find(dest_addr);
                      childrenTable.erase(c_it);
                      // release operand buffers
                      operandEntry.first.first = 0;
                      operandEntry.first.second = false;
                      operandEntry.second.first = 0;
                      operandEntry.second.second = false;
                      freeOperandBufIDs.push_back(operand_buf_id);
                      activeReturnBuffers.erase(ret_it);
                      curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + curUpBuffers[i]->LNG);
                      --i;
                    } else {
                      // no buffers, do nothing, try next cycle
                    }
                  } else {  // GET not ready, reserve an activeReturnBuffer slot, will process when GET comes
                    if (src_addr1 && !src_addr2) {
                      //cout << "CUBE#" << cubeID << " - operand buf id: " << operand_buf_id << " in cube#" << cubeID
                      //  << hex << ", src_addr1: " << src_addr1 << ", src_addr2: " << src_addr2 << ", op addr1: "
                      //  << operandEntry.first.first << ", op addr2: " << operandEntry.second.first << dec << endl;
                      assert(operandEntry.first.first == src_addr1);
                      assert(operandEntry.first.second == false);
                      operandEntry.first.second = true;
                    } else if (!src_addr1 && src_addr2) {
                      assert(operandEntry.second.first == src_addr2);
                      assert(operandEntry.second.second == false);
                      operandEntry.second.second = true;
                    } else {
                      assert("ACT_MULT: both src1 and src2 addr are non zero, ERROR ..." == 0);
                    }
                    assert(operandEntry.first.second && operandEntry.second.second);
#ifdef COMPUTE
                    int *src1 = (int *) operandEntry.first.first;
                    int *src2 = (int *) operandEntry.second.first;
                    int org = reserveTable[dest_addr].first.second;
                    int entry = org + (*src1) * (*src2);
                    reserveTable[dest_addr].first.second = entry;
#endif
                    reserveTable[dest_addr].first.first--;
                    operandEntry.first.first = 0;
                    operandEntry.first.second = false;
                    operandEntry.second.first = 0;
                    operandEntry.second.second = false;
                    freeOperandBufIDs.push_back(operand_buf_id);
#ifdef DEBUG_UPDATE
                    cout << "Packet#" << curUpBuffers[i]->TAG << " returns operand buffer " << operand_buf_id
                      << " in cube#" << cubeID << endl;
#ifdef COMPUTE
                    cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                      << *curUpBuffers[i] << ", update it from " << org << " to " << entry << ", src1 is " << *src1
                      << " (0x" << src1 << "), src2 is " << *src2 << " (0x" << src2
                      << "), remaining count: " << reserveTable[dest_addr].first.first
                      << " ---- put into activeReturnBuffers: ";
#else
                    cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                      << *curUpBuffers[i] << ", remaining count: " << reserveTable[dest_addr].first.first
                      << " ---- put into activeReturnBuffers: ";
#endif
                    cout << "add return packet for cube#" << cubeID << " by " << *curUpBuffers[i] << endl;
#endif
                    activeReturnBuffers.insert(make_pair(dest_addr, curUpBuffers[i]));
                    curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + curUpBuffers[i]->LNG);
                    --i;
                  }
                } else {  // not last one, consume the packet
                  if (src_addr1 && !src_addr2) {
                    assert(operandEntry.first.first == src_addr1);
                    assert(operandEntry.first.second == false);
                    operandEntry.first.second = true;
                  } else if (!src_addr1 && src_addr2) {
                    assert(operandEntry.second.first == src_addr2);
                    assert(operandEntry.second.second == false);
                    operandEntry.second.second = true;
                  } else {
                    assert("ACT_MULT: both src1 and src2 addr are non zero, ERROR ..." == 0);
                  }
                  assert(operandEntry.first.second && operandEntry.second.second);
#ifdef COMPUTE
                  int *src1 = (int *) operandEntry.first.first;
                  int *src2 = (int *) operandEntry.second.first;
                  int org = reserveTable[dest_addr].first.second;
                  int entry = org + (*src1) * (*src2);
                  reserveTable[dest_addr].first.second = entry;
#endif
                  reserveTable[dest_addr].first.first--;
                  operandEntry.first.first = 0;
                  operandEntry.first.second = false;
                  operandEntry.second.first = 0;
                  operandEntry.second.second = false;
                  freeOperandBufIDs.push_back(operand_buf_id);
#ifdef DEBUG_UPDATE
                  cout << "Packet#" << curUpBuffers[i]->TAG << " returns operand buffer " << operand_buf_id
                    << " in cube#" << cubeID << endl;
#ifdef COMPUTE
                  cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                    << *curUpBuffers[i] << ", update it from " << org << " to " << entry << ", src1 is " << *src1
                    << " (0x" << src1 << "), src2 is " << *src2 << " (0x" << src2
                    << "), remaining count: " << reserveTable[dest_addr].first.first << endl;
#else
                  cout << CYCLE() << "Active-Routing: target found in reserve table (cube#" << cubeID << ") by "
                    << *curUpBuffers[i] << ", remaining count: " << reserveTable[dest_addr].first.first << endl;
#endif
#endif
                  int pktLNG = curUpBuffers[i]->LNG;
                  delete curUpBuffers[i]->trace;
                  delete curUpBuffers[i];
                  curUpBuffers.erase(curUpBuffers.begin() + i, curUpBuffers.begin() + i + pktLNG);
                  --i;
                }
              }
            } else if (curUpBuffers[i]->CMD == ACT_GET && curUpBuffers[i]->DESTCUB == cubeID && curUpBuffers[i]->SRCCUB != cubeID) {
              uint64_t dest_addr = curUpBuffers[i]->DESTADRS;
              map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it = reserveTable.find(dest_addr);
              assert(it != reserveTable.end());
              map<uint64_t, vector<bool> >::iterator c_it = childrenTable.find(dest_addr);
              assert(c_it != childrenTable.end());
              int child_link = rf->findNextLink(inServiceLink, cubeID, curUpBuffers[i]->SRCCUB); // used to update childfield
              uint64_t child_count = reserveTable[dest_addr].second.second[child_link];
              uint64_t count = reserveTable[dest_addr].first.first - child_count;
              map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
              assert(ret_it != activeReturnBuffers.end());
              assert(ret_it->second == NULL);
              if (count == 0) {
                int parent_cube = reserveTable[dest_addr].second.first;
                if (inputBuffers[ll]->ReceiveUp(curUpBuffers[i])) {
                  reserveTable[dest_addr].first.first -= child_count;
                  reserveTable[dest_addr].second.second[child_link] = 0;
                  curUpBuffers[i]->SRCCUB = cubeID;
                  curUpBuffers[i]->DESTCUB = parent_cube;
                  curUpBuffers[i]->RTC = 0;
                  curUpBuffers[i]->URTC = 0;
                  curUpBuffers[i]->DRTC = 0;
                  curUpBuffers[i]->chkCRC = 0;
                  curUpBuffers[i]->RRP = 0;
                  curUpBuffers[i]->chkRRP = false;
#ifdef DEBUG_GATHER
                  cout << CYCLE() << "Active-Routing (flow:" << hex << dest_addr << dec << ": (2) return GET result back to parent#"
                    << parent_cube << " from child#" << cubeID << endl;
#endif
#ifdef COMPUTE
                  int *dest = (int *) dest_addr;
                  int current_dest = *dest;
                  *dest += reserveTable[dest_addr].first.second;
                  cout << "CUBE#" << cubeID << " sends back GET response, update target from " << current_dest
                    << " to " << *dest << endl;
#endif
                  activeReturnBuffers.erase(ret_it);
                  reserveTable.erase(it);
#ifdef DEBUG_FLOW
                  cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): clear flow entry at cube#" << cubeID << endl;
#endif
                  childrenTable.erase(c_it);
                  int pktLNG = curUpBuffers[i]->LNG;
                  curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+pktLNG);
                  --i;
                } else {
                  // try next cycle
                }
              } else {  // not last one, update the table and delete it
                reserveTable[dest_addr].first.first -= child_count;
                reserveTable[dest_addr].second.second[child_link] = 0;
                int pktLNG = curUpBuffers[i]->LNG;
                delete curUpBuffers[i]->trace;
                delete curUpBuffers[i];
                curUpBuffers.erase(curUpBuffers.begin()+i, curUpBuffers.begin()+i+pktLNG);
                --i;
              }
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
                    cout << CYCLE() << "cube#" << cubeID << " is sending GET response (flow: " << hex << curUpBuffers[i]->DESTADRS << dec
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
      if(inputBuffers[ll]->bufPopDelay == 0) {
        for(int i=0; i<curDownBuffers.size(); i++) {
          if(curDownBuffers[i] != NULL) {
            assert(curDownBuffers[i]->packetType == REQUEST);
            if (curDownBuffers[i]->DESTCUB == cubeID ||
                (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->SRCADRS1 && curDownBuffers[i]->DESTCUB1 == cubeID) ||
                (curDownBuffers[i]->CMD == ACT_MULT && curDownBuffers[i]->SRCADRS2 && curDownBuffers[i]->DESTCUB2 == cubeID)) {
              //Check request size and the maximum block size
              if(curDownBuffers[i]->reqDataSize > ADDRESS_MAPPING) {
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
                if (curDownBuffers[i]->CMD == ACT_MULT) {  // 03/24/17
                  bool operand_buf_avail = false;
                  bool is_full_pkt = false;
                  // make sure there is free operand buffer
                  uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                  is_full_pkt = (curDownBuffers[i]->SRCADRS1 != 0 && curDownBuffers[i]->SRCADRS2 != 0);
                  if (is_full_pkt) {
                    operand_buf_avail = freeOperandBufIDs.empty() ? false : true;
                    if (operand_buf_avail) {
                      numUpdates++;
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
                        numOperands++;
#ifdef DEBUG_ROUTING
                        cout << "CUBE#" << cubeID << ": Route MULT (" << (pkt->DESTCUB1 == cubeID ? "first" : "second")
                          << ") packet " << curDownBuffers[i]->TAG << " to my VaultCtrl" << endl;
#endif
#ifdef DEBUG_UPDATE
                        cout << "(0) CUBE#" << cubeID << ": split MULT packet " << curDownBuffers[i]->TAG
                          << ", dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
                        //cout << "(0) CUBE#" << cubeID << ": split MULT packet " << curDownBuffers[i]->TAG << " (" << curDownBuffers[i]
                        //  << "), dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
#endif
                        map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
                        it = reserveTable.find(dest_addr);
                        int link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                        int parent_cube = neighborCubeID[link];
                        if (it == reserveTable.end()) {
                          reserveTable.insert(make_pair(dest_addr, make_pair(make_pair(1, 0), make_pair(parent_cube,
                                    vector<int>(NUM_LINKS, 0)))));
#ifdef DEBUG_FLOW
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                          childrenTable.insert(make_pair(dest_addr, vector<bool>(NUM_LINKS, false)));
#ifdef DEBUG_UPDATE
                          cout << "Active-Routing: reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                        } else {
                          assert(it->second.second.first == parent_cube);
                          if (it->second.first.first == 0) {
#ifdef DEBUG_UPDATE
                            cout << "(0 MULT) It has been served for a while at cube#" << cubeID << ", new req " << *pkt
                              << " comes, the old await returned packet is " << *activeReturnBuffers[dest_addr] << endl;
#endif
                            map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                            assert(ret_it != activeReturnBuffers.end() && ret_it->second != NULL);
                            delete ret_it->second->trace;
                            delete ret_it->second;
                            activeReturnBuffers.erase(ret_it);
                          }
                          it->second.first.first++;
                        }
                        int operand_buf_id = freeOperandBufIDs.front();
                        freeOperandBufIDs.pop_front();
                        pair<pair<uint64_t, bool>, pair<uint64_t, bool> > &operandEntry = operandBuffers[operand_buf_id];
                        assert(operandEntry.first.first == 0 && operandEntry.second.first == 0);
                        assert(operandEntry.first.second == false && operandEntry.second.second == false);
                        operandEntry.first.first = curDownBuffers[i]->SRCADRS1;
                        operandEntry.second.first = curDownBuffers[i]->SRCADRS2;
#ifdef DEBUG_UPDATE
                        cout << "Packet#" << curDownBuffers[i]->TAG << " reserves operand buffer " << operand_buf_id
                          << " in cube#" << cubeID << endl;
                        //cout << "Packet#" << curDownBuffers[i]->TAG << " reserves operand buffer " << operand_buf_id
                        //  << " in cube#" << cubeID << ", src_addr1: " << hex << curDownBuffers[i]->SRCADRS1
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
                          cout << "CUBE#" << cubeID << " ReciveDown fails for packet " << curDownBuffers[i]->TAG
                          << " to vault " << vaultMap << endl;
#endif*/
                        //pkt->ReductGlobalTAG();
                        delete pkt;
                      }
                    } else {
                      opbufStalls++;
                      /*#ifdef DEBUG_UPDATE
                        cout << "CUBE#" << cubeID << ": no operand buffers (full), failed for packet " << curDownBuffers[i]->TAG << endl;
#endif*/
                    }
                  } else {
                    if (downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
                      numOperands++;
#ifdef DEBUG_ROUTING
                      cout << "CUBE#" << cubeID << ": Route MULT (second) packet " << curDownBuffers[i]->TAG << " to my VaultCtrl" << endl;
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
                    if (curDownBuffers[j] != NULL && ((curDownBuffers[j]->CMD == ACT_ADD ||
                            curDownBuffers[j]->CMD == ACT_MULT) && curDownBuffers[j]->DESTADRS == dest_addr)) {
                      is_inorder = false;
                      break;
                    }
                  }
#ifdef DEBUG_VERIFY
                  for (int j = i + 1; j < curDownBuffers.size(); j++) {
                    if (curDownBuffers[j] != NULL && (curDownBuffers[j]->CMD == ACT_ADD || curDownBuffers[j]->CMD == ACT_MULT)) {
                      assert(curDownBuffers[j]->DESTADRS != dest_addr);
                    }
                  }
#endif
                  if (!is_inorder) continue;
                  map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
                  it = reserveTable.find(dest_addr);
                  if(it == reserveTable.end()) cout << "HMC#" << cubeID <<" assert for destAddr#"<<dest_addr<< endl;
                  assert(it != reserveTable.end());
                  map<uint64_t, vector<bool> >::iterator c_it = childrenTable.find(dest_addr);
                  assert(c_it != childrenTable.end());
                  vector<int> child_links;
                  for (int c = 0; c < childrenTable[dest_addr].size(); ++c) {
                    if (childrenTable[dest_addr][c] == true) {
                      child_links.push_back(c);
                    }
                  }
                  // send GET to children if any
                  if (child_links.size() > 0) {
                    assert(reserveTable[dest_addr].first.first > 0);
                    map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                    if ((ret_it != activeReturnBuffers.end() && ret_it->second != NULL)) {
                      // process for current router first
                      Packet *pkt = ret_it->second;
                      delete pkt->trace;
                      delete pkt;
                      ret_it->second = NULL;
#ifdef DEBUG_GATHER
                      cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): change return pkt to NULL at cube#"
                        << cubeID << " before replicate to children" << endl;
#endif
                      //--i;
                    } else if (ret_it == activeReturnBuffers.end()) {
                      activeReturnBuffers.insert(make_pair(dest_addr, (Packet *) NULL));
#ifdef DEBUG_GATHER
                      cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an activeReturn entry at cube#"
                        << cubeID << " before replicate to children" << endl;
#endif
                      //--i;
                    }
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
                          << ") from parent#" << cubeID << " to child#" << child_cube << endl;
#endif
                        childrenTable[dest_addr][link] = false;
                        if (child_links.size() == 1) {  // last child
                          int pktLNG = curDownBuffers[i]->LNG;
                          delete curDownBuffers[i];
                          curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                        }
                        --i;
                        break;
                      } else {
                        //child_pkt->ReductGlobalTAG();
                        delete child_pkt;
                      }
                    }
                  } else {
#ifdef DEBUG_GATHER
                    cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): receive GET request at cube#" << cubeID << endl;
#endif
                    map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                    if (ret_it != activeReturnBuffers.end()) {
                      Packet *ret_pkt = activeReturnBuffers[dest_addr];
                      assert(ret_pkt != NULL && (ret_pkt->CMD == ACT_ADD || ret_pkt->CMD == ACT_MULT));
                      int count = reserveTable[dest_addr].first.first;
                      if (count == 0) {
                        if (inputBuffers[ll]->ReceiveUp(ret_pkt)) {
                          int parent_cube = reserveTable[dest_addr].second.first;
                          ret_pkt->SRCCUB = cubeID;
                          ret_pkt->DESTCUB = parent_cube;
                          ret_pkt->CMD = ACT_GET;
                          ret_pkt->RTC = 0;
                          ret_pkt->URTC = 0;
                          ret_pkt->DRTC = 0;
                          ret_pkt->chkCRC = false;
                          ret_pkt->RRP = 0;
                          ret_pkt->chkRRP = false;
                          assert(ret_pkt->packetType == RESPONSE);
#ifdef COMPUTE
                          int *dest = (int *) dest_addr;
                          int current_dest = *dest;
                          *dest += reserveTable[dest_addr].first.second;
                          cout << "CUBE#" << cubeID << " sends back GET response, update target from " << current_dest
                            << " to " << *dest << endl;
#endif
                          int pktLNG = curDownBuffers[i]->LNG;
                          delete curDownBuffers[i];
                          curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                          activeReturnBuffers.erase(ret_it);
                          reserveTable.erase(it);
#ifdef DEBUG_FLOW
                          cout << CYCLE() << "Active-Routing (flow: " << hex << dest_addr << dec << "): clear flow entry at cube#" << cubeID << endl;
#endif
                          childrenTable.erase(c_it);
                          --i;
                        } else {
                          // try next cycle
                        }
                      } else {
#ifdef DEBUG_UPDATE
                        cout << "It is not zero: " << reserveTable[dest_addr].first.first << " at cube#" << cubeID << endl;
#endif
                        // delete the packet and make it NULL indicating GET ready
                        delete ret_pkt->trace;
                        delete ret_pkt;
                        ret_it->second = NULL;
                        int pktLNG = curDownBuffers[i]->LNG;
                        delete curDownBuffers[i];
                        curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                        --i;
                      }
                    } else {
                      // register an activeReturnBuffers entry indicating GET ready
                      assert(reserveTable[dest_addr].first.first != 0);
                      activeReturnBuffers.insert(make_pair(dest_addr, (Packet *) NULL));
                      int pktLNG = curDownBuffers[i]->LNG;
                      delete curDownBuffers[i];
                      curDownBuffers.erase(curDownBuffers.begin()+i, curDownBuffers.begin()+i+pktLNG);
                      --i;
                    }
                  }
                }
                else if(downBufferDest[vaultMap]->ReceiveDown(curDownBuffers[i])) {
#ifdef DEBUG_ROUTING
                  cout << "CUBE#" << cubeID << ": Route packet " << curDownBuffers[i]->TAG << " to my VaultCtrl" << endl;
#endif
                  if (curDownBuffers[i]->CMD == ACT_ADD) {
                    numOperands++;
                    numUpdates++;
                    uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                    map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
                    it = reserveTable.find(dest_addr);
                    int link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                    int parent_cube = neighborCubeID[link];
                    if (it == reserveTable.end()) {
                      reserveTable.insert(make_pair(dest_addr, make_pair(make_pair(1, 0), make_pair(parent_cube,
                                vector<int>(NUM_LINKS, 0)))));
#ifdef DEBUG_FLOW
                      cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                      childrenTable.insert(make_pair(dest_addr, vector<bool>(NUM_LINKS, false)));
#ifdef DEBUG_UPDATE
                      cout << "Active-Routing: reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                    } else {
                      assert(it->second.second.first == parent_cube);
                      if (it->second.first.first == 0) {
#ifdef DEBUG_UPDATE
                        cout << "(0 ADD) It has been served for a while at cube#" << cubeID << ", new req " << *curDownBuffers[i]
                          << " comes, the old await returned packet is " << *activeReturnBuffers[dest_addr] << endl;
#endif
                        map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                        assert(ret_it != activeReturnBuffers.end() && ret_it->second != NULL);
                        activeReturnBuffers.erase(ret_it);
                      }
                      it->second.first.first++;
                    }
                    curDownBuffers[i]->SRCCUB = cubeID;
                    curDownBuffers[i]->DESTCUB = cubeID;
                  }
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
                      numUpdates++;
                      assert(curDownBuffers[i]->operandBufID == -1);
                      Packet *pkt = new Packet(*curDownBuffers[i]);
                      if (upBufferDest[link1]->currentState != LINK_RETRY && upBufferDest[link1]->ReceiveDown(pkt)) {
                        pkt->RTC = 0;
                        pkt->URTC = 0;
                        pkt->DRTC = 0;
                        pkt->chkCRC = false;
                        pkt->RRP = 0;
                        pkt->chkRRP = false;
                        pkt->SRCADRS2 = 0;
#ifdef DEBUG_UPDATE
                        cout << "(1) CUBE#" << cubeID << ": split MULT packet " << curDownBuffers[i]->TAG
                          << ", dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
                        //cout << "(1) CUBE#" << cubeID << ": split MULT packet " << curDownBuffers[i]->TAG << " (" << curDownBuffers[i]
                        //  << "), dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
#endif
                        map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it =
                          reserveTable.find(dest_addr);
                        int parent_link = rf->findNextLink(inServiceLink, cubeID, pkt->SRCCUB, true);
                        int parent_cube = neighborCubeID[parent_link];
                        if (it == reserveTable.end()) {
                          reserveTable.insert(make_pair(dest_addr, make_pair(make_pair(1, 0), make_pair(parent_cube,
                                    vector<int>(NUM_LINKS, 0)))));
#ifdef DEBUG_FLOW
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                          childrenTable.insert(make_pair(dest_addr, vector<bool>(NUM_LINKS, false)));
#ifdef DEBUG_UPDATE
                          cout << "Active-Routing: reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                        } else {
                          assert(it->second.second.first == parent_cube);
                          if (it->second.first.first == 0) {
#ifdef DEBUG_UPDATE
                            cout << "(2 MULT) It has been served for a while at cube#" << cubeID << ", new req " << *pkt
                              << " comes, the old await returned packet is " << *activeReturnBuffers[dest_addr] << endl;
#endif
                            map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                            assert(ret_it != activeReturnBuffers.end() && ret_it->second != NULL);
                            delete ret_it->second->trace;
                            delete ret_it->second;
                            activeReturnBuffers.erase(ret_it);
                          }
                          it->second.first.first++;//no need to increment counter, has increased in curDownBuffers
                        }
                        int operand_buf_id = freeOperandBufIDs.front();
                        freeOperandBufIDs.pop_front();
                        pair<pair<uint64_t, bool>, pair<uint64_t, bool> > &operandEntry =
                          operandBuffers[operand_buf_id];
                        assert(operandEntry.first.first == 0 && operandEntry.second.first == 0);
                        assert(operandEntry.first.second == false && operandEntry.second.second == false);
                        operandEntry.first.first = curDownBuffers[i]->SRCADRS1;
                        operandEntry.second.first = curDownBuffers[i]->SRCADRS2;
#ifdef DEBUG_UPDATE
                        cout << "Packet#" << pkt->TAG << " reserves operand buffer " << operand_buf_id << " in cube#"
                          << cubeID << endl;
                        //cout << "Packet#" << pkt->TAG << " reserves operand buffer " << operand_buf_id << " in cube#"
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
                        pkt->RTC = 0;
                        pkt->URTC = 0;
                        pkt->DRTC = 0;
                        pkt->chkCRC = false;
                        pkt->RRP = 0;
                        pkt->chkRRP = false;
                        pkt->SRCADRS1 = 0;
                        pkt->DESTCUB = pkt->DESTCUB2;
#ifdef DEBUG_UPDATE
                        cout << "(2) CUBE#" << cubeID << ": split MULT packet " << curDownBuffers[i]->TAG
                          << ", dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
                        //cout << "(2) CUBE#" << cubeID << ": split MULT packet " << curDownBuffers[i]->TAG << " (" << curDownBuffers[i]
                        //  << "), dest_cube1: " << curDownBuffers[i]->DESTCUB1 << ", dest_cube2: " << curDownBuffers[i]->DESTCUB2 << endl;
#endif
                        map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it =
                          reserveTable.find(dest_addr);
                        int link = rf->findNextLink(inServiceLink, cubeID, pkt->SRCCUB, true);
                        int parent_cube = neighborCubeID[link];
                        if (it == reserveTable.end()) {
                          reserveTable.insert(make_pair(dest_addr, make_pair(make_pair(1, 0), make_pair(parent_cube,
                                    vector<int>(NUM_LINKS, 0)))));
#ifdef DEBUG_FLOW
                          cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                          childrenTable.insert(make_pair(dest_addr, vector<bool>(NUM_LINKS, false)));
#ifdef DEBUG_UPDATE
                          cout << "Active-Routing: reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                        } else {
                          assert(it->second.second.first == parent_cube);
                          if (it->second.first.first == 0) {
#ifdef DEBUG_UPDATE
                            cout << "(3 MULT) It has been served for a while at cube#" << cubeID << ", new req " << *pkt
                              << " comes, the old await returned packet is " << *activeReturnBuffers[dest_addr] << endl;
#endif
                            map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                            assert(ret_it != activeReturnBuffers.end() && ret_it->second != NULL);
                            delete ret_it->second->trace;
                            delete ret_it->second;
                            activeReturnBuffers.erase(ret_it);
                          }
                          it->second.first.first++;//no need to increment counter, has increased in curDownBuffers
                        }
                        int operand_buf_id = freeOperandBufIDs.front();
                        freeOperandBufIDs.pop_front();
                        pair<pair<uint64_t, bool>, pair<uint64_t, bool> > &operandEntry =
                          operandBuffers[operand_buf_id];
                        assert(operandEntry.first.first == 0 && operandEntry.second.first == 0);
                        assert(operandEntry.first.second == false && operandEntry.second.second == false);
                        operandEntry.first.first = curDownBuffers[i]->SRCADRS1;
                        operandEntry.second.first = curDownBuffers[i]->SRCADRS2;
#ifdef DEBUG_UPDATE
                        cout << "Packet#" << pkt->TAG << " reserves operand buffer " << operand_buf_id << " in cube#"
                          << cubeID << endl;
                        //cout << "Packet#" << pkt->TAG << " reserves operand buffer " << operand_buf_id << " in cube#"
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
                          cout << "CUBE#" << cubeID << " downBufferDest Receive fails for packet " << pkt->TAG
                          << " to either link1 " << link1 << " (cube#" << neighborCubeID[link1]
                          << ") or link2 " << link2 << " (cube#" << neighborCubeID[link2] << ")" << endl;
#endif*/
                        delete pkt; // try next time
                      }
                    } else {  // operand buffer not available
                      opbufStalls++;
                      /*#ifdef DEBUG_UPDATE
                        cout << "CUBE#" << cubeID << ": no operand buffers (full), failed for packet " << curDownBuffers[i]->TAG << endl;
#endif*/
                    }
                  } else { // no need for spliting
                    if (upBufferDest[link]->currentState != LINK_RETRY && upBufferDest[link]->ReceiveDown(curDownBuffers[i])) {
                      map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
                      it = reserveTable.find(dest_addr);
                      int parent_link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                      int parent_cube = neighborCubeID[parent_link];
                      if (it == reserveTable.end()) {
                        reserveTable.insert(make_pair(dest_addr, make_pair(make_pair(1, 0), make_pair(parent_cube,
                                  vector<int>(NUM_LINKS, 0)))));
#ifdef DEBUG_FLOW
                        cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                        childrenTable.insert(make_pair(dest_addr, vector<bool>(NUM_LINKS, false)));
#ifdef DEBUG_UPDATE
                        cout << "Active-Routing: reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                      } else {
                        assert(it->second.second.first == parent_cube);
                        if (it->second.first.first == 0) {
#ifdef DEBUG_UPDATE
                          cout << "(1 MULT)  It has been served for a while at cube#" << cubeID << ", new req " << *curDownBuffers[i]
                            << " comes, the old await returned packet is " << *activeReturnBuffers[dest_addr] << endl;
#endif
                          map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                          assert(ret_it != activeReturnBuffers.end() && ret_it->second != NULL);
                          activeReturnBuffers.erase(ret_it);
                        }
                        it->second.first.first++;
                      }
                      curDownBuffers[i]->RTC = 0;
                      curDownBuffers[i]->URTC = 0;
                      curDownBuffers[i]->DRTC = 0;
                      curDownBuffers[i]->chkCRC = false;
                      curDownBuffers[i]->RRP = 0;
                      curDownBuffers[i]->chkRRP = false;
#ifdef DEBUG_ROUTING
                      cout << CYCLE() << "Active-Routing: route packet from cube#" << cubeID << " to cube#" <<
                        curDownBuffers[i]->DESTCUB << ", next hop is cube#" << neighborCubeID[link] << endl;
#endif
                      reserveTable[dest_addr].second.second[link]++;
                      childrenTable[dest_addr][link] = true;
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
                    cout << CYCLE() << "Active-Routing: route packet from cube#" << cubeID << " to cube#" <<
                      curDownBuffers[i]->DESTCUB << ", next hop is cube#" << neighborCubeID[link] << endl;
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
                  if (curDownBuffers[i]->CMD == ACT_ADD) {
                    assert(curDownBuffers[i]->packetType == REQUEST);
                    uint64_t dest_addr = curDownBuffers[i]->DESTADRS;
                    map<uint64_t, pair<pair<unsigned, int>, pair<int, vector<int> > > >::iterator it;
                    it = reserveTable.find(dest_addr);
                    int parent_link = rf->findNextLink(inServiceLink, cubeID, curDownBuffers[i]->SRCCUB, true);
                    int parent_cube = neighborCubeID[parent_link];
                    if (it == reserveTable.end()) {
                      reserveTable.insert(make_pair(dest_addr, make_pair(make_pair(1, 0), make_pair(parent_cube,
                                vector<int>(NUM_LINKS, 0)))));
#ifdef DEBUG_FLOW
                      cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                      childrenTable.insert(make_pair(dest_addr, vector<bool>(NUM_LINKS, false)));
#ifdef DEBUG_UPDATE
                      cout << "Active-Routing: reserve an entry for Active target at cube#" << cubeID << endl;
#endif
                    } else {
                      assert(it->second.second.first == parent_cube);
                      if (it->second.first.first == 0) {
#ifdef DEBUG_UPDATE
                        cout << "(1 ADD)  It has been served for a while at cube#" << cubeID << ", new req " << *curDownBuffers[i]
                          << " comes, the old await returned packet is " << *activeReturnBuffers[dest_addr] << endl;
#endif
                        map<uint64_t, Packet *>::iterator ret_it = activeReturnBuffers.find(dest_addr);
                        assert(ret_it != activeReturnBuffers.end() && ret_it->second != NULL);
                        activeReturnBuffers.erase(ret_it);
                      }
                      it->second.first.first++;
                    }
                    reserveTable[dest_addr].second.second[link]++;
                    childrenTable[dest_addr][link] = true;
                  }
                  curDownBuffers[i]->RRP = 0;  // Jiayi, 03/18/17, for retry pointer
                  curDownBuffers[i]->chkRRP = false;
                  curDownBuffers[i]->RTC = 0; //return Tokens from previous transactions should be cleared, Ideally this should be done at linkMaster
                  curDownBuffers[i]->URTC = 0;
                  curDownBuffers[i]->DRTC = 0;
                  //as soon as the RTC of the master is updated in UpdateToken function.
                  curDownBuffers[i]->chkCRC = false; //Make the packet is ready for further transmision on other links. Not very clear, but it works for now!!
#ifdef DEBUG_ROUTING
                  cout << CYCLE() << "Active-Routing: route packet " << curDownBuffers[i]->TAG << " from cube#" << cubeID
                    << " to cube#" << curDownBuffers[i]->DESTCUB << ", next hop is cube#" << neighborCubeID[link] << endl;
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
    cout << " -- downBuffers: (size: " << downBuffers.size() << ")" << endl;
    for (int i = 0; i < downBuffers.size(); i++) {
      if (downBuffers[i] != NULL) {
        int link = rf->findNextLink(inServiceLink, cubeID, downBuffers[i]->DESTCUB);
        int next_cube = neighborCubeID[link];
        cout << (downBuffers[i]->packetType == REQUEST ? "    Request " : "    Response ")
          << *downBuffers[i] << " from current cube " << cubeID << " to next cube " << next_cube
          << " (src_cube: " << downBuffers[i]->SRCCUB << ", dest_cube: " << downBuffers[i]->DESTCUB
          << ", packet length: " << downBuffers[i]->LNG;
        if (downBuffers[i]->CMD == ACT_MULT) {
          if (downBuffers[i]->SRCADRS1 && downBuffers[i]->SRCADRS2) {
            cout << ", full pkt, dest_cube1: " << downBuffers[i]->DESTCUB1 << ", dest_cube: " << downBuffers[i]->DESTCUB2
              << ")" << endl;
          } else if (downBuffers[i]->SRCADRS1 && !downBuffers[i]->SRCADRS2) {
            cout << ", first operand pkt)" << endl;
          } else {
            cout <<", second operand pkt)" << endl;
          }
        } else {
          cout << ")" << endl;
        }
      }
    }
    cout << " -- upBuffers: (size: " << upBuffers.size() << ")" << endl;
    for (int i = 0; i < upBuffers.size(); i++) {
      if (upBuffers[i] != NULL) {
        int link = rf->findNextLink(inServiceLink, cubeID, upBuffers[i]->DESTCUB);
        int next_cube = neighborCubeID[link];
        cout << (upBuffers[i]->packetType == REQUEST ? "    Request " : "    Response ")
          << *upBuffers[i] << " from current cube " << cubeID << " to next cube " << next_cube
          << " (src_cube: " << upBuffers[i]->SRCCUB << ", dest_cube: " << upBuffers[i]->DESTCUB
          << ", packet length: " << upBuffers[i]->LNG;
        if (upBuffers[i]->CMD == ACT_MULT) {
          if (upBuffers[i]->SRCADRS1 && upBuffers[i]->SRCADRS2) {
            cout << ", full pkt, dest_cube1: " << upBuffers[i]->DESTCUB1 << ", dest_cube2: " << upBuffers[i]->DESTCUB2
              << ")" << endl;
          } else if (upBuffers[i]->SRCADRS1 && !upBuffers[i]->SRCADRS2) {
            cout << ", first operand pkt)" << endl;
          } else {
            cout <<", second operand pkt)" << endl;
          }
        } else {
          cout << ")" << endl;
        }
      }
    }
    cout << " -- LinkMaster token counts:" << endl;
    for (int i = 0; i < upBufferDest.size(); i++) {
      cout << "    linkMaster " << i << ": utk( " << upBufferDest[i]->upTokenCount
        << " ) - dtk( " << upBufferDest[i]->downTokenCount << " )" << endl;
    }
    for (int i = 0; i < downBufferDest.size(); i++) {
      VaultController *vault = dynamic_cast<VaultController *> (downBufferDest[i]);
      assert(vault);
      vault->PrintBuffers();
    }
  }

} //namespace CasHMC
