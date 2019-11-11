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

#ifndef VAULTCONTROLLER_H
#define VAULTCONTROLLER_H

//VaultController.h

#include <stdint.h>		//uint64_t
#include <vector>		//vector
#include <map>      // map
#include <math.h>		//ceil()
#include <deque>

#include "DualVectorObject.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "DRAMCommand.h"
#include "CommandQueue.h"
#include "DRAM.h" 
using namespace std;

namespace CasHMC
{
  typedef uint64_t FlowID;

  enum Opcode {
    ADD,
    MAC,
    INVALID
  };

  struct VaultFlowEntry {
    Opcode   opcode;                    // function code: ADD, MAC, etc.
    double   result;                    // partial result
    uint64_t req_count;                 // number of requests
    uint64_t rep_count;                 // number of responses
    int      parent;                    // parent cubeID of ARTree (all have ARE as parent for now)
    bool     g_flag;                    // flag indicating gather command received or not

    VaultFlowEntry() : opcode(INVALID), result(0), req_count(0), rep_count(0), g_flag(false) {}
    VaultFlowEntry(Opcode op) : opcode(op), result(0), req_count(0), rep_count(0), parent(-1), g_flag(false) {}
  };

  struct VaultOperandEntry {
    FlowID   flowID;
    uint64_t src_addr1;
    uint64_t src_addr2;
    bool     op1_ready;
    bool     op2_ready;
    bool     ready;
    char     multStageCounter;

    VaultOperandEntry() : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(5), ready(false) {}
    VaultOperandEntry(char initMultStage) : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(initMultStage), ready(false) {}
  };

  class VaultController : public DualVectorObject<Packet, Packet>
  {
    public:
      //
      //Functions
      //
      VaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id);
      VaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id, string headerPrefix);
      virtual ~VaultController();
      void CallbackReceiveDown(Packet *packet, bool chkReceive);
      void CallbackReceiveUp(Packet *packet, bool chkReceive);
      void ReturnCommand(DRAMCommand *retRead);
      void MakeRespondPacket(DRAMCommand *retCMD);
      void Update();
      void UpdateCountdown();
      bool ConvPacketIntoCMDs(Packet *packet);
      void AddressMapping(uint64_t physicalAddress, unsigned &bankAdd, unsigned &colAdd, unsigned &rowAdd);
      void EnablePowerdown();
      void PrintState();
      void PrintBuffers();
      bool OperandBufferStatus(Packet* p);

      // Extension for vault-level parallelism:
      map<FlowID, VaultFlowEntry> flowTable;
      vector<VaultOperandEntry> operandBuffers;
      deque<int> freeOperandBufIDs;
      int operandBufSize;
      int multPipeOccupancy;
      int numMultStages;
      int numFlows;
      int cubeID;
      int numAdds;
      uint64_t opbufStalls;
      uint64_t numUpdates;
      uint64_t numOperands;

      //
      //Fields
      //
      unsigned vaultContID;
      unsigned refreshCountdown;
      bool powerDown;

      static unsigned DRAM_rd_data; 
      static unsigned DRAM_wr_data; 
      static unsigned DRAM_act_data; 
      DRAM *dramP;
      CommandQueue *commandQueue;
      DRAMCommand *poppedCMD;
      DRAMCommand *atomicCMD;
      unsigned atomicOperLeft;
      unsigned pendingDataSize;
      vector<unsigned> pendingReadData;	//Store Read packet TAG for return data
      vector<Packet *>pcuPacket;	//Store Read packet TAG for return data
      DualVectorObject<Packet, Packet> *upBufferDest;

      //Command and data to be transmitted to DRAM I/O
      DRAMCommand *cmdBus;
      unsigned cmdCyclesLeft;
      DRAMCommand *dataBus;
      unsigned dataCyclesLeft;
      vector<DRAMCommand *> writeDataToSend;
      vector<unsigned> writeDataCountdown;	
      
      TranStatistic *transtat;
  };

}

#endif
