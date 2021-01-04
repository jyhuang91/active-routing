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
#include "ArtModule.h"

using namespace std;

static unsigned maxSize=0;

namespace CasHMC
{

  class CrossbarSwitch : public DualVectorObject<Packet, Packet>
  {
    public:
      //
      //Functions
      //
      CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_);
      CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_, unsigned id, RoutingFunction *rf = NULL);
      virtual ~CrossbarSwitch();
      void CallbackReceiveDown(Packet *downEle, bool chkReceive);
      void CallbackReceiveUp(Packet *upEle, bool chkReceive);
      virtual void Update();
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

      // Jiayi, extended for active router, 02/06
      map<FlowID, FlowEntry> flowTable;
      vector<OperandEntry> operandBuffers;
      deque<int> freeOperandBufIDs;
      int operandBufSize;
      int multPipeOccupancy;
      int numMultStages;

      // Ram & Jiayi, 03/13/17
      unsigned cubeID;
      RoutingFunction *rf;
      vector<int> neighborCubeID; // <link(0,1,...,NUM_LINKS-1), cubeID>
      int upLink;
      int downLink;
      uint64_t opbufStalls;
      uint64_t numUpdates;
      uint64_t numOperands;

      vector<VaultController *> vaultControllers;   // VLP: Just for specialized HW to query for open OperandBuffers (OperandBufferStatus())

      map<int, long long> hist;

      int total_ready_operands;
      int total_results_ready;
      map<int, long long> ready_operands_hist;
      map<int, long long> results_ready_hist;

      TranStatistic *transtat;
  };

}

#endif
