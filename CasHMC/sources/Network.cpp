#include "Network.h"

using namespace std;

extern long numSimCycles;				//The number of CPU cycles to be simulated
extern string traceType;				//Trace type ('random' or 'file')
extern double memUtil;					//Frequency of requests - 0.0 = no requests, 1.0 = as fast as possible
extern double rwRatio;					//(%) The percentage of reads in request stream
extern string traceFileName;			//Trace file name

namespace CasHMC
{

  Network::Network(int dimension)
  {
    //
    //Class variable initialization
    //
    currentClockCycle = 0;
    //dramTuner = 1;// modified by Jiayi, 02/28/16
    downLinkTuner = 1;
    downLinkClock = 1;
    upLinkTuner = 1;
    upLinkClock = 1;
    linkPeriod = (1/LINK_SPEED);

    // Jiayi, for prelim experiment, 02/07
    computeLat = 200;
    computeFinishCycle = 0;

    //Check CPU clock cycle and link speed
    if(CPU_CLK_PERIOD < linkPeriod) {	//Check CPU clock cycle and link speed
      ERROR(" == Error - WRONG CPU clock cycle or link speed (CPU_CLK_PERIOD should be bigger than (1/LINK_SPEED))");
      ERROR(" == Error - CPU_CLK_PERIOD : "<<CPU_CLK_PERIOD<<"  1/LINK_SPEED : "<<linkPeriod);
      exit(0);
    }

    //
    // Log files generation
    //
    int status = 1;
    status = mkdir("result", S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
    benchname.clear();

    time_t now;
    struct tm t;
    time(&now);
    t = *localtime(&now);

    cout<<endl<<"****************************************************************"<<endl;
    cout<<"*                      CasHMC version 1.1                      *"<<endl;
    cout<<"*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
      <<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
      <<"            *"<<endl;
#ifndef DEBUG_LOG		
    cout<<"*                                                              *"<<endl;
    cout<<"*  Module abbreviation info.                                   *"<<endl;
    cout<<"*  [HC]   : HMC Controller                                     *"<<endl;
    cout<<"*  [LM_U/Di] : ith Up/Downstream Link Master                   *"<<endl;
    cout<<"*  [LK_U/Di] : ith Up/Downstream Link                          *"<<endl;
    cout<<"*  [LS_U/Di] : ith Up/Downstream Link Slave                    *"<<endl;
    cout<<"*  [CS]   : Crossbar Switch                                    *"<<endl;
    cout<<"*  [VC_j] : jth Vault Controller                               *"<<endl;
    cout<<"*  [CQ_k] : k-bank Command Queue                               *"<<endl;
    cout<<"*  [DR_j] : jth DRAM                                           *"<<endl;
#endif		
    cout<<"****************************************************************"<<endl<<endl;
    cout.setf(ios::left); 
    cout<<" = Log folder generating information"<<endl;
    if(status == 0) {
      cout<<"   Making result folder success (./result/)"<<endl;
    }
    else {
      if(errno == EEXIST) {
        cout<<"   Result folder already exists (./result/)"<<endl;
        status = 0;
      }
      else {
        cout<<"   Making result folder fail"<<endl;
        cout<<"   Debug and result files will be generated in CasHMC folder (./)"<<endl<<endl;
      }
    }

    if(status == 0)
      logName = "result/CasHMC";
    else
      logName = "CasHMC";

    unsigned int ver_num = 0;
    stringstream temp_vn;
    while(1) { 
      logName += "_no";
      temp_vn << ver_num;
      logName += temp_vn.str();
      logName += "_setting.log";

      if(access(logName.c_str(), 0) == -1)	break;
      else {
        logName.erase(logName.find("_no"));
        temp_vn.str( string() );
        temp_vn.clear();
        ver_num++;
      }
    }
    logName.erase(logName.find("_no"));
    cout.setf(ios::left);
    cout<<endl<<"   === Simulation start === "<<endl;

    if(BANDWIDTH_PLOT) {
      hmcTransmitSizeTemp = 0;
      downLinkDataSizeTemp = vector<uint64_t>(NUM_LINKS, 0);
      upLinkDataSizeTemp = vector<uint64_t>(NUM_LINKS, 0);

      //Plot data file initialization
      string plotName = "graph/CasHMC_plot_no";
      plotName += temp_vn.str();
      plotName += ".dat";
      plotDataOut.open(plotName.c_str());
      plotDataOut.setf(ios::left);
      cout<<"  [ "<<plotName<<" ] is generated"<<endl;

      plotDataOut<<setw(8)<<setfill(' ')<<"#Epoch";
      for(int i=0; i<NUM_LINKS; i++) {
        stringstream linkNum;
        linkNum << i;
        string plotLink = "Link[" + linkNum.str() + ']';
        plotDataOut<<setw(10)<<setfill(' ')<<plotLink;
      }
      plotDataOut<<setw(10)<<setfill(' ')<<"HMC bandwidth"<<std::endl;

      //Plot script file initialization
      plotName.erase(plotName.find(".dat"));
      plotName += ".gnuplot";
      plotScriptOut.open(plotName.c_str());
      cout<<"  [ "<<plotName<<" ] is generated"<<endl;

      plotName = "CasHMC_plot_no";
      plotName += temp_vn.str();
      plotName += ".png";

      plotScriptOut<<"set term png size 800, 500 font \"Times-Roman,\""<<std::endl;
      plotScriptOut<<"set output '"<<plotName<<"'"<<std::endl;
      plotScriptOut<<"set title \"CasHMC bandwidth graph\" font \",18\""<<std::endl;
      plotScriptOut<<"set autoscale"<<std::endl;
      plotScriptOut<<"set grid"<<std::endl;
      plotScriptOut<<"set xlabel \"Simulation plot epoch\""<<std::endl;
      plotScriptOut<<"set ylabel \"Bandwidth [GB/s]\""<<std::endl;
      plotScriptOut<<"set key left box"<<std::endl;
      plotScriptOut<<"plot  \"CasHMC_plot_no"<<ver_num<<".dat\" using 1:"<<NUM_LINKS+2<<" title 'HMC' with lines lw 2 , \\"<<std::endl;
      for(int i=0; i<NUM_LINKS; i++) {
        plotScriptOut<<"      \"CasHMC_plot_no"<<ver_num<<".dat\" using 1:"<<i+2<<" title 'Link["<<i<<"]' with lines lw 1 , \\"<<std::endl;
      }
    }
/*
    settingOut.open(logName.c_str());
    PrintSetting(t);
    cout<<"  [ "<<logName<<" ] is generated"<<endl;

    logName.erase(logName.find("_s"));
*/
    logNum = 0;
    PrintEpochHeader();
  }

  Network::~Network()
  {
    double elapsedTime = (double)(currentClockCycle*CPU_CLK_PERIOD*1E-9);
    cout << " ## VLTCtrller DRAM_rd_bw:"<<(double)VaultController::DRAM_rd_data/elapsedTime/(1<<30)<< "GBps DRAM_wr_bw:"<<(double)VaultController::DRAM_wr_data/elapsedTime/(1<<30)<<"GBps DRAM_act_bw:" << (double)VaultController::DRAM_act_data/elapsedTime/(1<<30)<<"GBps"<<endl;
    PrintEpochStatistic();
    PrintFinalStatistic();
    debugOut.flush();		debugOut.close();
    stateOut.flush();		stateOut.close();
    plotDataOut.flush();	plotDataOut.close();
    plotScriptOut.flush();	plotScriptOut.close();
    resultOut.flush();		resultOut.close();

    //Jiayi, 02/28/17
    for (int i = 0; i < hmcConts.size(); ++i) {
      delete hmcConts[i];
    }
    hmcConts.clear();
    for (int l = 0; l < hmcCntLinks.size(); ++l) {
      delete hmcCntLinks[l];
    }
    hmcCntLinks.clear();
    for (int i = 0; i < hmcs.size(); ++i) {
      delete hmcs[i];
      for (int j = 0; j < hmcLinks[i].size(); ++j) {
        delete hmcLinks[i][j];
      }
      hmcLinks[i].clear();
    }
    hmcs.clear();
    hmcLinks.clear();
    allLinks.clear();

    outstandRequests.clear();   // Jiayi, 02/07

    if (rf) {
      delete rf;
      rf = NULL;
    }
  }

  Network *Network::New(int dimension, TOPOLOGY topology, double cpu_clk)
  {
    SIM_TOPOLOGY = topology;
    CPU_CLK_PERIOD = cpu_clk;
    Network *result;
    switch (topology) {
      case DUAL_HMC:
        result = new DualHMC(dimension);
        break;
      case MESH:
        cout << "Created a Mesh topology with dimension " << dimension << endl;
        result = new MeshNet(dimension);
        break;
      case DFLY:
        cout << "Created a DragonFly topology with dimension " << dimension << endl;
        result = new DFly(dimension);
        break;
      case DEFAULT:
        cout << "Createtd Default HMC" << endl;
        result = new DefaultHMC();
        break;
      default:
        cout << "This mode not yet implemented, create the default one" << endl;
        result = new DefaultHMC();
        break;
    }

    for (int i = 0; i < result->hmcs.size(); i++) {
      result->hmcs[i]->crossbarSwitch->transtat = result;
    }
    return result;
  }

  void Network::Alloc()
  {
    // Stats
    opbufStalls.resize(nodes, 0);
    totOpbufStalls = 0;
    numUpdates.resize(nodes, 0);
    numOperands.resize(nodes, 0);

    //hmcConts.resize(ncpus);
    //hmcCntLinks.resize(ncpus);
    dramTuner.resize(nodes, 1);
    //hmcLinks.resize(nodes);
    //hmcs.resize(nodes);
    rf = RoutingFunction::New(nodes, SIM_TOPOLOGY);

    switch (SIM_TOPOLOGY) {
      case DUAL_HMC:
      case MESH:
        CPU_LINKS = 1;
        assert(NUM_LINKS == 4);
        /*for (int i = 0; i < ncpus; ++i) {
          stringstream headerPrefix;
          headerPrefix << "HC" << i;
          hmcConts.push_back(new HMCController(debugOut, stateOut, i));
          hmcCntLinks.push_back(new Link(debugOut, stateOut, 0, true, this, headerPrefix.str()));
        }*/

        hmcLinks.resize(nodes);
        for (int i = 0; i < nodes; ++i) {
          hmcs.push_back(new HMC(debugOut, stateOut, i, rf));
          //hmcLinks[i].resize(NUM_LINKS);
          for (int l = 0; l < NUM_LINKS; ++l) {
            //hmcLinks[i].push_back(new Link(debugOut, stateOut, l, false, this));
            stringstream headerPrefix;
            headerPrefix << "HMC" << i;
            if ((i == 0 && l == 0 )|| (i == 3 && l == 1) || (i == 12 && l == 2) || (i == 15 && l == 3)) { //Works only for 4X4 configuaration
              hmcLinks[i].push_back(new Link(debugOut, stateOut, l, false, this, headerPrefix.str()));
            } else {
              hmcLinks[i].push_back(new Link(debugOut, stateOut, l, true, this, headerPrefix.str()));
            }
            hmcLinks[i][l]->linkID = 4*i + l;
            allLinks.push_back(hmcLinks[i][l]);
          }
        }

        for (int i = 0; i < ncpus; ++i) {
          stringstream headerPrefix;
          headerPrefix << "HC" << i;
          hmcConts.push_back(new HMCController(debugOut, stateOut, i));
          hmcCntLinks.push_back(new Link(debugOut, stateOut, 0, true, this, headerPrefix.str()));
          hmcCntLinks[i]->linkID = 64 + i;
          allLinks.push_back(hmcCntLinks[i]);
        }
        break;
      case DFLY:
        CPU_LINKS = 1;
        assert(NUM_LINKS == 4);
        /*for (int i = 0; i < ncpus; ++i) {
          stringstream headerPrefix;
          headerPrefix << "HC" << i;
          hmcConts.push_back(new HMCController(debugOut, stateOut, i));
          hmcCntLinks.push_back(new Link(debugOut, stateOut, 0, true, this, headerPrefix.str()));
        }*/

        hmcLinks.resize(nodes);
        for (int i = 0; i < nodes; ++i) {
          hmcs.push_back(new HMC(debugOut, stateOut, i, rf));
          //hmcLinks[i].resize(NUM_LINKS);
          for (int l = 0; l < NUM_LINKS; ++l) {
            //hmcLinks[i].push_back(new Link(debugOut, stateOut, l, false, this));
            stringstream headerPrefix;
            headerPrefix << "HMC" << i;
            if (i % 5 == 0 && i / 5 == l) {
              hmcLinks[i].push_back(new Link(debugOut, stateOut, l, false, this, headerPrefix.str()));
            } else {
              hmcLinks[i].push_back(new Link(debugOut, stateOut, l, true, this, headerPrefix.str()));
            }
            hmcLinks[i][l]->linkID = 4*i + l;
            allLinks.push_back(hmcLinks[i][l]);
          }
        }

        for (int i = 0; i < ncpus; ++i) {
          stringstream headerPrefix;
          headerPrefix << "HC" << i;
          hmcConts.push_back(new HMCController(debugOut, stateOut, i));
          hmcCntLinks.push_back(new Link(debugOut, stateOut, 0, true, this, headerPrefix.str()));
          hmcCntLinks[i]->linkID = 64 + i;
          allLinks.push_back(hmcCntLinks[i]);
        }
        break;
      case DEFAULT:
      default:
        CPU_LINKS = 4;
        assert(NUM_LINKS == 4);
        //rf = NULL; // FIXME
        //rf = RoutingFunction::New(nodes, DFLY);

        // Make class objects
        //hmcLinks[0].resize(NUM_LINKS);
        hmcLinks.resize(1);
        //hmcCntLinks.reserve(NUM_LINKS);
        //hmcLinks[0].reserve(NUM_LINKS);
        for (int l = 0; l < NUM_LINKS; ++l) {
          hmcCntLinks.push_back(new Link(debugOut, stateOut, l, true, this));
          hmcLinks[0].push_back(new Link(debugOut, stateOut, l, false, this));
        }
        hmcConts.push_back(new HMCController(debugOut, stateOut));
        hmcs.push_back(new HMC(debugOut, stateOut, 0, rf));
        break;
    }
  }

  DualHMC::DualHMC(int dimension)
    : Network(dimension)
  {
    ncpus = 2;
    nodes = 2*dimension; //Shall be changend to dim*dim

    Alloc();

    for (int i = 0; i < ncpus; ++i) { 
      hmcConts[i]->downLinkMasters[0]->linkP = hmcCntLinks[i];
      hmcConts[i]->downLinkMasters[0]->localLinkSlave = hmcConts[i]->upLinkSlaves[0];
      hmcCntLinks[i]->linkMasterP = hmcConts[i]->downLinkMasters[0];
      hmcCntLinks[i]->linkSlaveP = hmcs[i]->downLinkSlaves[0];
      hmcConts[i]->upLinkSlaves[0]->upBufferDest = hmcConts[i];
      hmcConts[i]->upLinkSlaves[0]->localLinkMaster = hmcConts[i]->downLinkMasters[0];
    }

    for (int i = 0; i < nodes; ++i) {
      for (int l = 0; l < NUM_LINKS; ++l) {
        hmcs[i]->downLinkSlaves[l]->downBufferDest = hmcs[i]->crossbarSwitch;
        hmcs[i]->downLinkSlaves[l]->localLinkMaster = hmcs[i]->upLinkMasters[l];
        hmcs[i]->crossbarSwitch->upBufferDest[l] = hmcs[i]->upLinkMasters[l];
        hmcs[i]->upLinkMasters[l]->linkP = hmcLinks[i][l];
        hmcs[i]->upLinkMasters[l]->localLinkSlave = hmcs[i]->downLinkSlaves[l];
        hmcLinks[i][l]->linkMasterP = hmcs[i]->upLinkMasters[l];
        if(l==0){
          hmcLinks[i][l]->linkSlaveP = hmcConts[i]->upLinkSlaves[0];
        }else{
          hmcLinks[i][l]->linkSlaveP = hmcs[((i+1)%nodes)]->downLinkSlaves[l];
        }
      }
    }

    for (int i = 0; i < hmcs.size(); ++i) {
      hmcs[i]->RegisterNeighbors();
    }
  }

  DFly::DFly(int dimension)
    : Network(dimension)
  {
    ncpus = 4;//dimension;
    nodes = dimension * dimension;

    Alloc();

    for(int i=0; i<ncpus; ++i){
      hmcConts[i]->downLinkMasters[0]->linkP = hmcCntLinks[i];
      hmcConts[i]->downLinkMasters[0]->localLinkSlave = hmcConts[i]->upLinkSlaves[0];
      hmcCntLinks[i]->linkMasterP = hmcConts[i]->downLinkMasters[0];
      //hmcCntLinks[i]->linkSlaveP = hmcs[i * 5]->downLinkSlaves[i];
      // hmcCntLinks[i]->linkSlaveP = hmc[i]->downLinkSlaves[0];
      hmcConts[i]->upLinkSlaves[0]->upBufferDest = hmcConts[i];
      hmcConts[i]->upLinkSlaves[0]->localLinkMaster = hmcConts[i]->downLinkMasters[0];
    }

    for (int i = 0; i < nodes; ++i) {
      for(int l = 0; l<NUM_LINKS; ++l){
        hmcs[i]->downLinkSlaves[l]->downBufferDest = hmcs[i]->crossbarSwitch->inputBuffers[l];
        hmcs[i]->downLinkSlaves[l]->localLinkMaster = hmcs[i]->upLinkMasters[l];
        hmcs[i]->crossbarSwitch->upBufferDest[l] = hmcs[i]->upLinkMasters[l];
        hmcs[i]->upLinkMasters[l]->linkP = hmcLinks[i][l];
        hmcs[i]->upLinkMasters[l]->localLinkSlave = hmcs[i]->downLinkSlaves[l];
        hmcLinks[i][l]->linkMasterP = hmcs[i]->upLinkMasters[l];
      }
    }

    vector<vector<LinkMaster *> > LM;
    vector<vector<LinkSlave *> > LS;
    vector<vector<LinkMaster *> > LM_next;
    vector<vector<LinkSlave *> > LS_next;
    int n = nodes;
    LM.resize(n); 
    LS.resize(n);
    for(int i=0; i<n; i++){
      LS[i].resize(NUM_LINKS);
      LM[i].resize(NUM_LINKS);
      for(int l=0; l<NUM_LINKS; l++){
        LM[i][l] = hmcs[i]->upLinkMasters[l]; 
        LS[i][l] = hmcs[i]->downLinkSlaves[l]; 
      }
    }

    while(n > 1){
      n /= 4;    // number of QUADS
      LM_next.resize(n);
      LS_next.resize(n);
      for(int i=0;i<n;++i){
        LM_next[i].resize(NUM_LINKS);
        LS_next[i].resize(NUM_LINKS);
        for(int j=0;j<4;j++){
          LM_next[i][j] = LM[(i*4)+j][j]; // next is for inter-group links, this quad as virtual router
          LS_next[i][j] = LS[(i*4)+j][j];
          for(int l = 0; l<NUM_LINKS; ++l){
            if(j != l){
              LM[(i*4)+j][l]->linkP->linkSlaveP = LS[(i*4)+l][j]; // connection of intra-group links
            }
          }
        }
      }

      // update for next upper hierarchy (inter-group)
      LM.resize(n);
      LS.resize(n);
      for(int i=0;i<n;++i){
        LM[i].resize(NUM_LINKS);
        LS[i].resize(NUM_LINKS);
        for(int j=0;j<NUM_LINKS;++j){
          LM[i][j] = LM_next[i][j];
          LS[i][j] = LS_next[i][j];
        }
      }
    }

    for(int i=0; i<ncpus; ++i){ 
      hmcConts[i]->downLinkMasters[0]->linkP->linkSlaveP = LS[0][i];
      LM[0][i]->linkP->linkSlaveP = hmcConts[i]->upLinkSlaves[0];
    }

    for (int i = 0; i < hmcs.size(); ++i) {
      hmcs[i]->RegisterNeighbors();
    }

#ifdef DEBUG_NETCONNECT
    // print link connections
    for (int i = 0; i < ncpus; i++) {
      cout << "HMC controller link " << i << ": " << hmcCntLinks[i]->header << endl;
      cout << "    LinkMaster: " << hmcCntLinks[i]->linkMasterP->header << endl;
      cout << "    LinkSlave:  " << hmcCntLinks[i]->linkSlaveP->header << endl;
    }
    for (int i = 0; i < nodes; i++) {
      cout << "HMC " << i << ":" << endl;
      for (int l = 0; l < NUM_LINKS; l++) {
        cout << " -- Link " << l << ":" << hmcLinks[i][l]->header << endl;
        cout << "    LinkMaster: " << hmcLinks[i][l]->linkMasterP->header << endl;
        cout << "    LinkSlave:  " << hmcLinks[i][l]->linkSlaveP->header << endl;
      }
    }
#endif
  }

  MeshNet::MeshNet(int dimension)
    : Network(dimension)
  {
    ncpus = dimension * 4;
    nodes = dimension * dimension;

    Alloc();

    for (int i = 0; i < ncpus; ++i) {
      hmcConts[i]->downLinkMasters[0]->linkP = hmcCntLinks[i];
      hmcConts[i]->downLinkMasters[0]->localLinkSlave = hmcConts[i]->upLinkSlaves[0];
      hmcCntLinks[i]->linkMasterP = hmcConts[i]->downLinkMasters[0];
      hmcConts[i]->upLinkSlaves[0]->upBufferDest = hmcConts[i];
      hmcConts[i]->upLinkSlaves[0]->localLinkMaster = hmcConts[i]->downLinkMasters[0];
    }

    for (int i = 0; i < nodes; ++i) {
      for (int l = 0; l < NUM_LINKS; ++l) {
        hmcs[i]->downLinkSlaves[l]->downBufferDest = hmcs[i]->crossbarSwitch->inputBuffers[l];
        //hmcs[i]->downLinkSlaves[l]->downBufferDest = hmcs[i]->crossbarSwitch;
        hmcs[i]->downLinkSlaves[l]->localLinkMaster = hmcs[i]->upLinkMasters[l];
        hmcs[i]->crossbarSwitch->upBufferDest[l] = hmcs[i]->upLinkMasters[l];
        hmcs[i]->upLinkMasters[l]->linkP = hmcLinks[i][l];
        hmcs[i]->upLinkMasters[l]->localLinkSlave = hmcs[i]->downLinkSlaves[l];
        hmcLinks[i][l]->linkMasterP = hmcs[i]->upLinkMasters[l];
      }
    }

    // mesh topology connection
    for (int y = 0; y < dimension; ++y) {
      for (int x = 0; x < dimension; ++x) {
        int pos = dimension * y + x;

        // connect left LinkMaster to right LinkSlave
        if (x > 0) {
          hmcs[pos]->upLinkMasters[LEFT_LINK]->linkP->linkSlaveP =
            hmcs[pos-1]->downLinkSlaves[RIGHT_LINK];
        } else {
          hmcs[pos]->upLinkMasters[LEFT_LINK]->linkP->linkSlaveP =
            hmcConts[y]->upLinkSlaves[0];
          hmcConts[y]->downLinkMasters[0]->linkP->linkSlaveP =
            hmcs[pos]->downLinkSlaves[0];
        }

        // connect top LinkMaster to bottom LinkSlave
        if (y > 0) {
          hmcs[pos]->upLinkMasters[TOP_LINK]->linkP->linkSlaveP =
            hmcs[pos - dimension]->downLinkSlaves[BOT_LINK];
        } else {
          hmcs[pos]->upLinkMasters[TOP_LINK]->linkP->linkSlaveP =
            hmcConts[dimension + x]->upLinkSlaves[0];
          hmcConts[dimension + x]->downLinkMasters[0]->linkP->linkSlaveP =
            hmcs[pos]->downLinkSlaves[1];
        }

        // connect right LinkMaster to left LinkSlave
        if (x < dimension - 1) {
          hmcs[pos]->upLinkMasters[RIGHT_LINK]->linkP->linkSlaveP =
            hmcs[pos+1]->downLinkSlaves[LEFT_LINK];
        } else {
          hmcs[pos]->upLinkMasters[RIGHT_LINK]->linkP->linkSlaveP =
            hmcConts[2*dimension + y]->upLinkSlaves[0];
          hmcConts[2*dimension + y]->downLinkMasters[0]->linkP->linkSlaveP =
            hmcs[pos]->downLinkSlaves[2];
        }

        // connect bottom LinkMaster to top LinkSlave
        if (y < dimension - 1) {
          hmcs[pos]->upLinkMasters[BOT_LINK]->linkP->linkSlaveP = 
            hmcs[pos + dimension]->downLinkSlaves[TOP_LINK];
        } else {
          hmcs[pos]->upLinkMasters[BOT_LINK]->linkP->linkSlaveP =
            hmcConts[3*dimension + x]->upLinkSlaves[0];
          hmcConts[3*dimension + x]->downLinkMasters[0]->linkP->linkSlaveP =
            hmcs[pos]->downLinkSlaves[3];
        }
      }
    }

    for (int i = 0; i < hmcs.size(); ++i) {
      hmcs[i]->RegisterNeighbors();
    }
  }

  DefaultHMC::DefaultHMC()
    : Network(0)
  {
    ncpus = 1;
    nodes = 1;

    Alloc();

    //Link master, Link, and Link slave are linked each other by respective lanes
    for (int l = 0; l < NUM_LINKS; ++l) {
      // Downstream
      hmcConts[0]->downLinkMasters[l]->linkP = hmcCntLinks[l];
      hmcConts[0]->downLinkMasters[l]->localLinkSlave = hmcConts[0]->upLinkSlaves[l];
      hmcCntLinks[l]->linkMasterP = hmcConts[0]->downLinkMasters[l];
      hmcCntLinks[l]->linkSlaveP = hmcs[0]->downLinkSlaves[l];
      hmcs[0]->downLinkSlaves[l]->downBufferDest = hmcs[0]->crossbarSwitch;
      hmcs[0]->downLinkSlaves[l]->localLinkMaster = hmcs[0]->upLinkMasters[l];
      // Upstream
      hmcs[0]->crossbarSwitch->upBufferDest[l] = hmcs[0]->upLinkMasters[l];
      hmcs[0]->upLinkMasters[l]->linkP = hmcLinks[0][l];
      hmcs[0]->upLinkMasters[l]->localLinkSlave = hmcs[0]->downLinkSlaves[l];
      hmcLinks[0][l]->linkMasterP = hmcs[0]->upLinkMasters[l];
      hmcLinks[0][l]->linkSlaveP = hmcConts[0]->upLinkSlaves[l];
      hmcConts[0]->upLinkSlaves[l]->upBufferDest = hmcConts[0];
      hmcConts[0]->upLinkSlaves[l]->localLinkMaster = hmcConts[0]->downLinkMasters[l];
    }

    for (int i = 0; i < hmcs.size(); ++i) {
      hmcs[i]->RegisterNeighbors();
    }
  }

  //
  //Check buffer available space and receive transaction
  //
  bool Network::ReceiveTran(TransactionType tranType, uint64_t addr, unsigned size, int cpu_id = 0)
  {
    Transaction *newTran = new Transaction(tranType, addr, size, this, 0, 0);

    if(hmcConts[cpu_id]->ReceiveDown(newTran)) {
      //if (newTran->transactionType == ACTIVE_ADD) {  // Jiayi, 02/07
      //    outstandRequests.push_back(newTran->src_address);
      //}
      if (newTran->transactionType == ACTIVE_GET) {    // Jiayi, 02/09
        outstandRequests.push_back(newTran->dest_address);
      }
      DE_CR(ALI(18)<<" (BUS)"<<ALI(15)<<*newTran<<"Down) SENDING transaction to HMC controller (HC"<<cpu_id<<")");
      return true;
    }
    else {
      newTran->ReductGlobalID();
      delete newTran->trace;
      delete newTran;
      return false;
    }
  }

  bool Network::ReceiveTran(Transaction *tran, int cpu_id = 0)
  {
    if (hmcConts[cpu_id] == NULL) {
      cout << "NO HMC Controller" << endl;
      assert(0);
    }
    if(hmcConts[cpu_id]->ReceiveDown(tran)) {

      if (tran->transactionType == ACTIVE_GET) {    // Jiayi, 02/09
        outstandRequests.push_back(tran->dest_address);
      }
      DE_CR(ALI(18)<<" (BUS)"<<ALI(15)<<*tran<<"Down) SENDING transaction to HMC controller (HC"<<cpu_id<<")");
      return true;
    }
    else {
      return false;
    }
  }

  //this function is called from McSim HMCController 
  uint64_t Network::get_tran_addr(Transaction *tran)
  {//pritam added
    switch (tran->transactionType) {
      case ACTIVE_ADD:
      case ACTIVE_MULT:
      case ACTIVE_GET:
        return tran->dest_address;
      default:
        return tran->address;
        //return (tran->address << 30) >> 30;
    }
  }

  vector<pair<uint64_t, PacketCommandType> > &Network::get_serv_trans(int cpu_id)
  {//pritam added
    return hmcConts[cpu_id]->get_serv_trans();
  }

  //
  //Updates the state of HMC Wrapper
  //
  void Network::Update()
  {
    if(BANDWIDTH_PLOT && currentClockCycle>0 && currentClockCycle%PLOT_SAMPLING == 0) {
      MakePlotData();
    }

#ifdef DEBUG_LOG
    if(currentClockCycle>0 && currentClockCycle%LOG_EPOCH == 0) {
      cout<<"\n   === Simulation ["<<currentClockCycle/LOG_EPOCH<<"] epoch starts  ( CPU clk:"<<currentClockCycle<<" ) ===   "<<endl;
      PrintEpochStatistic();
      if(DEBUG_SIM) {
        debugOut.flush();	debugOut.close();
      }
      if(STATE_SIM) {
        stateOut.flush();	stateOut.close();
      }
      if(DEBUG_SIM || STATE_SIM) {
        PrintEpochHeader();
      }
      //PrintXbarBuffers();
    }
#endif

    //
    //update all class (HMC controller, links, and HMC block)
    //Roughly synchronize CPU clock cycle, link speed, and HMC clock cycle
    //
    for (int i = 0; i < hmcConts.size(); ++i) { // Jiayi modified, 02/28/17
      hmcConts[i]->Update();
      // Jiayi, prelim experiment, 02/07
      if (hmcConts[i]->responseQ.size() > 0) {
        uint64_t response = hmcConts[i]->responseQ[0];
        hmcConts[i]->responseQ.erase(hmcConts[i]->responseQ.begin());
        int found = -1;
        for (int i = 0; i < outstandRequests.size(); i++) {
          if (response == outstandRequests[i]) {
            found = i;
            break;
          }
        }
        if (found == -1 || found >= outstandRequests.size()) {
          cout << " == Error:" << (void *) response << " cannot find a matched request (CurrentClock: " << currentClockCycle << ")" << endl;
          exit(0);
        } else {
          outstandRequests.erase(outstandRequests.begin()+found);
          computeFinishCycle = currentClockCycle;
        }
      }
    }

    for (int i = 0; i < hmcConts.size(); ++i) { // Jiayi, 03/12/17
      //Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
      for(int l=0; l<CPU_LINKS; l++) {
        hmcConts[i]->downLinkMasters[l]->Update();
      }
    }

    //Downstream links update at CPU clock cycle (depending on ratio of CPU cycle to Link cycle)
    while(CPU_CLK_PERIOD*downLinkTuner > linkPeriod*(downLinkClock + 1)) {
      DownLinkUpdate(false);
    }
    if(CPU_CLK_PERIOD*downLinkTuner == linkPeriod*(downLinkClock + 1)) {
      DownLinkUpdate(false);
      downLinkTuner = 0;
      downLinkClock = 0;
    }
    DownLinkUpdate(true);

    //HMC update at CPU clock cycle, modifed by Jiayi, 02/28/17
    for(int i = 0; i < hmcs.size(); ++i){
      if(CPU_CLK_PERIOD <= tCK) {
        if(CPU_CLK_PERIOD*dramTuner[i] > tCK*hmcs[i]->clockTuner) {
          hmcs[i]->Update();
        }
        else if(CPU_CLK_PERIOD*dramTuner[i] == tCK*hmcs[i]->clockTuner) {
          dramTuner[i] = 0;
          hmcs[i]->clockTuner = 0;
          hmcs[i]->Update();
        }
      }
      else {
        while(CPU_CLK_PERIOD*dramTuner[i] > tCK*(hmcs[i]->clockTuner + 1)) {
          hmcs[i]->Update();
        }
        if(CPU_CLK_PERIOD*dramTuner[i] == tCK*(hmcs[i]->clockTuner + 1)) {
          hmcs[i]->Update();
          dramTuner[i] = 0;
          hmcs[i]->clockTuner = 0;
        }
        hmcs[i]->Update();
      }
    }

    //Upstream links update at CPU clock cycle (depending on ratio of CPU cycle to Link cycle)
    while(CPU_CLK_PERIOD*upLinkTuner > linkPeriod*(upLinkClock + 1)) {
      UpLinkUpdate(false);
    }
    if(CPU_CLK_PERIOD*upLinkTuner == linkPeriod*(upLinkClock + 1)) {
      UpLinkUpdate(false);
      upLinkTuner = 0;
      upLinkClock = 0;
    }
    UpLinkUpdate(true);

    //Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
    for (int i = 0; i < hmcConts.size(); ++i) { // Jiayi modified, 02/28/16
      for(int l=0; l<CPU_LINKS; l++) {
        hmcConts[i]->upLinkSlaves[l]->Update();
      }
    }

#ifndef DEBUG_LOG
    cout<<"ifndef DEBUG_LOG"<<endl;
#endif
    //Print respective class state, Jiayi modified, 02/28/17
    for (int i = 0; i < hmcConts.size(); ++i) {
      hmcConts[i]->PrintState();
      for(int l=0; l<CPU_LINKS; l++) {
        hmcConts[i]->downLinkMasters[l]->PrintState();
      }
    }
    for(int l=0; l<hmcCntLinks.size(); l++) {
      hmcCntLinks[l]->PrintState();
    }
    for (int i = 0; i < hmcs.size(); ++i) {
      hmcs[i]->PrintState();
      for(int l=0; l<NUM_LINKS; l++) {
        hmcLinks[i][l]->PrintState();
      }
    }
    for (int i = 0; i < hmcConts.size(); ++i) {
      for(int l=0; l<CPU_LINKS; l++) {
        hmcConts[i]->upLinkSlaves[l]->PrintState();
      }
    }

    currentClockCycle++;
    downLinkTuner++;
    upLinkTuner++;
    for (int n = 0; n < hmcs.size(); ++n) {
      dramTuner[n]++;
    }
    DE_ST("\n---------------------------------------[ CPU clk:"<<currentClockCycle<<" / HMC clk:"<<hmcs[0]->currentClockCycle<<" ]---------------------------------------");
  }

  //
  //Update links
  //
  void Network::DownLinkUpdate(bool lastUpdate)
  {
    for(int l=0; l<hmcCntLinks.size(); l++) {
      hmcCntLinks[l]->Update(lastUpdate);
    }
    downLinkClock++;
  }

  void Network::UpLinkUpdate(bool lastUpdate)
  {
    for (int i = 0; i < hmcLinks.size(); i++) {
      for(int l=0; l<NUM_LINKS; l++) {
        hmcLinks[i][l]->Update(lastUpdate);
      }
    }
    upLinkClock++;
  }

  //
  //Fast sync clocks for event driven, not cycle-accurate due to DRAM timing/actions, use it carefully
  //
  void Network::MultiStep(uint64_t cycles)
  {
    // HMC controller and link master update
    for (int i = 0; i < hmcConts.size(); i++) {
      hmcConts[i]->MultiStep(cycles);
      for (int l = 0; l < CPU_LINKS; l++) {
        hmcConts[i]->downLinkMasters[l]->MultiStep(cycles);
      }
    }

    // downstream links update
    downLinkTuner += cycles;
    uint64_t oldDownLinkClock = downLinkClock;
    downLinkClock = (int) (CPU_CLK_PERIOD * (double) downLinkTuner / linkPeriod);
    uint64_t downLinkCycles = downLinkClock - oldDownLinkClock;
    for (int l = 0; l < hmcCntLinks.size(); l++) {
      hmcCntLinks[l]->MultiStep(downLinkCycles);
    }
    // note: next Update() will compensate and sync tuner, no need to do here

    // HMC update
    for (int i = 0; i < hmcs.size(); i++) {
      if (CPU_CLK_PERIOD <= tCK) {
        dramTuner[i] += cycles;
        if (CPU_CLK_PERIOD * dramTuner[i] > tCK * hmcs[i]->clockTuner) {
          uint64_t oldClockTuner = hmcs[i]->clockTuner;
          hmcs[i]->clockTuner = (int) (CPU_CLK_PERIOD * (double) dramTuner[i] / tCK);
          uint64_t hmcCycles = hmcs[i]->clockTuner - oldClockTuner;
          hmcs[i]->MultiStep(hmcCycles);
          for (int l = 0; l < NUM_LINKS; l++) {
            hmcs[i]->downLinkSlaves[l]->MultiStep(hmcCycles);
          }
          hmcs[i]->crossbarSwitch->MultiStep(hmcCycles);
          for (int v = 0; v < NUM_VAULTS; v++) {
            hmcs[i]->vaultControllers[v]->MultiStep(hmcCycles);
            hmcs[i]->vaultControllers[v]->commandQueue->MultiStep(hmcCycles);
          }
          for (int v = 0; v < NUM_VAULTS; v++) {
            hmcs[i]->drams[v]->MultiStep(hmcCycles);
          }
        }
      }
      else {
        dramTuner[i] += cycles;
        uint64_t oldClockTuner = hmcs[i]->clockTuner;
        hmcs[i]->clockTuner = (int) (CPU_CLK_PERIOD * (double) dramTuner[i] / tCK);
        uint64_t hmcCycles = hmcs[i]->clockTuner - oldClockTuner;
        hmcs[i]->MultiStep(hmcCycles);
        for (int l = 0; l < NUM_LINKS; l++) {
          hmcs[i]->downLinkSlaves[l]->MultiStep(hmcCycles);
        }
        hmcs[i]->crossbarSwitch->MultiStep(hmcCycles);
        for (int v = 0; v < NUM_VAULTS; v++) {
          hmcs[i]->vaultControllers[v]->MultiStep(hmcCycles);
          hmcs[i]->vaultControllers[v]->commandQueue->MultiStep(hmcCycles);
        }
        for (int v = 0; v < NUM_VAULTS; v++) {
          hmcs[i]->drams[v]->MultiStep(hmcCycles);
        }
      }
    }

    // upstream links update
    upLinkTuner += cycles;
    uint64_t oldUpLinkClock = upLinkClock;
    upLinkClock = (int) (CPU_CLK_PERIOD * (double) upLinkTuner / linkPeriod);
    uint64_t upLinkCycles = upLinkClock - oldUpLinkClock;
    for (int i = 0; i < hmcLinks.size(); i++) {
      for (int l = 0; l < NUM_LINKS; l++) {
        hmcLinks[i][l]->MultiStep(upLinkCycles);
      }
    }

    // upLinkSlave update
    for (int i =  0; i < hmcConts.size(); i++) {
      for (int l = 0; l < CPU_LINKS; l++) {
        hmcConts[i]->upLinkSlaves[l]->MultiStep(cycles);
      }
    }

    currentClockCycle += cycles;
  }

  //
  //Print simulation debug and state every epoch
  //
  void Network::PrintEpochHeader()
  {
#ifdef DEBUG_LOG
    time_t now;
    struct tm t;
    time(&now);
    t = *localtime(&now);

    stringstream temp_vn;
    if(DEBUG_SIM) {
      string debName = logName + "_debug";
      while(1) { 
        debName += "[";
        temp_vn << logNum;
        debName += temp_vn.str();
        debName += "].log";
        if(access(debName.c_str(), 0)==-1)		break;
        else {
          debName.erase(debName.find("["));
          logNum++;
          temp_vn.str( string() );
          temp_vn.clear();
        }
      }

      debugOut.open(debName.c_str());
      debugOut.setf(ios::left);
      cout<<"  [ "<<debName<<" ] is generated"<<endl;
    }

    if(STATE_SIM) {
      string staName = logName + "_state";
      if(!DEBUG_SIM) {
        stringstream temp_vn_st;
        while(1) { 
          staName += "[";
          temp_vn_st << logNum;
          staName += temp_vn_st.str();
          staName += "].log";
          if(access(staName.c_str(), 0)==-1)		break;
          else {
            staName.erase(staName.find("["));
            logNum++;
            temp_vn_st.str( string() );
            temp_vn_st.clear();
          }
        }
      }
      else {
        staName += "[";
        temp_vn.str( string() );
        temp_vn.clear();
        temp_vn << logNum;
        staName += temp_vn.str();
        staName += "].log";
      }

      stateOut.open(staName.c_str());
      stateOut.setf(ios::left);
      cout<<"  [ "<<staName<<" ] is generated"<<endl;
    }
    cout<<endl;

    //
    //Simulation header generation
    //
    DE_ST("****************************************************************");
    DE_ST("*                      CasHMC version 1.1                      *");
    DEBUG("*                    simDebug log file ["<<logNum<<"]                     *");
    STATE("*                      state log file ["<<logNum<<"]                      *");
    DE_ST("*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
        <<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
        <<"            *");
    DE_ST("*                                                              *");
    DE_ST("*  Module abbreviation info.                                   *");
    DE_ST("*  [HC]   : HMC Controller                                     *");
    DE_ST("*  [LM_U/Di] : ith Up/Downstream Link Master                   *");
    DE_ST("*  [LK_U/Di] : ith Up/Downstream Link                          *");
    DE_ST("*  [LS_U/Di] : ith Up/Downstream Link Slave                    *");
    DE_ST("*  [CS]     : Crossbar Switch                                  *");
    DE_ST("*  [VC_j]   : jth Vault Controller                             *");
    DE_ST("*  [CQ_j-k] : jth k-bank Command Queue                         *");
    DE_ST("*  [DR_j]   : jth DRAM                                         *");
    STATE("*                                                              *");
    STATE("*  # RETRY (packet1 index)[packet1](packet2 index)[packet2]    *");
    DE_ST("****************************************************************");
    DE_ST(endl<<"- Trace type : "<<traceType);
    if(traceType == "random") {
      DE_ST("- Frequency of requests : "<<memUtil);
      DE_ST("- The percentage of reads [%] : "<<rwRatio);
    }
    else if(traceType == "file") {
      DE_ST("- Trace file : "<<traceFileName);
    }
#endif
    DE_ST(endl<<"---------------------------------------[ CPU clk:"<<currentClockCycle<<" / HMC clk:"<<hmcs[0]->currentClockCycle<<" ]---------------------------------------");
  }

  //
  //Print simulation setting info
  //
  void Network::PrintSetting(struct tm t)
  {
    if (benchname.empty()) {
      unsigned int ver_num = 0;
      stringstream temp_vn;
      while(1) {
        logName += "_no";
        temp_vn << ver_num;
        logName += temp_vn.str();
        logName += "_setting.log";

        if(access(logName.c_str(), 0) == -1)	break;
        else {
          logName.erase(logName.find("_no"));
          temp_vn.str( string() );
          temp_vn.clear();
          ver_num++;
        }
      }
    } else {
      logName += "_";
      logName += benchname;
      logName += "_setting.log";
    }
    settingOut.open(logName.c_str());
    cout<<"  [ "<<logName<<" ] is generated"<<endl;

    if (benchname.empty()) {
      logName.erase(logName.find("_s"));
    } else {
      logName.erase(logName.find(benchname));
    }

    settingOut<<"****************************************************************"<<endl;
    settingOut<<"*                      CasHMC version 1.1                      *"<<endl;
    settingOut<<"*                       setting log file                       *"<<endl;
    settingOut<<"*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
      <<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
      <<"            *"<<endl;
    settingOut<<"****************************************************************"<<endl;

    settingOut<<endl<<"        ==== Memory transaction setting ===="<<endl;
    settingOut<<ALI(36)<<" CPU cycles to be simulated : "<<numSimCycles<<endl;
    settingOut<<ALI(36)<<" CPU clock period [ns] : "<<CPU_CLK_PERIOD<<endl;
    settingOut<<ALI(36)<<" Data size of DRAM request [byte] : "<<TRANSACTION_SIZE<<endl;
    settingOut<<ALI(36)<<" Request buffer max size : "<<MAX_REQ_BUF<<endl;
    settingOut<<ALI(36)<<" Trace type : "<<traceType<<endl;
    if(traceType == "random") {
      settingOut<<ALI(36)<<" Frequency of requests : "<<memUtil<<endl;
      settingOut<<ALI(36)<<" The percentage of reads [%] : "<<rwRatio<<endl;
    }
    else if(traceType == "file") {
      settingOut<<ALI(36)<<" Trace file name : "<<traceFileName<<endl;
    }

    settingOut<<endl<<"              ==== Link(SerDes) setting ===="<<endl;
    settingOut<<ALI(36)<<" CRC checking : "<<(CRC_CHECK ? "Enable" : "Disable")<<endl;
    settingOut<<ALI(36)<<" The number of links : "<<NUM_LINKS<<endl;
    settingOut<<ALI(36)<<" Link width : "<<LINK_WIDTH<<endl;
    settingOut<<ALI(36)<<" Link speed [Gb/s] : "<<LINK_SPEED<<endl;
    settingOut<<ALI(36)<<" Link master/slave max buffer size: "<<MAX_LINK_BUF<<endl;
    settingOut<<ALI(36)<<" Crossbar switch buffer size : "<<MAX_CROSS_BUF<<endl;
    settingOut<<ALI(36)<<" Retry buffer max size : "<<MAX_RETRY_BUF<<endl;
    settingOut<<ALI(36)<<" Time to calculate CRC [clk] : "<<CRC_CAL_CYCLE<<endl;
    settingOut<<ALI(36)<<" The number of IRTRY packet : "<<NUM_OF_IRTRY<<endl;
    settingOut<<ALI(36)<<" Retry attempt limit : "<<RETRY_ATTEMPT_LIMIT<<endl;
    settingOut<<ALI(36)<<" Link BER (the power of 10) : "<<LINK_BER<<endl;
    settingOut<<ALI(36)<<" Link priority scheme : "<<LINK_PRIORITY<<endl;

    settingOut<<endl<<"              ==== DRAM general setting ===="<<endl;
    settingOut<<ALI(36)<<" Memory density : "<<MEMORY_DENSITY<<endl;
    settingOut<<ALI(36)<<" The number of vaults : "<<NUM_VAULTS<<endl;
    settingOut<<ALI(36)<<" The number of banks : "<<NUM_BANKS<<endl;
    settingOut<<ALI(36)<<" The number of rows : "<<NUM_ROWS<<endl;
    settingOut<<ALI(36)<<" The number of columns : "<<NUM_COLS<<endl;
    settingOut<<ALI(36)<<" Address mapping (Max block size) : "<<ADDRESS_MAPPING<<endl;
    settingOut<<ALI(36)<<" Vault controller max buffer size : "<<MAX_VLT_BUF<<endl;
    settingOut<<ALI(36)<<" Command queue max size : "<<MAX_CMD_QUE<<endl;
    settingOut<<ALI(36)<<" Command queue structure : "<<(QUE_PER_BANK ? "Bank-level command queue (Bank-Level parallelism)" 														: "Vault-level command queue (Non bank-Level parallelism)")<<endl;
    settingOut<<ALI(36)<<" Memory scheduling : "<<(OPEN_PAGE ? "open page policy" : "close page policy" )<<endl;
    settingOut<<ALI(36)<<" The maximum row buffer accesses : "<<MAX_ROW_ACCESSES<<endl;
    settingOut<<ALI(36)<<" Power-down mode : "<<(USE_LOW_POWER ? "Enable" : "Disable")<<endl;

    settingOut<<endl<<" ==== DRAM timing setting ===="<<endl;
    settingOut<<" Refresh period : "<<REFRESH_PERIOD<<endl;
    settingOut<<" tCK    [ns] : "<<tCK<<endl;
    settingOut<<"  CL   [clk] : "<<CL<<endl;
    settingOut<<"  AL   [clk] : "<<AL<<endl;
    settingOut<<" tRAS  [clk] : "<<tRAS<<endl;
    settingOut<<" tRCD  [clk] : "<<tRCD<<endl;
    settingOut<<" tRRD  [clk] : "<<tRRD<<endl;
    settingOut<<" tRC   [clk] : "<<tRC<<endl;
    settingOut<<" tRP   [clk] : "<<tRP<<endl;
    settingOut<<" tCCD  [clk] : "<<tCCD<<endl;
    settingOut<<" tRTP  [clk] : "<<tRTP<<endl;
    settingOut<<" tWTR  [clk] : "<<tWTR<<endl;
    settingOut<<" tWR   [clk] : "<<tWR<<endl;
    settingOut<<" tRTRS [clk] : "<<tRTRS<<endl;
    settingOut<<" tRFC  [clk] : "<<tRFC<<endl;
    settingOut<<" tFAW  [clk] : "<<tFAW<<endl;
    settingOut<<" tCKE  [clk] : "<<tCKE<<endl;
    settingOut<<" tXP   [clk] : "<<tXP<<endl;
    settingOut<<" tCMD  [clk] : "<<tCMD<<endl;
    settingOut<<"  RL   [clk] : "<<RL<<endl;
    settingOut<<"  WL   [clk] : "<<WL<<endl;
    settingOut<<"  BL   [clk] : "<<BL<<endl;

    settingOut.flush();		settingOut.close();
  }

  //
  //Print plot data file for bandwidth graph
  //
  void Network::MakePlotData()
  {
    double elapsedPlotTime = (double)(PLOT_SAMPLING*CPU_CLK_PERIOD*1E-9);

    //Effective link bandwidth
    vector<double> downLinkEffecPlotBandwidth = vector<double>(NUM_LINKS, 0);
    vector<double> upLinkEffecPlotBandwidth = vector<double>(NUM_LINKS, 0);
    vector<double> linkEffecPlotBandwidth = vector<double>(NUM_LINKS, 0);
    for(int i=0; i<NUM_LINKS; i++) {
      downLinkEffecPlotBandwidth[i] = (downLinkDataSize[i] - downLinkDataSizeTemp[i]) / elapsedPlotTime / (1<<30);
      upLinkEffecPlotBandwidth[i] = (upLinkDataSize[i] - upLinkDataSizeTemp[i]) / elapsedPlotTime / (1<<30);
      linkEffecPlotBandwidth[i] = downLinkEffecPlotBandwidth[i] + upLinkEffecPlotBandwidth[i];
      downLinkDataSizeTemp[i] = downLinkDataSize[i];
      upLinkDataSizeTemp[i] = upLinkDataSize[i];
    }

    //HMC bandwidth
    double hmcPlotBandwidth = (hmcTransmitSize - hmcTransmitSizeTemp)/elapsedPlotTime/(1<<30);
    hmcTransmitSizeTemp = hmcTransmitSize;

    plotDataOut<<setw(8)<<setfill(' ')<<currentClockCycle/PLOT_SAMPLING;
    for(int i=0; i<NUM_LINKS; i++) {
      plotDataOut<<setw(10)<<setfill(' ')<<linkEffecPlotBandwidth[i];
    }
    plotDataOut<<setw(10)<<setfill(' ')<<hmcPlotBandwidth<<std::endl;
  }

  //
  //Print transaction traced statistic on epoch boundaries
  //
  void Network::PrintEpochStatistic()
  {
    uint64_t elapsedCycles;
    if(currentClockCycle%LOG_EPOCH == 0) {
      elapsedCycles = LOG_EPOCH;
    }
    else {
      elapsedCycles = currentClockCycle%LOG_EPOCH;
    }

    //Count transaction and packet type
    uint64_t epochReads = 0;
    uint64_t epochWrites = 0;
    uint64_t epochAtomics = 0;
    uint64_t epochReq = 0;
    uint64_t epochRes = 0;
    uint64_t epochFlow = 0;
    unsigned epochError = 0;

    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      epochReads	+= readPerLink[i];
      epochWrites	+= writePerLink[i];
      epochAtomics+= atomicPerLink[i];
      epochReq	+= reqPerLink[i];
      epochRes	+= resPerLink[i];
      epochFlow	+= flowPerLink[i];
      epochError	+= errorPerLink[i];
    }

    //Ttransaction traced latency statistic calculation
    if(tranFullLat.size() != linkFullLat.size() || linkFullLat.size() != vaultFullLat.size() || tranFullLat.size() != vaultFullLat.size()) {
      ERROR(" == Error - Traces vector size are different (tranFullLat : "<<tranFullLat.size()
          <<", linkFullLat : "<<linkFullLat.size()<<", vaultFullLat : "<<vaultFullLat.size());
      exit(0);
    }
    unsigned tranCount = tranFullLat.size();
    unsigned errorCount = errorRetryLat.size();
    for(int i=0; i<errorCount; i++) {
      errorRetrySum += errorRetryLat[i];
    }

    unsigned tranFullMax = 0;
    unsigned tranFullMin = -1;//max unsigned value
    unsigned linkFullMax = 0;
    unsigned linkFullMin = -1;
    unsigned vaultFullMax = 0;
    unsigned vaultFullMin = -1;
    unsigned errorRetryMax = 0;
    unsigned errorRetryMin = -1;

    double tranStdSum = 0;
    double linkStdSum = 0;
    double vaultStdSum = 0;
    double errorStdSum = 0;

    double tranFullMean = (tranCount==0 ? 0 : (double)tranFullSum/tranCount);
    double linkFullMean = (tranCount==0 ? 0 : (double)linkFullSum/tranCount);
    double vaultFullMean = (tranCount==0 ? 0 : (double)vaultFullSum/tranCount);
    double errorRetryMean = (errorCount==0 ? 0 : (double)errorRetrySum/errorCount);

    for(int i=0; i<tranCount; i++) {		
      tranFullMax = max(tranFullLat[i], tranFullMax);
      tranFullMin = min(tranFullLat[i], tranFullMin);
      tranStdSum += pow(tranFullLat[i]-tranFullMean, 2);

      linkFullMax = max(linkFullLat[i], linkFullMax);
      linkFullMin = min(linkFullLat[i], linkFullMin);
      linkStdSum += pow(linkFullLat[i]-linkFullMean, 2);

      vaultFullMax = max(vaultFullLat[i], vaultFullMax);
      vaultFullMin = min(vaultFullLat[i], vaultFullMin);
      vaultStdSum += pow(vaultFullLat[i]-vaultFullMean, 2);
    }
    for(int i=0; i<errorCount; i++) {
      errorRetryMax = max(errorRetryLat[i], errorRetryMax);
      errorRetryMin = min(errorRetryLat[i], errorRetryMin);
      errorStdSum += pow(errorRetryLat[i]-errorRetryMean, 2);
    }
    double tranStdDev = sqrt(tranCount==0 ? 0 : tranStdSum/tranCount);
    double linkStdDev = sqrt(tranCount==0 ? 0 : linkStdSum/tranCount);
    double vaultStdDev = sqrt(tranCount==0 ? 0 : vaultStdSum/tranCount);
    double errorRetryDev = sqrt(errorCount==0 ? 0 : errorStdSum/errorCount);
    tranFullLat.clear();
    linkFullLat.clear();
    vaultFullLat.clear();
    errorRetryLat.clear();

    //Bandwidth calculation
    double elapsedTime = (double)(elapsedCycles*CPU_CLK_PERIOD*1E-9);
    double hmcBandwidth = hmcTransmitSize/elapsedTime/(1<<30);
    double linkBandwidthMax = LINK_SPEED * LINK_WIDTH / 8;
    vector<double> hmcCtrlLinkBandwidth = vector<double>(NUM_LINKS, 0);
    vector<double> downLinkBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> upLinkBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> linkBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    double linkBandwidthSum = 0;
    vector<double> downLinkEffecBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> upLinkEffecBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> linkEffecBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    double linkEffecBandwidthSum = 0;
    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      downLinkBandwidth[i] = downLinkTransmitSize[i] / elapsedTime / (1<<30);
      upLinkBandwidth[i] = upLinkTransmitSize[i] / elapsedTime / (1<<30);
      linkBandwidth[i] = downLinkBandwidth[i] + upLinkBandwidth[i];
      linkBandwidthSum += linkBandwidth[i];
      //Effective bandwidth is only data bandwith regardless of packet header and tail bits
      downLinkEffecBandwidth[i] = downLinkDataSize[i] / elapsedTime / (1<<30);
      upLinkEffecBandwidth[i] = upLinkDataSize[i] / elapsedTime / (1<<30);
      linkEffecBandwidth[i] = downLinkEffecBandwidth[i] + upLinkEffecBandwidth[i];
      linkEffecBandwidthSum += linkEffecBandwidth[i];
    }

