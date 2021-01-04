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

#ifndef VLPCROSSBARSWITCH_H
#define VLPCROSSBARSWITCH_H

//VLPCrossbarSwitch.h

#include <vector>   //vector
#include <map>      // map, Jiayi, 02/06
#include <deque>

#include "DualVectorObject.h"
#include "SimConfig.h"
#include "DRAMConfig.h"
#include "LinkMaster.h"
#include "RoutingFunction.h"
#include "InputBuffer.h"
#include "CrossbarSwitch.h"

using namespace std;

#define ROUND_ROBIN   0
#define CONTENT_AWARE 1
// may add more here...

namespace CasHMC
{

  class VLPCrossbarSwitch : public CrossbarSwitch
  {
    public:
      //
      //Functions
      //
      VLPCrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_);
      VLPCrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_, unsigned id, RoutingFunction *rf = NULL);
      virtual ~VLPCrossbarSwitch();
      virtual void Update();
      void UpdateDispatch(Packet* p);

      //
      //Fields
      //
      int numAdds;
      int numMults;
      int multVault;
      char dispatchPolicy;

      uint64_t numFlows;
  };

}

#endif
