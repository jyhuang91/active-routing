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

#ifndef HMCCONTROLLER_H
#define HMCCONTROLLER_H

//HMCController.h

#include <vector>		//vector
#include <map>

#include "DualVectorObject.h"
#include "SimConfig.h"
#include "LinkMaster.h"
#include "LinkSlave.h"

using namespace std;

namespace CasHMC
{

  class HMCController : public DualVectorObject<Transaction, Packet>
  {
    public:
      //
      //Functions
      //
      HMCController(ofstream &debugOut_, ofstream &stateOut_);
      HMCController(ofstream &debugOut_, ofstream &stateOut_, unsigned id);
      virtual ~HMCController();
      void CallbackReceiveDown(Transaction *downEle, bool chkReceive);
      void CallbackReceiveUp(Packet *upEle, bool chkReceive);
      void Update();
      Packet *ConvTranIntoPacket(Transaction *tran);
      void PrintState();
	    vector<pair<uint64_t, PacketCommandType> > &get_serv_trans();	//pritam added:: returns the serv_transac array
      void PrintARTree(uint64_t dest_addr); // Jiayi, print active routing tree, 03/23/17
      void PrintARTree2(uint64_t dest_addr);

      //
      //Fields
      //
      vector<LinkMaster *> downLinkMasters;
      vector<LinkSlave *> upLinkSlaves;
      int inServiceLink;
      unsigned hcID;
      vector<uint64_t> responseQ; // Jiayi, 02/07
      map<uint64_t, pair<int, int> > gatherBarrier; // Jiayi, <dest_addr, <nthreads, count> >, 03/30/17
      vector<pair<uint64_t, PacketCommandType> > serv_trans;//pritam added
  };

}

#endif