    //Print epoch statistic result
    STATE(endl<<endl<<"  ============= ["<<currentClockCycle/LOG_EPOCH<<"] Epoch statistic result ============="<<endl);
    STATE("  Current epoch : "<<currentClockCycle/LOG_EPOCH);
    STATE("  Current clock : "<<currentClockCycle<<"  (epoch clock : "<<elapsedCycles<<")"<<endl);

    STATE("        HMC bandwidth : "<<ALI(7)<<hmcBandwidth<<" GB/s  (Considered only data size)");
    STATE("       Link bandwidth : "<<ALI(7)<<linkBandwidthSum<<" GB/s  (Included flow packet)");
    STATE(" Effec Link bandwidth : "<<ALI(7)<<linkEffecBandwidthSum<<" GB/s  (Data bandwidth regardless of packet header and tail)");
    STATE("     Link utilization : "<<ALI(7)<<linkBandwidthSum/(linkBandwidthMax*NUM_LINKS*2)*100
        <<" %     (Link max bandwidth : "<<linkBandwidthMax*NUM_LINKS*2<<" GB/S)"<<endl);

    STATE("    Tran latency mean : "<<tranFullMean*CPU_CLK_PERIOD<<" ns");
    STATE("                 std  : "<<tranStdDev*CPU_CLK_PERIOD<<" ns");
    STATE("                 max  : "<<tranFullMax*CPU_CLK_PERIOD<<" ns");
    STATE("                 min  : "<<(tranFullMin!=-1 ? tranFullMin : 0)*CPU_CLK_PERIOD<<" ns");
    STATE("    Link latency mean : "<<linkFullMean*CPU_CLK_PERIOD<<" ns");
    STATE("                 std  : "<<linkStdDev*CPU_CLK_PERIOD<<" ns");
    STATE("                 max  : "<<linkFullMax*CPU_CLK_PERIOD<<" ns");
    STATE("                 min  : "<<(linkFullMin!=-1 ? linkFullMin : 0)*CPU_CLK_PERIOD<<" ns");
    STATE("   Vault latency mean : "<<vaultFullMean*tCK<<" ns");
    STATE("                 std  : "<<vaultStdDev*tCK<<" ns");
    STATE("                 max  : "<<vaultFullMax*tCK<<" ns");
    STATE("                 min  : "<<(vaultFullMin!=-1 ? vaultFullMin : 0)*tCK<<" ns");
    STATE("   Retry latency mean : "<<errorRetryMean*CPU_CLK_PERIOD<<" ns");
    STATE("                 std  : "<<errorRetryDev*CPU_CLK_PERIOD<<" ns");
    STATE("                 max  : "<<errorRetryMax*CPU_CLK_PERIOD<<" ns");
    STATE("                 min  : "<<(errorRetryMin!=-1 ? errorRetryMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl);

    STATE("           Read count : "<<epochReads);
    STATE("          Write count : "<<epochWrites);
    STATE("         Atomic count : "<<epochAtomics);
    STATE("        Request count : "<<epochReq);
    STATE("       Response count : "<<epochRes);
    STATE("           Flow count : "<<epochFlow);
    STATE("    Transaction count : "<<tranCount);
    STATE("    Error abort count : "<<epochError);
    STATE("    Error retry count : "<<errorCount<<endl);

    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      STATE("  ┌─────────────  [Link "<<i<<"]");
      STATE("  │              Read per link : "<<readPerLink[i]);
      STATE("  │             Write per link : "<<writePerLink[i]);
      STATE("  │            Atomic per link : "<<atomicPerLink[i]);
      STATE("  │           Request per link : "<<reqPerLink[i]);
      STATE("  │          Response per link : "<<resPerLink[i]);
      STATE("  │              Flow per link : "<<flowPerLink[i]);
      STATE("  │       Error abort per link : "<<errorPerLink[i]);
      STATE("  │       Downstream Bandwidth : "<<ALI(7)<<downLinkBandwidth[i]<<" GB/s  (Utilization : "<<downLinkBandwidth[i]/linkBandwidthMax*100<<" %)");
      STATE("  │         Upstream Bandwidth : "<<ALI(7)<<upLinkBandwidth[i]<<" GB/s  (Utilization : "<<upLinkBandwidth[i]/linkBandwidthMax*100<<" %)");
      STATE("  │            Total Bandwidth : "<<ALI(7)<<linkBandwidth[i]<<" GB/s  (Utilization : "<<linkBandwidth[i]/(linkBandwidthMax*2)*100<<" %)");
      STATE("  │ Downstream effec Bandwidth : "<<ALI(7)<<downLinkEffecBandwidth[i]<<" GB/s");
      STATE("  │   Upstream effec Bandwidth : "<<ALI(7)<<upLinkEffecBandwidth[i]<<" GB/s");
      STATE("  └───── Total effec Bandwidth : "<<ALI(7)<<linkEffecBandwidth[i]<<" GB/s"<<endl);
    }

