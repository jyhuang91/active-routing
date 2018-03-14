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

#include "HMC.h"

namespace CasHMC
{

  HMC::HMC(ofstream &debugOut_, ofstream &stateOut_, unsigned id_, RoutingFunction *rf_):  // Ram & Jiayi, 03/13/17
    SimulatorObject(debugOut_, stateOut_), cubeID(id_)
  {
    clockTuner = 1;

    classID << "HMC" << cubeID;

    //Make class objects
    downLinkSlaves.reserve(NUM_LINKS);
    upLinkMasters.reserve(NUM_LINKS);
    for(int l=0; l<NUM_LINKS; l++) {
      if (SIM_TOPOLOGY == DFLY) {
        downLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, true, classID.str()));
        if (cubeID % 5 == 0 && cubeID / 5 == l) {
          upLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, false, classID.str()));
        } else {
          upLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, true, classID.str()));
        }
      } else if(SIM_TOPOLOGY == MESH){
        downLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, true));
        if(cubeID%4 == 0 || cubeID%4 == 3 || cubeID/4 == 0 || cubeID/4 == 3){
          upLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, false, classID.str()));
        }else{
          upLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, true, classID.str()));
        }
      } else {
        downLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, true));
        upLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, false));
      }
      downLinkSlaves[l]->linkSlaveID = cubeID * 4 + l;
      upLinkMasters[l]->linkMasterID = cubeID * 4 + l;
    }
    crossbarSwitch = new CrossbarSwitch(debugOut, stateOut, id_, rf_);
    vaultControllers.reserve(NUM_VAULTS);
    drams.reserve(NUM_VAULTS);
    for(int v=0; v<NUM_VAULTS; v++) {
      classID.str("");
      classID << "HMC" << cubeID;
      vaultControllers.push_back(new VaultController(debugOut, stateOut, v, classID.str()));
      classID << "_VC" << v;
      drams.push_back(new DRAM(debugOut, stateOut, v, vaultControllers[v], classID.str()));
    }

    //initialize refresh counters
    for(unsigned v=0; v<NUM_VAULTS; v++) {
      vaultControllers[v]->refreshCountdown = ((REFRESH_PERIOD/tCK)/NUM_VAULTS)*(v+1);
    }

    //Crossbar switch, and vault are linked each other by respective lanes
    for(int v=0; v<NUM_VAULTS; v++) {
      //Downstream
      crossbarSwitch->downBufferDest[v] = vaultControllers[v];
      vaultControllers[v]->dramP = drams[v];
      //Upstream
      vaultControllers[v]->upBufferDest = crossbarSwitch->inputBuffers[NUM_LINKS];
    }
  }

  HMC::~HMC()
  {
    for(int l=0; l<NUM_LINKS; l++) {
      delete downLinkSlaves[l];
      delete upLinkMasters[l];
    }
    downLinkSlaves.clear();
    upLinkMasters.clear();

    delete crossbarSwitch;
    crossbarSwitch = NULL;

    for(int v=0; v<NUM_VAULTS; v++) {
      delete vaultControllers[v];
      delete drams[v];
    }
    vaultControllers.clear();
    drams.clear();
  }

  //
  //Update the state of HMC
  //
  void HMC::Update()
  {	
    for(int l=0; l<NUM_LINKS; l++) {
      downLinkSlaves[l]->Update();
    }
    crossbarSwitch->Update();
    for(int v=0; v<NUM_VAULTS; v++) {
      vaultControllers[v]->Update();
    }
    for(int v=0; v<NUM_VAULTS; v++) {
      drams[v]->Update();
    }
    for(int l=0; l<NUM_LINKS; l++) {
      upLinkMasters[l]->Update();
    }
    clockTuner++;
    Step();
  }

  //
  //Print current state in state log file
  //
  void HMC::PrintState()
  {
    for(int l=0; l<NUM_LINKS; l++) {
      downLinkSlaves[l]->PrintState();
    }
    crossbarSwitch->PrintState();
    for(int v=0; v<NUM_VAULTS; v++) {
      vaultControllers[v]->PrintState();
    }
    for(int v=0; v<NUM_VAULTS; v++) {
      drams[v]->PrintState();
    }
    for(int l=0; l<NUM_LINKS; l++) {
      upLinkMasters[l]->PrintState();
    }
  }

  //
  // Jiayi, Register neighbor cube ID for crossbar, 03/15/17
  //
  void HMC::RegisterNeighbors()
  {
#ifdef DEBUG_NETCONNECT
    cout << "HMC#" << crossbarSwitch->cubeID << "'s neighbors <link, cubeID>: ";
#endif
    for (int l = 0; l < NUM_LINKS; ++l) {
      LinkSlave *linkSlaveP = dynamic_cast<LinkSlave *> (crossbarSwitch->upBufferDest[l]->linkP->linkSlaveP);
      assert(linkSlaveP);
      InputBuffer *neigh_ib = dynamic_cast<InputBuffer *> (linkSlaveP->downBufferDest);
      CrossbarSwitch *neigh_xbar = NULL;
      if (neigh_ib) {
        neigh_xbar = dynamic_cast<CrossbarSwitch *> (neigh_ib->xbar);
      }
      if (neigh_xbar) {
        crossbarSwitch->neighborCubeID[l] = neigh_xbar->cubeID;
      } else {
        crossbarSwitch->neighborCubeID[l] = crossbarSwitch->cubeID;
      }
#ifdef DEBUG_NETCONNECT
      cout << "<" << l << ", " << crossbarSwitch->neighborCubeID[l] << "> ";
#endif
    }
#ifdef DEBUG_NETCONNECT
    cout << endl;
#endif
  }

} //namespace CasHMC