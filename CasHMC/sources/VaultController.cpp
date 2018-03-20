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
    vaultContID(id)
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
  }

  VaultController::VaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id, string headerPrefix):
    DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_VLT_BUF, MAX_VLT_BUF),
    vaultContID(id)
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
  }

  VaultController::~VaultController()
  {
    pendingReadData.clear(); 
    writeDataToSend.clear(); 
    writeDataCountdown.clear(); 

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
        case ACT_ADD:	DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETURNING active_add response packet");		break;  // Jiayi
        case ACT_MULT: DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETRUNING active mult response packet");  break;
        case PEI_DOT: DE_CR(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RETRUNING PEI_DOT response packet");  break;
        default:
                    ERROR(header<<"  == Error - WRONG response packet command type  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
                    exit(0);
                    break;
      }
    }
    else {
      ERROR(header<<"  == Error - Vault controller upstream packet buffer FULL  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
      ERROR(header<<"             Vault buffer max size : "<<upBufferMax<<", current size : "<<upBuffers.size()<<", "<<*upEle<<" size : "<<upEle->LNG);
      exit(0);
    }
  }	

  //
  //Return read commands from DRAM
  //
  void VaultController::ReturnCommand(DRAMCommand *retCMD)
  {
    //Read and write data commands share one data bus
    if(dataBus != NULL) {
      ERROR(header<<"  == Error - Data Bus Collision  (dataBus:"<<*dataBus<<", retCMD:"<<*retCMD<<")  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }

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
            MakeRespondPacket(retCMD);
            delete retCMD;
          }
        }
        else {
          delete retCMD;
        }
        pendingReadData.erase(pendingReadData.begin()+i);
        foundMatch = true;
        break;
      }
    }
    if(!foundMatch) {
      ERROR(header<<"  == Error - Can't find a matching transaction  "<<*retCMD<<" 0x"<<hex<<retCMD->packetTAG<<dec<<"  (CurrentClock : "<<currentClockCycle<<")");
      exit(0);
    }
  }

  //
  //Make response packet from request
  //
  void VaultController::MakeRespondPacket(DRAMCommand *retCMD)
  {
    if(retCMD->trace != NULL) {
      retCMD->trace->vaultFullLat = currentClockCycle - retCMD->trace->vaultIssueTime;
    }
    Packet *newPacket;
    if(retCMD->atomic) {
      if(retCMD->packetCMD == _2ADD8 || retCMD->packetCMD == ADD16 || retCMD->packetCMD == INC8
          || retCMD->packetCMD == EQ8 || retCMD->packetCMD == EQ16 || retCMD->packetCMD == BWR) {
        newPacket = new Packet(RESPONSE, WR_RS, retCMD->packetTAG, 1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        pendingDataSize -= 1;
      }
      else {
        newPacket = new Packet(RESPONSE, RD_RS, retCMD->packetTAG, 2, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        pendingDataSize -= 2;
      }
    }
    else {
      if(retCMD->commandType == WRITE_DATA) {
        //packet, cmd, tag, lng, *lat
        newPacket = new Packet(RESPONSE, WR_RS, retCMD->packetTAG, 1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        pendingDataSize -= 1;
        //DEBUG(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   pendingDataSize 1 decreased   (current pendingDataSize : "<<pendingDataSize<<")");
      }
      else if(retCMD->commandType == READ_DATA) {
        //packet, cmd, tag, lng, *lat
        newPacket = new Packet(RESPONSE, RD_RS, retCMD->packetTAG, (retCMD->dataSize/16)+1, retCMD->trace, retCMD->dest_cube, retCMD->src_cube);
        pendingDataSize -= (retCMD->dataSize/16)+1;
        //DEBUG(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   pendingDataSize "<<(retCMD->dataSize/16)+1<<" decreased   (current pendingDataSize : "<<pendingDataSize<<")");
        newPacket->ADRS = retCMD->addr; // Jiayi, 03/27/17
       
        if (retCMD->packetCMD == ACT_ADD) { // Jiayi
          newPacket->CMD = ACT_ADD; // Jiayi, 03/15/17
          newPacket->active = true;
          newPacket->DESTADRS = retCMD->destAddr;
          newPacket->SRCADRS1 = retCMD->srcAddr1;
          newPacket->LNG = 2;
#ifdef DEBUG_UPDATE
          cout << "Active ADD packet " << newPacket->TAG << " is returned" << endl;
#endif

        } else if(retCMD->packetCMD == PEI_DOT) {
          newPacket->LNG = 2;
          newPacket->CMD = PEI_DOT; // Jiayi, 03/15/17
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
          newPacket->LNG = 2;
#ifdef DEBUG_UPDATE
          cout << CYCLE() << "Active-Routing: Active MULT packet " << newPacket->TAG;
          if (newPacket->SRCADRS1) {
            assert(newPacket->SRCADRS2 == 0);
            cout << " for operand 1 is returned, src_cube: " << newPacket->SRCCUB << ", dest_cube: " << newPacket->DESTCUB
              << endl;
              //<<" (srcAddr1: 0x" << hex << newPacket->SRCADRS1
              //<< ", srcAddr2: 0x" << newPacket->SRCADRS2 << ")" << dec << endl;
          } else {
            assert(newPacket->SRCADRS1 == 0 && newPacket->SRCADRS2);
            cout << " for operand 2 is returned, src_cube: " << newPacket->SRCCUB << ", dest_cube: " << newPacket->DESTCUB
              << endl;
              //<<" (srcAddr1: 0x" << hex << newPacket->SRCADRS1
              //<< ", srcAddr2: 0x" << newPacket->SRCADRS2 << ")" << dec << endl;
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
    if(newPacket->CMD != PEI_DOT){
    ReceiveUp(newPacket);
    }else{
       if(pcuPacket.empty()){ 
         newPacket->bufPopDelay = PCU_DELAY;
       }else{
         newPacket->bufPopDelay = max(PCU_DELAY,(pcuPacket.back())->bufPopDelay + 1);
       }
       pcuPacket.push_back(newPacket);
    }
  }

  //
  //Update the state of vault controller
  //
  void VaultController::Update()
  {
    if(!pcuPacket.empty() && (pcuPacket.front())->bufPopDelay == 0){
      ReceiveUp(pcuPacket.front()); pcuPacket.erase(pcuPacket.begin());
    } 
    //Update DRAM state and various countdown
    UpdateCountdown();

    //Convert request packet into DRAM commands
    if(bufPopDelay == 0) {
      for(int i=0; i<downBuffers.size(); i++) {
        //Make sure that buffer[0] is not virtual tail packet.
        if(downBuffers[i] != NULL) {
          if(ConvPacketIntoCMDs(downBuffers[i])) {
            int tempLNG = downBuffers[i]->LNG;
            // Jiayi, 02/06, print out if active packet
            if (downBuffers[i]->CMD == ACT_ADD) {
              assert(downBuffers[i]->SRCADRS1 != 0);
#ifdef DEBUG_ACTIVE
              cout << ":::convert active packet " << downBuffers[i]->TAG << " to commands" << endl;
#endif
            }
            delete downBuffers[i];
            downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
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
            if(poppedCMD->packetCMD == ACT_MULT || poppedCMD->packetCMD == ACT_ADD) DRAM_act_data += poppedCMD->dataSize;
            if(!poppedCMD->atomic) {
              pendingDataSize += (poppedCMD->dataSize/16)+1;
              //DEBUG(ALI(18)<<header<<ALI(15)<<*poppedCMD<<"Down) pendingDataSize "<<(poppedCMD->dataSize/16)+1<<" increased   (current pendingDataSize : "<<pendingDataSize<<")");
            }
            else {
              if(poppedCMD->packetCMD == _2ADD8 || poppedCMD->packetCMD == ADD16 || poppedCMD->packetCMD == INC8
                  || poppedCMD->packetCMD == EQ8 || poppedCMD->packetCMD == EQ16 || poppedCMD->packetCMD == BWR) {
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
    for(int i=0; i<pcuPacket.size(); i++){
      assert(pcuPacket[i]->bufPopDelay > 0);
      pcuPacket[i]->bufPopDelay--;
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
            dataBus->trace->tranFullLat = ceil(currentClockCycle * (double)tCK/CPU_CLK_PERIOD) - dataBus->trace->tranTransmitTime;
            dataBus->trace->linkFullLat = ceil(currentClockCycle * (double)tCK/CPU_CLK_PERIOD) - dataBus->trace->linkTransmitTime;
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
      case RD16:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD32:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD48:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD64:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD80:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD96:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD112:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD128:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case RD256:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
      case MD_RD:		tempCMD = OPEN_PAGE ? READ : READ_P;	pendingReadData.push_back(packet->TAG);	break;
                    //Arithmetic atomic
      case _2ADD8:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case ADD16:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case P_2ADD8:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	tempPosted = true;	break;
      case P_ADD16:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	tempPosted = true;	break;
      case _2ADDS8R:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case ADDS16R:	atomic = true;	tempCMD = READ; pendingReadData.push_back(packet->TAG);	break;
      case INC8:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case P_INC8:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	tempPosted = true;	break;
                    //Boolean atomic
      case XOR16:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case OR16:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case NOR16:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case AND16:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case NAND16:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
                    //Comparison atomic
      case CASGT8:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case CASLT8:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case CASGT16:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case CASLT16:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case CASEQ8:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case CASZERO16:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case EQ16:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case EQ8:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
                  //Bitwise atomic
      case BWR:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      case P_BWR:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	tempPosted = true;	break; 
      case BWR8R:		atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break; 
      case SWAP16:	atomic = true;	tempCMD = READ;	pendingReadData.push_back(packet->TAG);	break;
      // Active Add
      case ACT_ADD:   tempCMD = OPEN_PAGE ? READ : READ_P; pendingReadData.push_back(packet->TAG); break; 
      case ACT_MULT:   tempCMD = OPEN_PAGE ? READ : READ_P; pendingReadData.push_back(packet->TAG); break; 
      case PEI_DOT:   tempCMD = OPEN_PAGE ? READ : READ_P; pendingReadData.push_back(packet->TAG); break; 

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
          // Jiayi, active, 02/06, 03/24
          //if (packet->active) {
          if (packet->CMD == ACT_ADD) {
            assert(packet->SRCADRS1 && !packet->SRCADRS2);
            rwCMD->srcAddr1 = packet->SRCADRS1;
            rwCMD->destAddr = packet->DESTADRS;
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
          }
          //}
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
          << ", packet length: " << upBuffers[i]->LNG << endl;
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

} //namespace CasHMC
