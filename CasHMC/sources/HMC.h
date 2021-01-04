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

#ifndef HMC_H
#define HMC_H

//HMC.h

#include <vector>   //vector

#include "SimulatorObject.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "LinkSlave.h"
#include "LinkMaster.h"
#include "CrossbarSwitch.h"
#include "VaultController.h"
#include "VLPCrossbarSwitch.h"
#include "VLPVaultController.h"
#include "DRAM.h"

using namespace std;

namespace CasHMC
{

  class HMC : public SimulatorObject
  {
    public:
      //
      //Functions
      //
      HMC(ofstream &debugOut_, ofstream &stateOut_, unsigned id_, RoutingFunction *rf_);  // Ram & Jiayi, 03/13/17
      virtual ~HMC();
      void Update();
      void PrintState();
      void RegisterNeighbors(); // Jiayi, 03/15/17

      //
      //Fields
      //
      uint64_t clockTuner;
      int total_ready_operands;
      int total_results_ready;
      int total_updates_received;
      // These map a clock cycle to the number of these events that occured in that
      // cycle for this cube
      map<long long, int> ready_operands_counts;
      map<long long, int> results_ready_counts;
      map<long long, int> updates_received_counts;

      vector<LinkSlave *> downLinkSlaves;
      vector<LinkMaster *> upLinkMasters;
      CrossbarSwitch *crossbarSwitch;
      vector<VaultController *> vaultControllers;
      vector<DRAM *> drams;

      // Ram & Jiayi, 03/13/17
      unsigned cubeID;
  };

}

#endif