    vector<uint64_t> epoch_stalls(nodes, 0); 
    uint64_t tot_epoch_stalls = 0;
    STATEN("  Operand buffer stalls : ");
    for (int i = 0; i < nodes; i++) {
      epoch_stalls[i] = hmcs[i]->crossbarSwitch->opbufStalls - opbufStalls[i];
      tot_epoch_stalls += epoch_stalls[i];
      opbufStalls[i] = hmcs[i]->crossbarSwitch->opbufStalls;
      STATEN("[" << i << "] " << epoch_stalls[i] << "  ");
    }
    STATE("");
    STATE("  Total operand buffer stalls " << tot_epoch_stalls << endl);
    totOpbufStalls += tot_epoch_stalls;

    vector<uint64_t> epoch_updates(nodes, 0);
    STATEN("  Number of updates : ");
    for (int i = 0; i < nodes; i++) {
      epoch_updates[i] = hmcs[i]->crossbarSwitch->numUpdates - numUpdates[i];
      numUpdates[i] = hmcs[i]->crossbarSwitch->numUpdates;
      STATEN("[" << i << "] " << epoch_updates[i] << "  ");
    }
    STATE("");

    vector<uint64_t> epoch_operands(nodes, 0);
    STATEN("  Number of operands : ");
    for (int i = 0; i < nodes; i++) {
      epoch_operands[i] = hmcs[i]->crossbarSwitch->numOperands - numOperands[i];
      numOperands[i] = hmcs[i]->crossbarSwitch->numOperands;
      STATEN("[" << i << "] " << epoch_operands[i] << "  ");
    }
    STATE("");

