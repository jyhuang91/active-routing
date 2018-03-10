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

#ifndef LINK_H
#define LINK_H

//Link.h

#include <stdint.h>		//uint64_t

//#include "SingleVectorObject.h"
#include "DualVectorObject.h"
#include "SimConfig.h"

using namespace std;

namespace CasHMC
{
  //forward declaration
  class LinkMaster;
  class Link : public SimulatorObject
  {
    public:
      //
      //Functions
      //
      Link(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down, TranStatistic *statisP);
      Link(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down, TranStatistic *statisP, string headerPrefix);
      virtual ~Link();
      void Update() {};
      void Update(bool lastUpdate);
      void UpdateStatistic(Packet *packet);
      void NoisePacket(Packet *packet);
      void PrintState();

      //
      //Fields
      //
      unsigned linkID;
      unsigned localLinkID;
      bool downstream;
      unsigned errorProba;
      LinkMaster *linkMasterP;
      TranStatistic *statis;
      //SingleVectorObject<Packet> *linkSlaveP;
      DualVectorObject<Packet, Packet> *linkSlaveP;

      //Currently transmitting packet through link
      Packet *inFlightPacket;
      unsigned inFlightCountdown;
  };

}

#endif
