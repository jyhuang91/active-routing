#ifndef NETWORK_H
#define NETWORK_H

// Network.h

//#include "CasHMCWrapper.h"
#include <time.h>     //time
#include <sys/stat.h> //mkdir
#include <errno.h>    //EEXIST
#include <unistd.h>   //access()
#include <sstream>    //stringstream
#include <fstream>    //ofstream
#include <vector>     //vector

#include "SimConfig.h"
#include "TranStatistic.h"
#include "HMCController.h"
#include "Link.h"
#include "HMC.h"
#include "RoutingFunction.h"

using namespace std;

namespace CasHMC
{

  class Network : public TranStatistic
  {
    public:
      //
      //Functions
      //
      Network(int dimension, string benchname);
      virtual ~Network();
      static Network *New(int dimension, TOPOLOGY topology, string benchname, double cpu_clk = 0.5); // cpu_clk in ns
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
      void Alloc();
      void PrintXbarBuffers();
      void MultiStep(uint64_t cycles);

      uint64_t get_tran_addr(Transaction *tran);//pritam added
      vector<pair<uint64_t, PacketCommandType> > &get_serv_trans(int cpu_id);//pritam added
      uint64_t get_tran_tag(Transaction *tran) { return tran->transactionID; }//pritam added

      //
      //Fields
      //
      // Ram and Jiayi, 02/28/17
      int ncpus;
      int nodes;

      RoutingFunction *rf;

      // Ram and Jiayi, 02/28/17
      vector<HMCController *> hmcConts;
      vector<Link *> hmcCntLinks;
      vector<vector<Link *> > hmcLinks;
      vector<HMC *> hmcs;

      map<int, long long> hist;

      vector<Link *> allLinks;

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
      string benchname;

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

      // Stats
      vector<uint64_t> opbufStalls;
      uint64_t totOpbufStalls;
      vector<uint64_t> numUpdates;
      vector<uint64_t> numOperands;
  };

  class DualHMC : public Network
  {
    public:
      DualHMC(int dimension, string benchname);
  };

  class MeshNet : public Network
  {
    public:
      MeshNet(int dimension, string benchname);
  };

  class DFly : public Network
  {
    public:
      DFly(int dimension, string benchname);
  };

  class DefaultHMC : public Network
  {
    public:
      DefaultHMC();
  };

}

#endif // NETWORK_H
