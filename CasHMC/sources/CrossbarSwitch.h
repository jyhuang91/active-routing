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

#ifndef CROSSBARSWITCH_H
#define CROSSBARSWITCH_H

//CrossbarSwitch.h

#include <vector>   //vector
#include <map>      // map, Jiayi, 02/06
#include <deque>

#include "DualVectorObject.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "LinkMaster.h"
//#include "TranStatistic.h"
#include "RoutingFunction.h"
#include "InputBuffer.h"
#include "VaultController.h"

using namespace std;

static unsigned maxSize=0;

#define ROUND_ROBIN   0
#define CONTENT_AWARE 1
// may add more here...

namespace CasHMC
{
  struct FlowEntry {
    Opcode   opcode;                    // function code: ADD, MAC, etc.
    double   result;                    // partial result
    uint64_t req_count;                 // number of requests
    uint64_t rep_count;                 // number of responses
    int      parent;                    // parent cubeID of ARTree
    int      children_count[NUM_LINKS]; // number of updates sent to children
    bool     children_gflag[NUM_LINKS];
    bool     g_flag;                    // flag indicating gather command received or not
    int      vault_count[NUM_VAULTS];		// how many requests were sent to each vault
    bool     vault_gflag[NUM_VAULTS];		// how many get requests were sent to each vault

    FlowEntry() : opcode(INVALID), result(0), req_count(0), rep_count(0), parent(-1), g_flag(false) {
      for (int i = 0; i < NUM_LINKS; i++) {
        children_count[i] = 0;
        children_gflag[i] = false;
      }
      for (int i = 0; i < NUM_VAULTS; i++) {
        vault_count[i] = 0;
        vault_gflag[i] = false;
      }
    }
    FlowEntry(Opcode op) : opcode(op), result(0), req_count(0), rep_count(0), parent(-1), g_flag(false) {
      for (int i = 0; i < NUM_LINKS; i++) {
        children_count[i] = 0;
        children_gflag[i] = false;
      }
      for (int i = 0; i < NUM_VAULTS; i++) {
        vault_count[i] = 0;
        vault_gflag[i] = false;
      }
    }
  };

  struct OperandEntry {
    FlowID   flowID;
    uint64_t src_addr1;
    uint64_t src_addr2;
    bool     op1_ready;
    bool     op2_ready;
    bool     ready;
    char     multStageCounter;
    unsigned vault;             // for each vault, an operand gives a partial result from that vault

    OperandEntry() : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(5), ready(false), vault(-1) {}
    OperandEntry(char initMultStage) : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(initMultStage), ready(false), vault(-1) {}
  };

  class CrossbarSwitch : public DualVectorObject<Packet, Packet>
  {
    public:
      //
      //Functions
      //
      CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_);
      CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_, unsigned id, RoutingFunction *rf); // Ram
      virtual ~CrossbarSwitch();
      void CallbackReceiveDown(Packet *downEle, bool chkReceive);
      void CallbackReceiveUp(Packet *upEle, bool chkReceive);
      void Update();
      void UpdateDispatch(Packet* p);
      void PrintState();
      void PrintBuffers();

      //
      //Fields
      //
      vector<DualVectorObject<Packet, Packet> *> downBufferDest;
      vector<LinkMaster *> upBufferDest;
      int inServiceLink;
      vector<unsigned> pendingSegTag;     //Store segment packet tag for returning
      vector<Packet *> pendingSegPacket;  //Store segment packets
      vector<InputBuffer *> inputBuffers;
      vector<VaultController *> vaultControllers;   // Just for specialized HW to query for open OperandBuffers (OperandBufferStatus())

      // Jiayi, extended for active router, 02/06
      map<FlowID, FlowEntry> flowTable;
      vector<OperandEntry> operandBuffers;
      deque<int> freeOperandBufIDs;
      int operandBufSize;
      int multPipeOccupancy;
      int numMultStages;
      int numAdds;
      int numMults;
      int multVault;
      char dispatchPolicy;

      // Ram & Jiayi, 03/13/17
      unsigned cubeID;
      RoutingFunction *rf;
      vector<int> neighborCubeID; // <link(0,1,...,NUM_LINKS-1), cubeID>
      int upLink;
      int downLink;
      uint64_t opbufStalls;
      uint64_t numUpdates;
      uint64_t numOperands;
      uint64_t numFlows;

      TranStatistic *transtat;
  };

}

#endif