    //One epoch simulation statistic results are accumulated
    totalTranCount += tranCount;
    totalErrorCount += errorCount;

    totalHmcTransmitSize += hmcTransmitSize;
    hmcTransmitSize = 0;
    //for(int i=0; i<NUM_LINKS; i++) {
    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      totalDownLinkTransmitSize[i] += downLinkTransmitSize[i];
      totalUpLinkTransmitSize[i] += upLinkTransmitSize[i];
      downLinkTransmitSize[i] = 0;
      upLinkTransmitSize[i] = 0;
      totalDownLinkDataSize[i] += downLinkDataSize[i];
      totalUpLinkDataSize[i] += upLinkDataSize[i];
      downLinkDataSize[i] = 0;
      upLinkDataSize[i] = 0;
      totalLinkPasTransmitSize[i] += linkPasTransmitSize[i];
      totalLinkFlowTransmitSize[i] += linkFlowTransmitSize[i];
      totalLinkActTransmitSize[i] += linkActTransmitSize[i];
      linkActTransmitSize[i] = 0;
      linkPasTransmitSize[i] = 0;
      linkFlowTransmitSize[i] = 0;
    }

    totalTranFullSum += tranFullSum;
    totalLinkFullSum += linkFullSum;
    totalVaultFullSum += vaultFullSum;
    totalErrorRetrySum += errorRetrySum;
    tranFullSum = 0;
    linkFullSum = 0;
    vaultFullSum = 0;
    errorRetrySum = 0;

    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      totalReadPerLink[i] += readPerLink[i];		readPerLink[i] = 0;
      totalWritePerLink[i] += writePerLink[i];	writePerLink[i] = 0;
      totalAtomicPerLink[i] += atomicPerLink[i];	atomicPerLink[i] = 0;
      totalReqPerLink[i] += reqPerLink[i];		reqPerLink[i] = 0;
      totalResPerLink[i] += resPerLink[i];		resPerLink[i] = 0;
      totalFlowPerLink[i] += flowPerLink[i];		flowPerLink[i] = 0;
      totalErrorPerLink[i] += errorPerLink[i];	errorPerLink[i] = 0;
    }

    totalTranFullMax = max(tranFullMax, totalTranFullMax);
    totalTranFullMin = min(tranFullMin, totalTranFullMin);
    totalLinkFullMax = max(linkFullMax, totalLinkFullMax);
    totalLinkFullMin = min(linkFullMin, totalLinkFullMin);
    totalVaultFullMax = max(vaultFullMax, totalVaultFullMax);
    totalVaultFullMin = min(vaultFullMin, totalVaultFullMin);
    totalErrorRetryMax = max(errorRetryMax, totalErrorRetryMax);
    totalErrorRetryMin = min(errorRetryMin, totalErrorRetryMin);

    totalTranStdSum += tranStdSum;
    totalLinkStdSum += linkStdSum;
    totalVaultStdSum += vaultStdSum;
    totalErrorStdSum += errorStdSum;

    hmcTransmitSizeTemp = 0;
    for(int i=0; i<NUM_LINKS; i++) {
      downLinkDataSizeTemp[i] = 0;
      upLinkDataSizeTemp[i] = 0;
    }
  }

  //
  //Print final transaction traced statistic accumulated on every epoch
  //
  void Network::PrintFinalStatistic()
  {
    time_t now;
    struct tm t;
    time(&now);
    t = *localtime(&now);

    PrintSetting(t);

    string resName;
    if (benchname.empty()) {
      resName = logName + "_result.log";
    } else {
      resName = logName + benchname + "_result.log";
    }

    resultOut.open(resName.c_str());
    cout<<"\n   === Simulation finished  ( CPU clk:"<<currentClockCycle<<" ) ===   "<<endl;
    cout<<"  [ "<<resName<<" ] is generated"<<endl<<endl;
    //cout << "sim compute time: " << computeFinishCycle << endl;

    double elapsedTime = (double)(currentClockCycle*CPU_CLK_PERIOD*1E-9);
    double hmcBandwidth = totalHmcTransmitSize/elapsedTime/(1<<30);
    double linkBandwidthMax = LINK_SPEED * LINK_WIDTH / 8;
    vector<double> downLinkActBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> downLinkPasBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> downLinkFlowBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> downLinkBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> upLinkBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> linkBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    double linkHCBandwidthSum = 0;
    double linkBandwidthSum = 0;
    vector<double> downLinkEffecBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> upLinkEffecBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    vector<double> linkEffecBandwidth = vector<double>(TOTAL_NUM_LINKS, 0);
    double linkHCEffecBandwidthSum = 0;
    double linkEffecBandwidthSum = 0;
    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      downLinkActBandwidth[i] = totalLinkActTransmitSize[i] / elapsedTime / (1<<30);
      downLinkPasBandwidth[i] = totalLinkPasTransmitSize[i] / elapsedTime / (1<<30);
      downLinkFlowBandwidth[i] = totalLinkFlowTransmitSize[i] / elapsedTime / (1<<30);
      downLinkBandwidth[i] = totalDownLinkTransmitSize[i] / elapsedTime / (1<<30);
      upLinkBandwidth[i] = totalUpLinkTransmitSize[i] / elapsedTime / (1<<30);
      linkBandwidth[i] = downLinkBandwidth[i] + upLinkBandwidth[i];
      linkBandwidthSum += linkBandwidth[i];
      if(i >= 64) linkHCBandwidthSum += linkBandwidth[i];
      //Effective bandwidth is only data bandwith regardless of packet header and tail bits
      downLinkEffecBandwidth[i] = totalDownLinkDataSize[i] / elapsedTime / (1<<30);
      upLinkEffecBandwidth[i] = totalUpLinkDataSize[i] / elapsedTime / (1<<30);
      linkEffecBandwidth[i] = downLinkEffecBandwidth[i] + upLinkEffecBandwidth[i];
      linkEffecBandwidthSum += linkEffecBandwidth[i];
      if(i >= 64) linkHCEffecBandwidthSum += linkEffecBandwidth[i];
    }

    double tranFullMean = (totalTranCount==0 ? 0 : (double)totalTranFullSum/totalTranCount);
    double linkFullMean = (totalTranCount==0 ? 0 : (double)totalLinkFullSum/totalTranCount);
    double vaultFullMean = (totalTranCount==0 ? 0 : (double)totalVaultFullSum/totalTranCount);
    double errorRetryMean = (totalErrorCount==0 ? 0 : (double)totalErrorRetrySum/totalErrorCount);
    double tranStdDev = sqrt(totalTranCount==0 ? 0 : totalTranStdSum/totalTranCount);
    double linkStdDev = sqrt(totalTranCount==0 ? 0 : totalLinkStdSum/totalTranCount);
    double vaultStdDev = sqrt(totalTranCount==0 ? 0 : totalVaultStdSum/totalTranCount);
    double errorRetryDev = sqrt(totalErrorCount==0 ? 0 : totalErrorStdSum/totalErrorCount);

    uint64_t epochReads = 0;
    uint64_t epochWrites = 0;
    uint64_t epochAtomics = 0;
    uint64_t epochReq = 0;
    uint64_t epochRes = 0;
    uint64_t epochFlow = 0;
    unsigned epochError = 0;
    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      epochReads	+= totalReadPerLink[i];
      epochWrites	+= totalWritePerLink[i];
      epochAtomics+= totalAtomicPerLink[i];
      epochReq	+= totalReqPerLink[i];
      epochRes	+= totalResPerLink[i];
      epochFlow	+= totalFlowPerLink[i];
      epochError	+= totalErrorPerLink[i];
    }


    //Print statistic result
    resultOut<<"****************************************************************"<<endl;
    resultOut<<"*                      CasHMC version 1.1                      *"<<endl;
    resultOut<<"*                       result log file                        *"<<endl;
    resultOut<<"*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
      <<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
      <<"            *"<<endl;
    resultOut<<"****************************************************************"<<endl<<endl;

    resultOut<<"- Trace type : "<<traceType<<endl;
    if(traceType == "random") {
      resultOut<<"- Frequency of requests : "<<memUtil<<endl;
      resultOut<<"- The percentage of reads [%] : "<<rwRatio<<endl;
    }
    else if(traceType == "file") {
      resultOut<<"- Trace file : "<<traceFileName<<endl<<endl;
    }

    resultOut<<"  ============= CasHMC statistic result ============="<<endl<<endl;
    resultOut<<"  Elapsed epoch : "<<currentClockCycle/LOG_EPOCH<<endl;
    resultOut<<"  Elapsed clock : "<<currentClockCycle<<endl<<endl;

    resultOut<<"        HMC bandwidth : "<<ALI(7)<<hmcBandwidth<<" GB/s  (Considered only data size)"<<endl;
    resultOut<<"    Link HC bandwidth : "<<ALI(7)<<linkHCBandwidthSum<<" GB/s  (Bandwidth flowing into HMC Network)"<<endl;
    resultOut<<"       Link bandwidth : "<<ALI(7)<<linkBandwidthSum<<" GB/s  (Sum of bandwidth over " << ncpus + nodes*4<<" Links, Included flow packet)"<<endl;
    resultOut<<" Effec Link bandwidth : "<<ALI(7)<<linkEffecBandwidthSum<<" GB/s  (Data bandwidth(sum) regardless of packet header and tail over "<< ncpus + nodes*4 <<" Links)"<<endl;
    resultOut<<" Ef Link HC bandwidth : "<<ALI(7)<<linkHCEffecBandwidthSum<<" GB/s  (Data bandwidth(sum) regardless of packet header and tail over CPU links injecting into HMCNET)"<<endl;
    resultOut<<"     Link utilization : "<<ALI(7)<<linkBandwidthSum/(linkBandwidthMax*TOTAL_NUM_LINKS)*100
      <<" %     (Max link bandwidth : "<<linkBandwidthMax*NUM_LINKS*2<<" GB/S)"<<endl<<endl;

    resultOut<<"    Tran latency mean : "<<tranFullMean*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 std  : "<<tranStdDev*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 max  : "<<totalTranFullMax*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 min  : "<<(totalTranFullMin!=-1 ? totalTranFullMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"    Link latency mean : "<<linkFullMean*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 std  : "<<linkStdDev*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 max  : "<<totalLinkFullMax*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 min  : "<<(totalLinkFullMin!=-1 ? totalLinkFullMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"   Vault latency mean : "<<vaultFullMean*tCK<<" ns"<<endl;
    resultOut<<"                 std  : "<<vaultStdDev*tCK<<" ns"<<endl;
    resultOut<<"                 max  : "<<totalVaultFullMax*tCK<<" ns"<<endl;
    resultOut<<"                 min  : "<<(totalVaultFullMin!=-1 ? totalVaultFullMin : 0)*tCK<<" ns"<<endl;
    resultOut<<"   Retry latency mean : "<<errorRetryMean*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 std  : "<<errorRetryDev*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 max  : "<<totalErrorRetryMax*CPU_CLK_PERIOD<<" ns"<<endl;
    resultOut<<"                 min  : "<<(totalErrorRetryMin!=-1 ? totalErrorRetryMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl<<endl;

    resultOut<<"           Read count : "<<epochReads<<endl;
    resultOut<<"          Write count : "<<epochWrites<<endl;
    resultOut<<"         Atomic count : "<<epochAtomics<<endl;
    resultOut<<"        Request count : "<<epochReq<<endl;
    resultOut<<"       Response count : "<<epochRes<<endl;
    resultOut<<"           Flow count : "<<epochFlow<<endl;
    resultOut<<"    Transaction count : "<<totalTranCount<<endl;
    resultOut<<"    Error abort count : "<<epochError<<endl;
    resultOut<<"    Error retry count : "<<totalErrorCount<<endl<<endl;

    for(int i=0; i<TOTAL_NUM_LINKS; i++) {
      resultOut<<"  ┌─────────────  [Link "<<i<<"]"<<allLinks[i]->header<<endl;
      resultOut<<"  │              Read per link : "<<totalReadPerLink[i]<<endl;
      resultOut<<"  │             Write per link : "<<totalWritePerLink[i]<<endl;
      resultOut<<"  │            Atomic per link : "<<totalAtomicPerLink[i]<<endl;
      resultOut<<"  │           Request per link : "<<totalReqPerLink[i]<<endl;
      resultOut<<"  │          Response per link : "<<totalResPerLink[i]<<endl;
      resultOut<<"  │              Flow per link : "<<totalFlowPerLink[i]<<endl;
      resultOut<<"  │       Error abort per link : "<<totalErrorPerLink[i]<<endl;
      resultOut<<"  │       Active Pkt Bandwidth : "<<ALI(7)<<downLinkActBandwidth[i]<<" GB/s  (Utilization : "<<downLinkActBandwidth[i]/linkBandwidthMax*100<<" %)"<<endl;
      resultOut<<"  │      Passive Pkt Bandwidth : "<<ALI(7)<<downLinkPasBandwidth[i]<<" GB/s  (Utilization : "<<downLinkPasBandwidth[i]/linkBandwidthMax*100<<" %)"<<endl;
      resultOut<<"  │         Flow Pkt Bandwidth : "<<ALI(7)<<downLinkFlowBandwidth[i]<<" GB/s  (Utilization : "<<downLinkFlowBandwidth[i]/linkBandwidthMax*100<<" %)"<<endl;
      resultOut<<"  │       Downstream Bandwidth : "<<ALI(7)<<downLinkBandwidth[i]<<" GB/s  (Utilization : "<<downLinkBandwidth[i]/linkBandwidthMax*100<<" %)"<<endl;
      resultOut<<"  │         Upstream Bandwidth : "<<ALI(7)<<upLinkBandwidth[i]<<" GB/s  (Utilization : "<<upLinkBandwidth[i]/linkBandwidthMax*100<<" %)"<<endl;
      resultOut<<"  │            Total Bandwidth : "<<ALI(7)<<linkBandwidth[i]<<" GB/s  (Utilization : "<<linkBandwidth[i]/(linkBandwidthMax*2)*100<<" %)"<<endl;
      //resultOut<<"  │ Downstream effec Bandwidth : "<<ALI(7)<<downLinkEffecBandwidth[i]<<" GB/s"<<endl;
      //resultOut<<"  │   Upstream effec Bandwidth : "<<ALI(7)<<upLinkEffecBandwidth[i]<<" GB/s"<<endl;
      resultOut<<"  └───── Total effec Bandwidth : "<<ALI(7)<<linkEffecBandwidth[i]<<" GB/s"<<endl<<endl;
    }

    resultOut << "  Operand buffer stalls : ";
    totOpbufStalls = 0;
    for (int i = 0; i < nodes; i++) {
      opbufStalls[i] = hmcs[i]->crossbarSwitch->opbufStalls;
      totOpbufStalls += hmcs[i]->crossbarSwitch->opbufStalls;
      resultOut << "[" << i << "] " << opbufStalls[i] << "  ";
    }
    resultOut << endl << "  Total operand buffer stalls " << totOpbufStalls << endl << endl;

    resultOut << "  Number of updates : ";
    for (int i = 0; i < nodes; i++) {
      numUpdates[i] = hmcs[i]->crossbarSwitch->numUpdates;
      resultOut << "[" << i << "] " << numUpdates[i] << "  ";
    }
    resultOut << endl << endl;

    resultOut << "  Number of operands : ";
    for (int i = 0; i < nodes; i++) {
      numOperands[i] = hmcs[i]->crossbarSwitch->numOperands;
      resultOut << "[" << i << "] " << numOperands[i] << "  ";
    }
    resultOut << endl << endl;
  }

  //
  //Print CrossbarSwitch buffers
  //
  void Network::PrintXbarBuffers()
  {
    cout << "========== CrossbarSwitch Buffers Begin ==========" << endl;
    for (int i = 0; i < hmcs.size(); i++) {
      hmcs[i]->crossbarSwitch->PrintBuffers();
    }
    cout << "========== CrossbarSwitch Buffers End ==========" << endl;
  }

} // namespace CasHMC
