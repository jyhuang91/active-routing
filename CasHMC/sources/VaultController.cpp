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

#include <assert.h>   // Jiayi, for debugging

#include "VaultController.h"
#include "CrossbarSwitch.h"

unsigned CasHMC::VaultController::DRAM_rd_data = 0;
unsigned CasHMC::VaultController::DRAM_wr_data = 0;
unsigned CasHMC::VaultController::DRAM_act_data = 0;

namespace CasHMC
{

  VaultController::VaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_VLT_BUF, MAX_VLT_BUF),
    vaultContID(id), operandBufSize(MAX_OPERAND_BUF),
    numMultStages(5), multPipeOccupancy(0)
  {
    classID << vaultContID;
    header = "        (VC_" + classID.str() + ")";

    refreshCountdown = 0;
    powerDown = false;
    poppedCMD = NULL;
    atomicCMD = NULL;
    atomicOperLeft = 0;
    pendingDataSize = 0;

    cmdBus = NULL;
    cmdCyclesLeft = 0;
    dataBus = NULL;
    dataCyclesLeft = 0;

    //Make class objects
    commandQueue = new CommandQueue(debugOut, stateOut, this, vaultContID);

    // For vault-level parallelism:
    operandBuffers.resize(operandBufSize, VaultOperandEntry(numMultStages));
    for (int i = 0; i < operandBufSize; i++) {
      freeOperandBufIDs.push_back(i);
    }
    numADDUpdates = 0;
    numOperands = 0;
    opbufStalls = 0;
    numFlows = 0;
    numAdds = 0;
    numMults = 0;
    cubeID = -1;
    // Update Counts for MULTs:
    numLocalReqRecv = 0;
    numLocalRespRecv = 0;
    numRemoteReqRecv = 0;
    numFlowRespSent = 0;
    numRemoteRespSent = 0;
  }

  VaultController::VaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id, string headerPrefix):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_VLT_BUF, MAX_VLT_BUF),
    vaultContID(id), operandBufSize(MAX_OPERAND_BUF),
    numMultStages(5), multPipeOccupancy(0)
  {
    classID << vaultContID;
    header = "        (" + headerPrefix + "_VC_" + classID.str() + ")";

    refreshCountdown = 0;
    powerDown = false;
    poppedCMD = NULL;
    atomicCMD = NULL;
    atomicOperLeft = 0;
    pendingDataSize = 0;

    cmdBus = NULL;
    cmdCyclesLeft = 0;
    dataBus = NULL;
    dataCyclesLeft = 0;

    //Make class objects
    commandQueue = new CommandQueue(debugOut, stateOut, this, vaultContID);

    // For vault-level parallelism:
    operandBuffers.resize(operandBufSize, VaultOperandEntry(numMultStages));
    for (int i = 0; i < operandBufSize; i++) {
      freeOperandBufIDs.push_back(i);
    }
    numADDUpdates = 0;
    numOperands = 0;
    opbufStalls = 0;
    numFlows = 0;
    numAdds = 0;
    numMults = 0;
    cubeID = -1;
    // Update Counts for MULTs:
    numLocalReqRecv = 0;
    numLocalRespRecv = 0;
    numRemoteReqRecv = 0;
    numFlowRespSent = 0;
    numRemoteRespSent = 0;
  }

  VaultController::~VaultController()
  {
    pendingReadData.clear();
    writeDataToSend.clear();
    writeDataCountdown.clear();

    // Debugging Vault-Level Parallelism:
#ifdef DEBUG_VAULT
    if (numAdds > 0)
      cout << "VC " << vaultContID << " CUBE " << cubeID << ", " << numAdds << " ADDs:" << endl 
        << "\t" << numADDUpdates << " Updates" << endl
        << "\t" << numFlowRespSent << " Flow Responses " << endl
        << "\t" << numFlows << " Flows" << endl;
        

    if (numMults > 0)
      cout << "VC " << vaultContID << " CUBE " << cubeID << ", " << numMults << " MULTs:" << endl 
        << "\t" << numLocalReqRecv << " Local Requests Received" << endl 
        << "\t" << numLocalRespRecv << " Local Responses Received" << endl 
        << "\t" << numRemoteReqRecv << " Remote Requests Received" << endl
        << "\t" << numRemoteRespSent << " Remote Responses Sent" << endl
        << "\t" << numFlowRespSent << " Flow Responses Sent" << endl 
        << "\t" << numFlows << " Flows" << endl;

    for (int i = 0; i < operandBuffers.size(); i++)
      if (operandBuffers[i].flowID != 0)
        cout << "Error: For VC " << vaultContID << " CUBE " << cubeID << " operand entry " << i << "still in use" << endl;


    map<FlowID, VaultFlowEntry>::iterator iter = flowTable.begin();
    while (iter != flowTable.end()) {
      FlowID flowID = iter->first;
      cout << "Error: For VC " << vaultContID << " CUBE " << cubeID << " found flow table still has req_count " << iter->second.req_count << " and rep_count " << iter->second.rep_count << " and g_flag " << iter->second.g_flag << endl;
      iter++;
    }
#endif

    // For vault-level parallelism:
    flowTable.clear();
    operandBuffers.clear(); // Jiayi, 03/24/17
    freeOperandBufIDs.clear();

    delete commandQueue;
  }

  //
  //Callback receiving packet result
  //
  void VaultController::CallbackReceiveDown(Packet *downEle, bool chkReceive)
  {
    /*	if(chkReceive) {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
        }
        else {
        DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
        }*/
  }

  void VaultController::CallbackReceiveUp(Packet *upEle, bool chkReceive)
  {
    if(chkReceive) {
      switch(upEle->CMD) {
        case RD_RS:	DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETURNING read data response packet");	break;
        case WR_RS:	DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETURNING write response packet");		break;
        case ACT_ADD:	DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETURNING active_add response packet");		break;
        case ACT_MULT: DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETRUNING active mult response packet");  break;
        case ACT_DOT:	DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETURNING active_dot response packet");		break;
        case ACT_GET:	DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETURNING active_get response packet");		break;
        case PEI_DOT: DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETRUNING pei dot response packet");  break;
        case PEI_ATOMIC: DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETRUNING pei atomic response packet");  break;
        default:
                    ERROR(header<<"  == Error - WRONG response packet command type  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
                    exit(0);
                    break;
      }
    }
    else {
      WARN(header<<"  == Error - Vault controller upstream packet buffer FULL  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
      WARN(header<<"             Vault buffer max size : "<<upBufferMax<<", current size : "<<upBuffers.size()<<", "<<*upEle<<" size : "<<upEle->LNG);
      //ERROR(header<<"  == Error - Vault controller upstream packet buffer FULL  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
      //ERROR(header<<"             Vault buffer max size : "<<upBufferMax<<", current size : "<<upBuffers.size()<<", "<<*upEle<<" size : "<<upEle->LNG);
      //exit(0);
    }
  }

  //
  //Return read commands from DRAM
  //
  bool VaultController::ReturnCommand(DRAMCommand *retCMD)
  {
    //Read and write data commands share one data bus
    if(dataBus != NULL) {
      ERROR(header<<"  == Error - Data Bus Collision  (dataBus:"<<*dataBus<<", retCMD:"<<*retCMD<<")  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }

    bool consumeRetCMD = true;
    bool foundMatch = false;
    for(int i=0; i<pendingReadData.size(); i++) {
      if(retCMD->packetTAG == pendingReadData[i]) {
        if(retCMD->lastCMD == true) {
          if(retCMD->atomic) {
            atomicCMD = retCMD;
            atomicOperLeft = 1;
            DE_CR(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   RETURNING ATOMIC read data");
          }
          else {
            if (MakeRespondPacket(retCMD))
              delete retCMD;
            else
              consumeRetCMD = false;
          }
        }
        else {
          delete retCMD;
        }
        if (consumeRetCMD)
          pendingReadData.erase(pendingReadData.begin()+i);
        foundMatch = true;
        break;
      }
    }
    if(!foundMatch) {
      ERROR(header<<"  == Error - Can't find a matching transaction  "<<*retCMD<<" 0x"<<hex<<retCMD->packetTAG<<dec<<"  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }

    return consumeRetCMD;
  }

  //
  //Make response packet from request
  //
  bool VaultController::MakeRespondPacket(DRAMCommand *retCMD)
  {
    if(retCMD->packetCMD == ACT_ADD || retCMD->packetCMD == ACT_DOT) {
#ifdef DEBUG_VAULT
      cout << "VC " << vaultContID << " CUBE " << cubeID << " got a local ADD/DOT request" << endl;
#endif
      // Search for the operand entry and mark as ready
      int voperandID = retCMD->vaultOperandBufID;
      VaultOperandEntry &operandEntry = operandBuffers[voperandID];
      operandEntry.op1_ready = true;
      operandEntry.ready = true;
      pendingDataSize -= (retCMD->dataSize/16)+1;
      if(retCMD->trace != NULL) {
        retCMD->trace->vaultFullLat = currentClockCycle - retCMD->trace->vaultIssueTime;
      }
      return true;   // Don't actually send a response yet
    }
    else if (retCMD->packetCMD == ACT_MULT && retCMD->src_cube == cubeID && retCMD->computeVault == vaultContID) {
#ifdef DEBUG_VAULT
      cout << "VC " << vaultContID << " CUBE " << cubeID << " got a local MULT request" << endl;
      numLocalReqRecv++;
#endif
      int voperandID = retCMD->vaultOperandBufID;
      VaultOperandEntry &operandEntry = operandBuffers[voperandID];
      if (retCMD->srcAddr1 == 0) {
        assert(retCMD->srcAddr2 != 0);
        operandEntry.op2_ready = true;
      } else {
        assert(retCMD->srcAddr1 != 0 && retCMD->srcAddr2 == 0);
        operandEntry.op1_ready = true;
      }
      if (operandEntry.op1_ready && operandEntry.op2_ready) {
        FlowID flowID = operandEntry.flowID;
        assert(flowTable.find(flowID) != flowTable.end());
        VaultFlowEntry &flowEntry = flowTable[flowID];
        operandEntry.ready = true;
#ifdef DEBUG_VAULT
        cout << "VC " << vaultContID << " CUBE " << cubeID << " local request made operand entry #" << voperandID << " ready" << endl;
        numMults++;
#endif
      }
      pendingDataSize -= (retCMD->dataSize/16)+1;
      if(retCMD->trace != NULL) {
        retCMD->trace->vaultFullLat = currentClockCycle - retCMD->trace->vaultIssueTime;
      }
      return true;   // When computation vault is here, don't send a response packet
    }

    bool consumeRetCMD = true;
    unsigned newPendingDataSize = pendingDataSize;

    Packet *newPacket;
    if(retCMD->atomic) {
      if(retCMD->packetCMD == _2ADD8 || retCMD->packetCMD == ADD16 || retCMD->packetCMD == INC8
          || retCMD->packetCMD == EQ8 || retCMD->packetCMD == EQ16 || retCMD->packetCMD == BWR) {
        newPacket = new Packet(RESPONSE, WR_RS, retCMD->packetTAG, 1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        newPendingDataSize -= 1;
      }
      else if (retCMD->packetCMD == PEI_ATOMIC) {
        newPacket = new Packet(RESPONSE, PEI_ATOMIC, retCMD->packetTAG, 1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        newPendingDataSize -= 1;
      }
      else {
        newPacket = new Packet(RESPONSE, RD_RS, retCMD->packetTAG, 2, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        newPendingDataSize -= 2;
      }
    }
    else {
      if(retCMD->commandType == WRITE_DATA) {
        //packet, cmd, tag, lng, *lat
        newPacket = new Packet(RESPONSE, WR_RS, retCMD->packetTAG, 1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        newPendingDataSize -= 1;
        //DEBUG(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   newPendingDataSize 1 decreased   (current newPendingDataSize : "<<newPendingDataSize<<")");
      }
      else if(retCMD->commandType == READ_DATA) {
        //packet, cmd, tag, lng, *lat
        newPacket = new Packet(RESPONSE, RD_RS, retCMD->packetTAG, (retCMD->dataSize/16)+1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        newPendingDataSize -= (retCMD->dataSize/16)+1;
        //DEBUG(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   newPendingDataSize "<<(retCMD->dataSize/16)+1<<" decreased   (current newPendingDataSize : "<<newPendingDataSize<<")");
        newPacket->ADRS = retCMD->addr; // Jiayi, 03/27/17

        if (retCMD->packetCMD == ACT_ADD ||
            retCMD->packetCMD == ACT_DOT) {
          newPacket->CMD = (retCMD->packetCMD == ACT_ADD ? ACT_ADD : ACT_DOT);
          newPacket->active = true;
          newPacket->DESTADRS = retCMD->destAddr;
          newPacket->SRCADRS1 = retCMD->srcAddr1;
          newPacket->computeVault = retCMD->computeVault;
          newPacket->operandBufID = retCMD->operandBufID;
          newPacket->vaultOperandBufID = retCMD->vaultOperandBufID;
          //newPacket->LNG = 2; // comment it, LNG is calculated from dataSize/16+1
#if defined(DEBUG_UPDATE) || defined(DEBUG_VAULT)
          cout << "Active ADD packet " << *newPacket << " is returned for (ADD) operand addr " << hex << newPacket->SRCADRS1 << dec << endl;
#endif

        } else if(retCMD->packetCMD == PEI_DOT) {
          //newPacket->LNG = 2;
          newPacket->CMD = PEI_DOT;
        }
        else if (retCMD->packetCMD == ACT_MULT) { // 03/24/17
          newPacket->CMD = ACT_MULT;
          //newPacket->SRCCUB = retCMD->dest_cube;
          //newPacket->DESTCUB = retCMD->src_cube;
          newPacket->active = true;
          newPacket->DESTADRS = retCMD->destAddr;
          assert((retCMD->srcAddr1 && !retCMD->srcAddr2) || (!retCMD->srcAddr1 && retCMD->srcAddr2));
          newPacket->SRCADRS1 = retCMD->srcAddr1;
          newPacket->SRCADRS2 = retCMD->srcAddr2;
          newPacket->operandBufID = retCMD->operandBufID;
          newPacket->vaultOperandBufID = retCMD->vaultOperandBufID;
          newPacket->computeVault = retCMD->computeVault;
          //newPacket->LNG = 2;//TODO
#ifdef DEBUG_UPDATE
          cout << CYCLE() << "Active-Routing: Active MULT packet " << newPacket->TAG;
          if (newPacket->SRCADRS1) {
            assert(newPacket->SRCADRS2 == 0);
            cout << " for operand 1 is returned, src_cube: " << newPacket->SRCCUB << ", dest_cube: " << newPacket->DESTCUB
              << " (srcAddr1: 0x" << hex << newPacket->SRCADRS1
              << ", srcAddr2: 0x" << newPacket->SRCADRS2 << ")" << dec << endl;
          } else {
            assert(newPacket->SRCADRS1 == 0 && newPacket->SRCADRS2);
            cout << " for operand 2 is returned, src_cube: " << newPacket->SRCCUB << ", dest_cube: " << newPacket->DESTCUB
              << " (srcAddr1: 0x" << hex << newPacket->SRCADRS1
              << ", srcAddr2: 0x" << newPacket->SRCADRS2 << ")" << dec << endl;
          }
#endif
        }
      }
      else {
        ERROR(header<<"  == Error - Unknown response packet command  cmd : "<<retCMD->commandType<<"  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
    }
    newPacket->ADRS = (retCMD->addr << 30) >> 30;
    newPacket->orig_addr = retCMD->addr;
    newPacket->tran_tag = retCMD->tran_tag;
    newPacket->segment = retCMD->segment;
    if (newPacket->CMD != PEI_DOT) {
      consumeRetCMD = ReceiveUp(newPacket);
      if (newPacket->CMD == ACT_MULT) {
#ifdef DEBUG_VAULT
        cout << "VC " << vaultContID << " CUBE " << cubeID << " sent a remote response" << endl;
        numRemoteRespSent++;
#endif
      }
    } else {
      if (pcuPacket.empty()) {
        newPacket->bufPopDelay = PCU_DELAY;
      } else {
        newPacket->bufPopDelay = max(PCU_DELAY,(pcuPacket.back())->bufPopDelay + 1);
      }
      pcuPacket.push_back(newPacket);
    }

    if (consumeRetCMD == true) {
      if(retCMD->trace != NULL) {
        retCMD->trace->vaultFullLat = currentClockCycle - retCMD->trace->vaultIssueTime;
      }
      pendingDataSize = newPendingDataSize;
#ifdef DEBUG_UPDATE
      if (retCMD->packetCMD == ACT_ADD ||
          retCMD->packetCMD == ACT_DOT)
      {
        cout << CYCLE() << "ART: Active response packet " << *newPacket
          << " is returned for operand addr " << hex << newPacket->SRCADRS1 << dec << endl;
      }
      else if(retCMD->packetCMD == PEI_DOT)
      {
        cout << CYCLE() << "PEI response packet " << *newPacket
          << " is returned for operand addr " << hex << newPacket->SRCADRS1 << dec << endl;
      }
      else if (retCMD->packetCMD == ACT_MULT)
      {
        cout << CYCLE() << "ART: Active MULT packet " << newPacket->TAG;
        if (newPacket->SRCADRS1) {
          assert(newPacket->SRCADRS2 == 0);
          cout << " for operand 1 is returned, src_cube: " << newPacket->SRCCUB << ", dest_cube: " << newPacket->DESTCUB
            << " (srcAddr1: 0x" << hex << newPacket->SRCADRS1
            << ", srcAddr2: 0x" << newPacket->SRCADRS2 << ")" << dec << endl;
        } else {
          assert(newPacket->SRCADRS1 == 0 && newPacket->SRCADRS2);
          cout << " for operand 2 is returned, src_cube: " << newPacket->SRCCUB << ", dest_cube: " << newPacket->DESTCUB
            << " (srcAddr1: 0x" << hex << newPacket->SRCADRS1
            << ", srcAddr2: 0x" << newPacket->SRCADRS2 << ")" << dec << endl;
        }
      }
#endif
    } else {
      delete newPacket;
    }

    return consumeRetCMD;
  }

  //
  //Update the state of vault controller
  //
  void VaultController::Update()
  {
    if (cubeID == -1) {
      InputBuffer *ibuf = dynamic_cast<InputBuffer *> (upBufferDest);
      assert(ibuf);
      CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (ibuf->xbar);
      cubeID = xbar->cubeID;
    }
    if(!pcuPacket.empty() && (pcuPacket.front())->bufPopDelay == 0){
      if (ReceiveUp(pcuPacket.front()))
        pcuPacket.erase(pcuPacket.begin());
    }

    // Free available operand buffer entries and commit ready flows:
    // Active-Routing processing
    // 1) consume available operands and free operand buffer
    bool startedMult = false;
    for (int i = 0; i < operandBuffers.size(); i++) {
      VaultOperandEntry &operandEntry = operandBuffers[i];
      if (operandEntry.ready) {
        FlowID flowID = operandEntry.flowID;
        assert(flowTable.find(flowID) != flowTable.end());
        VaultFlowEntry &flowEntry = flowTable[flowID];
        //cout << CYCLE() << "VC " << vaultContID << " CUBE " << cubeID << " Current Occupancy: " << multPipeOccupancy << endl;
        if (flowEntry.opcode == MAC) {
          if (operandEntry.multStageCounter == numMultStages) {
            if (!startedMult && multPipeOccupancy < numMultStages) {
              //cout << CYCLE() << "VC " << vaultContID << " CUBE " << cubeID << " Starting MULT for operand " << i << endl;
              startedMult = true;
              multPipeOccupancy++;
              operandEntry.multStageCounter--;
            }
            // Otherwise wait to start until pipeline is not full
            continue; // don't free the buffer
          } else {
            //cout << CYCLE() << "VC " << vaultContID << " CUBE " << cubeID << " Moving MULT operand in stage " << (int) operandEntry.multStageCounter << endl;
            operandEntry.multStageCounter--;
            if (operandEntry.multStageCounter > 0)
              continue;
            else
              multPipeOccupancy--;
          }
        }
        //cout << CYCLE() << "VC " << vaultContID << " CUBE " << cubeID << " Finished MULT from operand  " << i << endl;
        flowEntry.rep_count++;
#ifdef COMPUTE
        int org_res, new_res;
        if (flowEntry.opcode == ADD) {
          assert(operandEntry.src_addr1 && operandEntry.src_addr2 == 0 &&
              operandEntry.op1_ready && !operandEntry.op2_ready);
          int *value_p = (int *) operandEntry.src_addr1;
          org_res = flowEntry.result;
          new_res = org_res + *value_p;
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
        operandEntry.multStageCounter = numMultStages;
        freeOperandBufIDs.push_back(i);
#ifdef DEBUG_VAULT
        cout << "VC " << vaultContID << " CUBE " << cubeID << " freeing operand entry " << i << endl;
#endif
      }
    }

    // 2) reply ready GET response to commit the flow
    map<FlowID, VaultFlowEntry>::iterator iter = flowTable.begin();
    while (iter != flowTable.end()) {
      FlowID flowID = iter->first;
      VaultFlowEntry &flowEntry = iter->second;
      bool gather_sent = false;
      if (flowEntry.req_count == flowEntry.rep_count && flowEntry.g_flag) {
        TranTrace *trace = new TranTrace(transtat);
        int parent_cube = flowEntry.parent;
        Packet *gpkt = new Packet(RESPONSE, ACT_GET, flowID, vaultContID, 0, 2, trace, parent_cube, parent_cube); // for now, these are the same, signifying that a packet is coming from a vault with vaultContID in src_addr field
        if (ReceiveUp(gpkt)) {
          numFlowRespSent++;
          gather_sent = true;
        } else {
          delete trace;
          delete gpkt;
        }
      }
      if (flowTable.empty()) // To Troy: why need the check and break here? Whether the while loop check includes this scenario?
        break;
      if (gather_sent) {
        flowTable.erase(iter++);
      } else {
        iter++;
      }
    }

    //Update DRAM state and various countdown
    UpdateCountdown();

    //Convert request packet into DRAM commands
    if(bufPopDelay == 0) {
      for(int i=0; i<downBuffers.size(); i++) {
        //Make sure that buffer[0] is not virtual tail packet.
        if(downBuffers[i] != NULL) {
          if (downBuffers[i]->CMD == ACT_GET) {
            uint64_t dest_addr = downBuffers[i]->DESTADRS;
            // Jiayi, force the ordering for gather after update, 07/02/17
            bool is_inorder = true;
            for (int j = 0; j < i; j++) {
              if (downBuffers[j] != NULL &&
                  ((downBuffers[j]->CMD == ACT_ADD ||
                    downBuffers[j]->CMD == ACT_DOT ||
                    downBuffers[j]->CMD == ACT_MULT) &&
                   downBuffers[j]->DESTADRS == dest_addr)) {
                is_inorder = false;
                break;
              }
            }
#ifdef DEBUG_VERIFY
            for (int j = i + 1; j < downBuffers.size(); j++) {
              if (downBuffers[j] != NULL &&
                  (downBuffers[j]->CMD == ACT_ADD ||
                   downBuffers[j]->CMD == ACT_DOT ||
                   downBuffers[j]->CMD == ACT_MULT))
                assert(downBuffers[j]->DESTADRS != dest_addr);
            }
#endif
            if (!is_inorder) {
              continue;
            }
            map<FlowID, VaultFlowEntry>::iterator it  = flowTable.find(dest_addr);
            if(it == flowTable.end()) cout << "HMC " << cubeID <<" at VC "<<vaultContID<<" assert for flow " << hex << dest_addr << dec << endl;
            assert(it != flowTable.end());

            // mark the g flag to indicate gather requtest arrives
            flowTable[dest_addr].g_flag = true;

            int tempLNG = downBuffers[i]->LNG;
            // Jiayi, 02/06, print out if active packet
            if (downBuffers[i]->CMD == ACT_ADD ||
                downBuffers[i]->CMD == ACT_DOT) {
              assert(downBuffers[i]->SRCADRS1 != 0);
#ifdef DEBUG_ACTIVE
              cout << ":::convert active packet " << downBuffers[i]->TAG << " to commands" << endl;
#endif
            }
            delete downBuffers[i];
            downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
            --i;
            continue;
          }
          else if (downBuffers[i]->CMD == ACT_ADD || downBuffers[i]->CMD == ACT_DOT) {
#ifdef DEBUG_VAULT
            cout << "VC " << vaultContID << " CUBE " << cubeID << " ADD UPDATE" << endl;
#endif
            bool operand_buf_avail = freeOperandBufIDs.empty() ? false : true;
            if (operand_buf_avail) {
              int operand_buf_id = freeOperandBufIDs.front();
              freeOperandBufIDs.pop_front();
              downBuffers[i]->vaultOperandBufID = operand_buf_id;
              if(ConvPacketIntoCMDs(downBuffers[i])) {
                numOperands++;
                numADDUpdates++;
                numAdds++;
                uint64_t dest_addr = downBuffers[i]->DESTADRS;
                map<FlowID, VaultFlowEntry>::iterator it = flowTable.find(dest_addr);
                if (it == flowTable.end()) {
                  flowTable.insert(make_pair(dest_addr, VaultFlowEntry(ADD)));
                  numFlows++;
                  flowTable[dest_addr].parent = cubeID; // for now
                  flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
                  cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << xbar->cubeID << endl;
#endif
                } else {
                  assert(it->second.parent == cubeID);
                  it->second.req_count++;
                }
                VaultOperandEntry &operandEntry = operandBuffers[operand_buf_id];
                assert(operandEntry.src_addr1 == 0 && operandEntry.src_addr2 == 0);
                assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
                operandEntry.flowID = dest_addr;
                operandEntry.src_addr1 = downBuffers[i]->SRCADRS1; // only use the first operand
#ifdef DEBUG_UPDATE
                cout << "Packet " << *curDownBuffers[i] << " reserves operand buffer " << operand_buf_id
                  << " at cube " << xbar->cubeID << " with operand addr " << hex << operandEntry.src_addr1 << dec << endl;
#endif
                int tempLNG = downBuffers[i]->LNG;
                assert(downBuffers[i]->SRCADRS1 != 0);
#ifdef DEBUG_ACTIVE
                  cout << ":::convert active packet " << downBuffers[i]->TAG << " to commands" << endl;
#endif
                delete downBuffers[i];
                downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
                --i;
              }
              else {
                freeOperandBufIDs.push_back(operand_buf_id);
              }
            }
          }
          else if (downBuffers[i]->CMD == ACT_MULT && downBuffers[i]->packetType == RESPONSE) {
#ifdef DEBUG_VAULT
            cout << "VC " << vaultContID << " CUBE " << cubeID << " got a local response (from crossbar)" << endl;
            numLocalRespRecv++;
#endif
            assert(downBuffers[i]->computeVault == vaultContID);
            int vault_operand_buf_id = downBuffers[i]->vaultOperandBufID;
            uint64_t dest_addr = downBuffers[i]->DESTADRS;
            VaultOperandEntry &operandEntry = operandBuffers[vault_operand_buf_id];
#ifdef DEBUG_VAULT
            cout << "VC " << vaultContID << " CUBE " << cubeID << " RESPONSE DEST " << hex << dest_addr << " SRC1 " << downBuffers[i]->SRCADRS1 << " SRC2 " << downBuffers[i]->SRCADRS2 << endl;
            cout << "\tOperandEntry #" << dec << vault_operand_buf_id << hex << " op.dest " << operandEntry.flowID << " op.src1 = " << operandEntry.src_addr1 << " op.src2 = " << operandEntry.src_addr2 << dec << endl;
#endif
            assert (operandEntry.src_addr1 == downBuffers[i]->SRCADRS1 || operandEntry.src_addr2 == downBuffers[i]->SRCADRS2);
            if (operandEntry.src_addr1 == downBuffers[i]->SRCADRS1) {
#ifdef DEBUG_VAULT
              cout << "VC " << vaultContID << " CUBE " << cubeID << " FLOW " << hex << operandEntry.flowID << dec << " updated src1 with " << hex << operandEntry.src_addr1 << dec << endl;
#endif
              operandEntry.op1_ready = true;
            } else {
#ifdef DEBUG_VAULT
              cout << "VC " << vaultContID << " CUBE " << cubeID << " FLOW " << hex << operandEntry.flowID << dec << " updated src2 with " << hex << operandEntry.src_addr2 << dec << endl;
#endif
              assert(operandEntry.src_addr2 == downBuffers[i]->SRCADRS2);
              operandEntry.op2_ready = true;
            }
            if (operandEntry.op1_ready && operandEntry.op2_ready) {
              operandEntry.ready = true;
#ifdef DEBUG_VAULT
              cout << "VC " << vaultContID << " CUBE " << cubeID << " remote response made operand entry #" << vault_operand_buf_id << " ready" << endl;
              numMults++;
#endif
            }
            int tempLNG = downBuffers[i]->LNG;
            delete downBuffers[i];
            downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
            --i;
          } else {
            if(ConvPacketIntoCMDs(downBuffers[i])) {
#ifdef DEBUG_VAULT
              if (downBuffers[i]->CMD == ACT_MULT && 
                  (downBuffers[i]->SRCCUB != cubeID || downBuffers[i]->computeVault != vaultContID)) {
                cout << "VC " << vaultContID << " CUBE " << cubeID << " got a remote request" << endl;
                numRemoteReqRecv++;
              }
#endif
              int tempLNG = downBuffers[i]->LNG;
              // Jiayi, 02/06, print out if active packet
              if (downBuffers[i]->CMD == ACT_ADD ||
                  downBuffers[i]->CMD == ACT_DOT) {
                assert(downBuffers[i]->SRCADRS1 != 0);
#ifdef DEBUG_ACTIVE
                cout << ":::convert active packet " << downBuffers[i]->TAG << " to commands" << endl;
#endif
              }
              delete downBuffers[i];
              downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
              --i;
            }
          }
        }
      }
    }

    //Send response packet to crossbar switch
    if(upBuffers.size() > 0) {
      //Make sure that buffer[0] is not virtual tail packet.
      if(upBuffers[0] == NULL) {
        ERROR(header<<"  == Error - Vault controller up buffer[0] is NULL (It could be one of virtual tail packet occupying packet length  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      else{
        if(upBufferDest->ReceiveUp(upBuffers[0])) {
          DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   SENDING packet to crossbar switch (CS)");
          upBuffers.erase(upBuffers.begin(), upBuffers.begin()+upBuffers[0]->LNG);
        }
        else {
          //DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   Crossbar switch buffer FULL");
        }
      }
    }

    //Pop command from command queue
    if(commandQueue->CmdPop(&poppedCMD)) {
      //Write data command will be issued after countdown
      if(poppedCMD->commandType == WRITE || poppedCMD->commandType == WRITE_P) {
        DRAMCommand *writeData = new DRAMCommand(*poppedCMD);
        writeData->commandType = WRITE_DATA;
        writeDataToSend.push_back(writeData);
        writeDataCountdown.push_back(WL);
      }

      // Jiayi, to update pendingDataSize for response, 02/04
      if(poppedCMD->lastCMD == true) {
        if(!poppedCMD->posted) {
          if(poppedCMD->commandType == WRITE || poppedCMD->commandType == WRITE_P) {
            DRAM_wr_data += poppedCMD->dataSize;
            if(!poppedCMD->atomic) {
              pendingDataSize += 1;
              //DEBUG(ALI(18)<<header<<ALI(15)<<*poppedCMD<<"Down) pendingDataSize 1 increased   (current pendingDataSize : "<<pendingDataSize<<")");
            }
          }
          else if(poppedCMD->commandType == READ || poppedCMD->commandType == READ_P) {
            DRAM_rd_data += poppedCMD->dataSize;
            if (poppedCMD->packetCMD == ACT_MULT ||
                poppedCMD->packetCMD == ACT_ADD ||
                poppedCMD->packetCMD == ACT_DOT) {
              DRAM_act_data += poppedCMD->dataSize;
            }
            if(!poppedCMD->atomic) {
              pendingDataSize += (poppedCMD->dataSize/16)+1;
              //DEBUG(ALI(18)<<header<<ALI(15)<<*poppedCMD<<"Down) pendingDataSize "<<(poppedCMD->dataSize/16)+1<<" increased   (current pendingDataSize : "<<pendingDataSize<<")");
            }
            else {
              if(poppedCMD->packetCMD == _2ADD8 || poppedCMD->packetCMD == ADD16 || poppedCMD->packetCMD == INC8
                  || poppedCMD->packetCMD == EQ8 || poppedCMD->packetCMD == EQ16 || poppedCMD->packetCMD == BWR
                  || poppedCMD->packetCMD == PEI_ATOMIC) {
                pendingDataSize += 1;
              }
              else {
                pendingDataSize += 2;
              }
            }
          }
        }
      }

      //Check for collision on bus
      if(cmdBus != NULL) {
        ERROR(header<<"  == Error - Command bus collision  (cmdBus:"<<*cmdBus<<", poppedCMD:"<<*poppedCMD<<")  (CurrentClock : "<<currentClockCycle<<")");
        exit(0);
      }
      cmdBus = poppedCMD;
      cmdCyclesLeft = tCMD;
      poppedCMD = NULL;
    }

    //Atomic command operation (assume that all operations consume one clock cycle)
    if(atomicCMD != NULL) {
      if(atomicOperLeft == 0) {
        //The results are written back to DRAM, overwriting the original memory operands
        if(atomicCMD->packetCMD != EQ16 && atomicCMD->packetCMD != EQ8) {
          DRAMCommand *atmRstCMD = new DRAMCommand(*atomicCMD);
          atmRstCMD->commandType = (OPEN_PAGE ? WRITE : WRITE_P);
          atmRstCMD->dataSize = 16;
          atmRstCMD->lastCMD = true;
          if(!atmRstCMD->posted) {
            atmRstCMD->trace = NULL;
          }
          if(QUE_PER_BANK) {
            commandQueue->queue[atomicCMD->bank].insert(commandQueue->queue[atomicCMD->bank].begin(), atmRstCMD);
            classID.str( string() );	classID.clear();
            classID << atomicCMD->bank;
            DE_CR(ALI(18)<<(commandQueue->header+"-"+classID.str()+")")<<ALI(15)<<*atmRstCMD<<"Down) PUSHING atomic result command into command queue");
          }
          else {
            commandQueue->queue[0].insert(commandQueue->queue[0].begin(), atmRstCMD);
            DE_CR(ALI(18)<<(commandQueue->header+")")<<ALI(15)<<*atmRstCMD<<"Down) PUSHING atomic result command into command queue");
          }
        }

        if(!atomicCMD->posted) {
          MakeRespondPacket(atomicCMD);
          assert(atomicCMD->packetCMD != PEI_DOT);
        }
        delete atomicCMD;
        atomicCMD = NULL;
      }
      else {
        DEBUG(ALI(18)<<(commandQueue->header+")")<<ALI(15)<<*atomicCMD<<"Down) Atomic command is now operating ["<<atomicOperLeft<<" clk]");
        atomicOperLeft--;
      }
    }

    //Power-down mode setting
    EnablePowerdown();

    commandQueue->Update();

    for (int i=0; i<pcuPacket.size(); i++) {
      //assert(pcuPacket[i]->bufPopDelay > 0);
      if (pcuPacket[i]->bufPopDelay > 0)
        pcuPacket[i]->bufPopDelay--;
      else
        cout << CYCLE() << "PEI: local upBuffer must be fulled for a while" << endl;
    }
    Step();
  }

  //
  //Update DRAM state and various countdown
  //
  void VaultController::UpdateCountdown()
  {
    //Check for outgoing command and handle countdowns
    if(cmdBus != NULL) {
      cmdCyclesLeft--;
      if(cmdCyclesLeft == 0) {	//command is ready to be transmitted
        DE_CR(ALI(18)<<header<<ALI(15)<<*cmdBus<<"Down) ISSUING command to DRAM");
        if(cmdBus->trace != NULL && cmdBus->trace->vaultIssueTime == 0) {
          cmdBus->trace->vaultIssueTime = currentClockCycle;
        }
        dramP->receiveCMD(cmdBus);
        cmdBus = NULL;
      }
    }
    //Check for outgoing write data packets and handle countdowns
    if(dataBus != NULL) {
      dataCyclesLeft--;
      if(dataCyclesLeft == 0) {
        DE_CR(ALI(18)<<header<<ALI(15)<<*dataBus<<"Down) ISSUING data corresponding to previous write command");
        if(dataBus->lastCMD) {
          if(!dataBus->atomic && !dataBus->posted) {
            MakeRespondPacket(dataBus);
            assert(dataBus->packetCMD != PEI_DOT);
          }
          else if(dataBus->trace != NULL && dataBus->posted) {
            dataBus->trace->tranFullLat = ceil(currentClockCycle * (double)tCK/gCpuClkPeriod) - dataBus->trace->tranTransmitTime;
            dataBus->trace->linkFullLat = ceil(currentClockCycle * (double)tCK/gCpuClkPeriod) - dataBus->trace->linkTransmitTime;
            dataBus->trace->vaultFullLat = currentClockCycle - dataBus->trace->vaultIssueTime;
            delete dataBus->trace;
          }
        }
        dramP->receiveCMD(dataBus);
        dataBus = NULL;
      }
    }
    //Check write data to be sent to DRAM and handle countdowns
    if(writeDataCountdown.size() > 0) {
      if(writeDataCountdown[0] == 0) {
        if(dataBus != NULL) {
          ERROR(header<<"   == Error - Data Bus Collision  "<<*dataBus<<"  (CurrentClock : "<<currentClockCycle<<")");
          exit(0);
        }
        dataBus = writeDataToSend[0];
        dataCyclesLeft = BL;	//block size according to address mapping / DATA_WIDTH

        writeDataCountdown.erase(writeDataCountdown.begin());
        writeDataToSend.erase(writeDataToSend.begin());
      }

      for(int i=0; i<writeDataCountdown.size(); i++) {
        writeDataCountdown[i]--;
      }
    }

    //Time for a refresh issue a refresh
    refreshCountdown--;
    if(refreshCountdown == 0) {
      commandQueue->refreshWaiting = true;
      refreshCountdown = REFRESH_PERIOD/tCK;
      DEBUG(ALI(39)<<header<<"REFRESH countdown is over");
    }
    //If a rank is powered down, make sure we power it up in time for a refresh
    else if(powerDown && refreshCountdown<=tXP)	{
    }
  }

  //
  //Convert packet into DRAM commands
  //
  bool VaultController::ConvPacketIntoCMDs(Packet *packet)
  {
    unsigned bankAdd, colAdd, rowAdd;
    AddressMapping(packet->ADRS, bankAdd, colAdd, rowAdd);

    DRAMCommandType tempCMD;
    bool tempPosted = false;
    bool atomic = false;
    switch(packet->CMD) {
      //Write
      case WR16:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR32:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR48:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR64:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR80:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR96:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR112:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR128:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case WR256:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
      case MD_WR:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	break;
                    //Poseted Write
      case P_WR16:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR32:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR48:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR64:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR80:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR96:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR112:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR128:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
      case P_WR256:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempPosted = true;	break;
                    //Read
      case RD16:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD32:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD48:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD64:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD80:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD96:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD112:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD128:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case RD256:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
      case MD_RD:		tempCMD = OPEN_PAGE ? READ : READ_P;	break;
                    //Arithmetic atomic
      case _2ADD8:	atomic = true;	tempCMD = READ;	break;
      case ADD16:		atomic = true;	tempCMD = READ;	break;
      case P_2ADD8:	atomic = true;	tempCMD = READ;	tempPosted = true;	break;
      case P_ADD16:	atomic = true;	tempCMD = READ;	tempPosted = true;	break;
      case _2ADDS8R:	atomic = true;	tempCMD = READ;	break;
      case ADDS16R:	atomic = true;	tempCMD = READ; break;
      case INC8:		atomic = true;	tempCMD = READ;	break;
      case P_INC8:	atomic = true;	tempCMD = READ;	tempPosted = true;	break;
                    //Boolean atomic
      case XOR16:		atomic = true;	tempCMD = READ;	break;
      case OR16:		atomic = true;	tempCMD = READ;	break;
      case NOR16:		atomic = true;	tempCMD = READ;	break;
      case AND16:		atomic = true;	tempCMD = READ;	break;
      case NAND16:	atomic = true;	tempCMD = READ;	break;
                    //Comparison atomic
      case CASGT8:	atomic = true;	tempCMD = READ;	break;
      case CASLT8:	atomic = true;	tempCMD = READ;	break;
      case CASGT16:	atomic = true;	tempCMD = READ;	break;
      case CASLT16:	atomic = true;	tempCMD = READ;	break;
      case CASEQ8:	atomic = true;	tempCMD = READ;	break;
      case CASZERO16:	atomic = true;	tempCMD = READ;	break;
      case EQ16:		atomic = true;	tempCMD = READ;	break;
      case EQ8:		atomic = true;	tempCMD = READ;	break;
                  //Bitwise atomic
      case BWR:		atomic = true;	tempCMD = READ;	break;
      case P_BWR:		atomic = true;	tempCMD = READ;	tempPosted = true;	break;
      case BWR8R:		atomic = true;	tempCMD = READ;	break;
      case SWAP16:	atomic = true;	tempCMD = READ;	break;
      // Active ops
      case ACT_ADD:   tempCMD = OPEN_PAGE ? READ : READ_P; break;
      case ACT_MULT:  tempCMD = OPEN_PAGE ? READ : READ_P; break;
      case ACT_DOT:   tempCMD = OPEN_PAGE ? READ : READ_P; break;
      case PEI_DOT:   tempCMD = OPEN_PAGE ? READ : READ_P; break;
      // PEI atomic
      case PEI_ATOMIC: atomic = true; tempCMD = READ; break;

      default:
                      ERROR(header<<"  == Error - WRONG packet command type  (CurrentClock : "<<currentClockCycle<<")");
                      exit(0);
    }

    //Due to the internal 32-byte granularity of the DRAM data bus within each vault in the HMC (HMC spec v2.1 p.99)
    if(commandQueue->AvailableSpace(bankAdd, ceil((double)packet->reqDataSize/32)+1)) {
      DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<"Down) phyAdd : 0x"<<hex<<setw(9)<<setfill('0')<<packet->ADRS<<dec<<"  bankAdd : "<<bankAdd<<"  colAdd : "<<colAdd<<"  rowAdd : "<<rowAdd);
      //cmdtype, tag, bnk, col, rw, *dt, dSize, pst, *lat
      DRAMCommand *actCMD = new DRAMCommand(ACTIVATE, packet->TAG, bankAdd, colAdd, rowAdd, 0, false, packet->trace, true, packet->CMD, atomic, packet->segment);
      // Ram & Jiayi, 03/13/17, unnecessary?
      actCMD->src_cube = packet->SRCCUB;
      actCMD->dest_cube = packet->DESTCUB;
      commandQueue->Enqueue(bankAdd, actCMD);

      for(int i=0; i<ceil((double)packet->reqDataSize/32); i++) {
        DRAMCommand *rwCMD;
        if(i < ceil((double)packet->reqDataSize/32)-1) {
          if(tempCMD == WRITE_P) {
            rwCMD = new DRAMCommand(WRITE, packet->TAG, bankAdd, colAdd, rowAdd, packet->reqDataSize, tempPosted, packet->trace, false, packet->CMD, atomic, packet->segment);
          }
          else if(tempCMD == READ_P) {
            rwCMD = new DRAMCommand(READ, packet->TAG, bankAdd, colAdd, rowAdd, packet->reqDataSize, tempPosted, packet->trace, false, packet->CMD, atomic, packet->segment);
          }
          else {
            rwCMD = new DRAMCommand(tempCMD, packet->TAG, bankAdd, colAdd, rowAdd, packet->reqDataSize, tempPosted, packet->trace, false, packet->CMD, atomic, packet->segment);
          }
        }
        else {
          rwCMD = new DRAMCommand(tempCMD, packet->TAG, bankAdd, colAdd, rowAdd, packet->reqDataSize, tempPosted, packet->trace, true, packet->CMD, atomic, packet->segment);
        }
        // Ram & Jiayi, 03/13/17
        rwCMD->src_cube = packet->SRCCUB;
        rwCMD->dest_cube = packet->DESTCUB;
        commandQueue->Enqueue(bankAdd, rwCMD);
        if(tempCMD == READ || tempCMD == READ_P) {
          pendingReadData.push_back(packet->TAG);
          if (packet->CMD == ACT_ADD ||
              packet->CMD == ACT_DOT) {
            assert(packet->SRCADRS1 && !packet->SRCADRS2);
            rwCMD->srcAddr1 = packet->SRCADRS1;
            rwCMD->destAddr = packet->DESTADRS;
            rwCMD->operandBufID = packet->operandBufID;
            rwCMD->vaultOperandBufID = packet->vaultOperandBufID;
          } else if (packet->CMD == ACT_MULT) {
            assert((!packet->SRCADRS1 && packet->SRCADRS2) || (packet->SRCADRS1 && !packet->SRCADRS2));
            if (packet->SRCADRS1 != 0) {
              rwCMD->srcAddr1 = packet->SRCADRS1;
            } else {
              assert(packet->SRCADRS2);
              rwCMD->srcAddr2 = packet->SRCADRS2;
            }
            rwCMD->destAddr = packet->DESTADRS;
            rwCMD->operandBufID = packet->operandBufID;
            rwCMD->vaultOperandBufID = packet->vaultOperandBufID;
            rwCMD->computeVault = packet->computeVault;
          }
        }
        rwCMD->addr = packet->orig_addr;//packet->ADRS; // Jiayi, 03/27/17
        rwCMD->tran_tag = packet->tran_tag;
      }
      return true;
    }
    else{
      return false;
    }
  }

  //
  //Mapping physical address to bank, column, and row
  //
  void VaultController::AddressMapping(uint64_t physicalAddress, unsigned &bankAdd, unsigned &colAdd, unsigned &rowAdd)
  {
    unsigned maxBlockBit = _log2(ADDRESS_MAPPING);
    physicalAddress >>= maxBlockBit/*max block bits*/ + _log2(NUM_VAULTS)/*vault address bits*/;

    //Extract bank address
    bankAdd = physicalAddress & (NUM_BANKS-1);
    physicalAddress >>= _log2(NUM_BANKS);

    //Extract column address
    colAdd = physicalAddress & ((NUM_COLS-1)>>maxBlockBit);
    colAdd <<= maxBlockBit;
    physicalAddress >>= (_log2(NUM_COLS)-maxBlockBit);

    //Extract row address
    rowAdd = physicalAddress & (NUM_ROWS-1);
  }

  //
  //Power-down mode setting
  //
  void VaultController::EnablePowerdown()
  {
    if(USE_LOW_POWER) {
      if(commandQueue->isEmpty() && !commandQueue->refreshWaiting) {
        if(dramP->powerDown()) {
          powerDown = true;
        }
      }
      else if(powerDown && dramP->bankStates[0]->nextPowerUp <= currentClockCycle) {
        dramP->powerUp();
        powerDown = false;
      }
    }
  }

  //
  //Print current state in state log file
  //
  void VaultController::PrintState()
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

    commandQueue->PrintState();
  }

  //
  //Print buffers
  //
  void VaultController::PrintBuffers()
  {
    InputBuffer *ibuf = dynamic_cast<InputBuffer *> (upBufferDest);
    assert(ibuf);
    CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (ibuf->xbar);
    assert(xbar);
    cout << " -- Vault " << vaultContID << endl;
    cout << "  - downBuffers size: " << downBuffers.size() << endl;
    for (int i = 0; i < downBuffers.size(); i++) {
      if (downBuffers[i] != NULL) {
        int link = xbar->rf->findNextLink(xbar->inServiceLink, xbar->cubeID, downBuffers[i]->DESTCUB);
        int next_cube = xbar->neighborCubeID[link];
        cout << (downBuffers[i]->packetType == REQUEST ? "    Request " : "    Response ")
          << *downBuffers[i] << " from current cube " << xbar->cubeID << " to next cube " << next_cube
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
    cout << "  - upBuffers size: " << upBuffers.size() << endl;
    for (int i = 0; i < upBuffers.size(); i++) {
      if (upBuffers[i] != NULL) {
        int link = xbar->rf->findNextLink(xbar->inServiceLink, xbar->cubeID, upBuffers[i]->DESTCUB);
        int next_cube = xbar->neighborCubeID[link];
        cout << (upBuffers[i]->packetType == REQUEST ? "    Request " : "    Response ")
          << *upBuffers[i] << " from current cube " << xbar->cubeID << " to next cube " << next_cube
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
  }

  /* returns true if there are operand buffers to reserve and reserves one if
    there are any available */
  //
  //Returns operand buffer id if there are any operands available and reserves one
  //Otherwise, return -1
  //
  int VaultController::OperandBufferStatus(Packet* pkt) {
    bool operand_buf_avail = freeOperandBufIDs.empty() ? false : true;
    if (operand_buf_avail) {
      InputBuffer *ibuf = dynamic_cast<InputBuffer *> (upBufferDest);
      assert(ibuf);
      CrossbarSwitch *xbar = dynamic_cast<CrossbarSwitch *> (ibuf->xbar);
      numOperands++;
      int operand_buf_id = freeOperandBufIDs.front();
      freeOperandBufIDs.pop_front();
      pkt->vaultOperandBufID = operand_buf_id;
      pkt->computeVault = vaultContID;
      uint64_t dest_addr = pkt->DESTADRS;
      map<FlowID, VaultFlowEntry>::iterator it = flowTable.find(dest_addr);
      if (it == flowTable.end()) {
        flowTable.insert(make_pair(dest_addr, VaultFlowEntry(MAC)));
        numFlows++;
        cubeID = xbar->cubeID;
        flowTable[dest_addr].parent = xbar->cubeID; // for now
        flowTable[dest_addr].req_count = 1;
#if defined(DEBUG_FLOW) || defined(DEBUG_UPDATE)
        cout << "Active-Routing (flow: " << hex << dest_addr << dec << "): reserve an entry for Active target at cube " << xbar->cubeID << endl;
#endif
      } else {
        assert(it->second.parent == xbar->cubeID);
        it->second.req_count++;
      }
      VaultOperandEntry &operandEntry = operandBuffers[operand_buf_id];
#ifdef DEBUG_VAULT
      cout << "VC " << vaultContID << " CUBE " << cubeID << " reserving operand buffer " << operand_buf_id << endl;
#endif
      assert(operandEntry.src_addr1 == 0 && operandEntry.src_addr2 == 0);
#ifdef DEBUG_VAULT
      if (!(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready)) {
        cout << "VC " << vaultContID << " CUBE " << cubeID << " operand entry #" << operand_buf_id << endl;
        cout << "\tflowID " << hex << operandEntry.flowID << dec << " op1 ready " << operandEntry.op1_ready << " op2 ready " << operandEntry.op2_ready <<
          " op ready " << operandEntry.ready << endl;
      }
#endif
      assert(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready);
      operandEntry.flowID = dest_addr;
      // Should we go ahead and put these here?
      operandEntry.src_addr1 = pkt->SRCADRS1;
      if (operandEntry.src_addr1 == 0) cout << "VC " << vaultContID << " CUBE " << xbar->cubeID << " got an update with 0 in src1" << endl;
      operandEntry.src_addr2 = pkt->SRCADRS2;
      if (operandEntry.src_addr2 == 0) cout << "VC " << vaultContID << " CUBE " << xbar->cubeID << " got an update with 0 in src2" << endl;
      return operand_buf_id;
    }
    else
      return -1;
  }

  void VaultController::FreeOperandBuffer(int i) {
    VaultOperandEntry &operandEntry = operandBuffers[i];
    FlowID flowID = operandEntry.flowID;
    assert(flowTable.find(flowID) != flowTable.end());
    VaultFlowEntry &flowEntry = flowTable[flowID];
    flowEntry.req_count--;  // undo the last request
    assert(flowEntry.req_count >= 0);
    operandEntry.flowID = 0;
    operandEntry.src_addr1 = 0;
    operandEntry.src_addr2 = 0;
    operandEntry.op1_ready = false;
    operandEntry.op2_ready = false;
    operandEntry.ready = false;
    operandEntry.multStageCounter = numMultStages;
    freeOperandBufIDs.push_back(i);
#ifdef DEBUG_VAULT
    cout << "VC " << vaultContID << " CUBE " << cubeID << " freeing operand entry " << i << endl;
    if (!(!operandEntry.op1_ready && !operandEntry.op2_ready && !operandEntry.ready)) {
      cout << "VC " << vaultContID << " CUBE " << cubeID << " operand entry #" << i << endl;
      cout << "\tflowID " << hex << operandEntry.flowID << dec << " op1 ready " << operandEntry.op1_ready << " op2 ready " << operandEntry.op2_ready <<
        " op ready " << operandEntry.ready << endl;
    }
#endif
  }

} //namespace CasHMC
