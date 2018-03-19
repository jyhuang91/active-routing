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

#include <vector>		//vector
#include <map>      // map, Jiayi, 02/06
#include <deque>

#include "DualVectorObject.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "LinkMaster.h"
#include "RoutingFunction.h"
#include "InputBuffer.h"

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
	    CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_, unsigned id, RoutingFunction *rf); // Ram
      virtual ~CrossbarSwitch();
      void CallbackReceiveDown(Packet *downEle, bool chkReceive);
      void CallbackReceiveUp(Packet *upEle, bool chkReceive);
      void Update();
      void PrintState();
      void PrintBuffers();

      //
      //Fields
      //
      vector<DualVectorObject<Packet, Packet> *> downBufferDest;
      vector<LinkMaster *> upBufferDest;
      int inServiceLink;
      vector<unsigned> pendingSegTag;		//Store segment packet tag for returning
      vector<Packet *> pendingSegPacket;	//Store segment packets
      vector<InputBuffer *> inputBuffers;

      // Jiayi, extended for active router, 02/06
      // map<dest_addr(id), pair< pair<count, result>, <parent, vector<children_count> > > >
      // children count for each outgoing link, find the children ID in neighborCubeID
      map<uint64_t, pair< pair<unsigned, int>, pair<int, vector<int> > > > reserveTable;
      map<uint64_t, vector<bool> > childrenTable; // GET replicate, true: not sent yet, false: no child or sent
      map<uint64_t, Packet *> activeReturnBuffers;  // <dest_addr, packet for return>
      // Jiayi, two-memory operands, <dest_addr, < <src_addr1, ready?>, <src_addr2, ready?> > >, 03/24/17
      //map<uint64_t, vector<pair<pair<uint64_t, bool>, pair<uint64_t, bool> > > > operandBuffers;
      //map<uint64_t, deque<int> > freeOperandBufIDs;
      vector<pair<pair<uint64_t, bool>, pair<uint64_t, bool> > > operandBuffers;
      deque<int> freeOperandBufIDs;
      int operandBufSize;

      // Ram & Jiayi, 03/13/17
      unsigned cubeID;
      RoutingFunction *rf;
      vector<int> neighborCubeID; // <link(0,1,...,NUM_LINKS-1), cubeID>
      int upLink;
      int downLink;
      uint64_t opbufStalls;
      uint64_t numUpdates;
      uint64_t numOperands;
  };

}

#endif
