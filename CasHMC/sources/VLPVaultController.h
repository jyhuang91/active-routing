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

#ifndef VLPVAULTCONTROLLER_H
#define VLPVAULTCONTROLLER_H

//VLPVaultController.h

#include <stdint.h> //uint64_t
#include <vector>   //vector
#include <map>      // map
#include <math.h>   //ceil()
#include <deque>

#include "DualVectorObject.h"
#include "VaultController.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "DRAMCommand.h"
#include "CommandQueue.h"
#include "DRAM.h"
#include "ArtModule.h"

using namespace std;

namespace CasHMC
{

  class VLPVaultController : public VaultController
  {
    public:
      //
      //Functions
      //
      VLPVaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id);
      VLPVaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id, string headerPrefix);
      virtual ~VLPVaultController();
      virtual bool MakeRespondPacket(DRAMCommand *retCMD);
      virtual void Update();
      virtual bool ConvPacketIntoCMDs(Packet *packet);
      virtual int  OperandBufferStatus(Packet* p);
      virtual void FreeOperandBuffer(int i);

      // Extension for vault-level parallelism:
      map<FlowID, VaultFlowEntry> flowTable;
      vector<VaultOperandEntry> operandBuffers;
      deque<int> freeOperandBufIDs;
      int operandBufSize;
      int multPipeOccupancy;
      int numMultStages;
      int numFlows;
      int numOperands;
      int cubeID;
      int numAdds;
      int numMults;
      int numResponses;
      uint64_t opbufStalls;
      uint64_t numADDUpdates;
      // Update Counts For MULTs:
      uint64_t numRemoteReqRecv;
      uint64_t numLocalRespRecv;
      uint64_t numLocalReqRecv;
      uint64_t numFlowRespSent;
      uint64_t numRemoteRespSent;

      map<int, long long> ready_operands_hist;
      int total_ready_operands;
      map<int, long long> results_ready_hist;
      int total_results_ready;
      map<int, long long> updates_received_hist;
      int total_updates_received;

      TranStatistic *transtat;
  };

}

#endif
