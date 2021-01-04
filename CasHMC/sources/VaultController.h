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

#include <stdint.h> //uint64_t
#include <vector>   //vector
#include <math.h>   //ceil()

#include "DualVectorObject.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "DRAMCommand.h"
#include "CommandQueue.h"
#include "DRAM.h"
using namespace std;

namespace CasHMC
{

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
      void CallbackReceiveArt(Packet *packet, bool chkReceive);
      bool ReturnCommand(DRAMCommand *retRead);
      virtual bool MakeRespondPacket(DRAMCommand *retCMD);
      virtual void Update();
      void UpdateCountdown();
      virtual bool ConvPacketIntoCMDs(Packet *packet);
      void AddressMapping(uint64_t physicalAddress, unsigned &bankAdd, unsigned &colAdd, unsigned &rowAdd);
      void EnablePowerdown();
      void PrintState();
      void PrintBuffers();

      // For VLP
      virtual int  OperandBufferStatus(Packet* p) { return -1; };
      virtual void FreeOperandBuffer(int i) {};

      uint64_t totalOperandRequests;

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
      vector<unsigned> pendingReadData; //Store Read packet TAG for return data
      vector<Packet *>pcuPacket;        //Store Read packet TAG for return data
      DualVectorObject<Packet, Packet> *upBufferDest;

      //Command and data to be transmitted to DRAM I/O
      DRAMCommand *cmdBus;
      unsigned cmdCyclesLeft;
      DRAMCommand *dataBus;
      unsigned dataCyclesLeft;
      vector<DRAMCommand *> writeDataToSend;
      vector<unsigned> writeDataCountdown;
  };

}

#endif
