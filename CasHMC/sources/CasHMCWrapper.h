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
#if 0
#ifndef CASHMCWRAPPER_H
#define CASHMCWRAPPER_H

//CasHMCWrapper.h

#include <time.h>		//time
#include <sys/stat.h>	//mkdir
#include <errno.h>		//EEXIST
#include <unistd.h>		//access()
#include <sstream>		//stringstream
#include <fstream>		//ofstream
#include <vector>		//vector

#include "SimConfig.h"
#include "TranStatistic.h"
#include "HMCController.h"
#include "Link.h"
#include "HMC.h"

using namespace std;

namespace CasHMC
{

  class CasHMCWrapper : public TranStatistic
  {
    public:
      //
      //Functions
      //
      CasHMCWrapper();
      virtual ~CasHMCWrapper();
      bool ReceiveTran(TransactionType tranType, uint64_t addr, unsigned size, int cpu_id);
      bool ReceiveTran(Transaction *tran, int cpu_id);
      void Update();
      void DownLinkUpdate(bool lastUpdate);
      void UpLinkUpdate(bool lastUpdate);
      void PrintEpochHeader();
      void PrintSetting(struct tm t);
      void MakePlotData();
      void PrintEpochStatistic();
      void PrintFinalStatistic();

      //
      //Fields
      //
      //HMCController *hmcCont;
      //vector<Link *> downstreamLinks;
      //vector<Link *> upstreamLinks;
      //HMC *hmc;
      // Ram and Jiayi, 02/28/17
      vector<HMCController *> hmcConts;
      vector<Link *> hmcCntLinks;
      vector<vector<Link *> > hmcLinks;
      vector<HMC *> hmcs;

      uint64_t currentClockCycle;
      //uint64_t dramTuner;
      vector<uint64_t> dramTuner; // Ram and Jiayi, 02/28/17
      uint64_t downLinkTuner;
      uint64_t downLinkClock;
      uint64_t upLinkTuner;
      uint64_t upLinkClock;
      double linkPeriod;
      string logName;
      int logNum;

      unsigned cpu_link_ratio;
      unsigned cpu_link_tune;
      unsigned clockTuner_link;
      unsigned clockTuner_HMC;

      // Jiayi, for outstanding requests, 02/07
      uint64_t computeLat;
      uint64_t computeFinishCycle;
      vector<uint64_t> outstandRequests;

      //Temporary variable for plot data
      uint64_t hmcTransmitSizeTemp;
      vector<uint64_t> downLinkDataSizeTemp;
      vector<uint64_t> upLinkDataSizeTemp;

      //Output log files
      ofstream settingOut;
      ofstream debugOut;
      ofstream stateOut;
      ofstream plotDataOut;
      ofstream plotScriptOut;
      ofstream resultOut;
  };

}

#endif
#endif
