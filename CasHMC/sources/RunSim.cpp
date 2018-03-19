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

#include <getopt.h>		//getopt_long
#include <stdlib.h>		//exit(0)
#include <fstream>		//ofstream
#include <vector>		//vector

//#include "CasHMCWrapper.h"
#include "Network.h"
#include "Transaction.h"

using namespace std;
using namespace CasHMC;

long numSimCycles = 100000;
uint64_t lineNumber = 1;
double memUtil = 0.1;
double rwRatio = 80;
string traceType = "";
string traceFileName = "";
vector<Transaction *> transactionBuffers;
//CasHMCWrapper *casHMCWrapper;
Network *casHMCWrapper;

void help()
{
  cout<<endl<<"-c (--cycle)   : The number of CPU cycles to be simulated"<<endl;
  cout<<"-t (--trace)   : Trace type ('random' or 'file', or 'active')"<<endl;
  cout<<"-u (--util)    : Requests frequency (0 = no requests, 1 = as fast as possible) [Default 0.1]"<<endl;
  cout<<"-r (--rwratio) : (%) The percentage of reads in request stream [Default 80]"<<endl;
  cout<<"-f (--file)    : Trace file name"<<endl;
  cout<<"-h (--help)    : Simulation option help"<<endl<<endl;
}

void MakeRandomTransaction(void)
{
  int rand_tran = rand()%10000+1;
  if(rand_tran <= (int)(memUtil*10000)) {
    Transaction *newTran;
    uint64_t physicalAddress = rand();
    physicalAddress = (physicalAddress<<32)|rand();

    if(physicalAddress%101 <= (int)rwRatio) {
      newTran = new Transaction(DATA_READ, physicalAddress, TRANSACTION_SIZE, casHMCWrapper, 0, 0);		// Read transaction
    }
    else {
      newTran = new Transaction(DATA_WRITE, physicalAddress, TRANSACTION_SIZE, casHMCWrapper, 0, 0);	// Write transaction
    }
    transactionBuffers.push_back(newTran);
  }
}

// for Active Network Computing, Jiayi, 01/30/17
void MakeActiveTransaction(void)
{
  int active_tran = rand() % 10000 + 1;
  if (active_tran <= (int)(memUtil*10000)) {
    Transaction *newTran;
    uint64_t physicalAddress = rand();
    physicalAddress = (physicalAddress << 32) | rand();

    newTran = new Transaction(ACTIVE_ADD, 3, physicalAddress, TRANSACTION_SIZE, casHMCWrapper, 0, 0);
    //newTran = new Transaction(ACTIVE_ADD, physicalAddress, TRANSACTION_SIZE, casHMCWrapper);

    transactionBuffers.push_back(newTran);
  }
}

void MakeActiveTransaction(TransactionType tran_type, uint64_t dest_addr, uint64_t src_addr, int src_cube,
    int dest_cube)
{
  Transaction *newTran;

  newTran = new Transaction(tran_type, dest_addr, src_addr, TRANSACTION_SIZE, casHMCWrapper, src_cube, dest_cube);

  transactionBuffers.push_back(newTran);
}

void MakeActiveTransaction(TransactionType tran_type, uint64_t dest_addr, uint64_t src_addr1, uint64_t src_addr2,
    int src_cube, int dest_cube1, int dest_cube2)
{
  Transaction *newTran;
  newTran = new Transaction(tran_type, dest_addr, src_addr1, src_addr2, TRANSACTION_SIZE, casHMCWrapper, src_cube,
      dest_cube1, dest_cube2);
  transactionBuffers.push_back(newTran);
}

void parseTraceFileLine(string &line, uint64_t &clockCycle, uint64_t &addr, TransactionType &tranType, unsigned &dataSize)
{
  int previousIndex=0;
  int spaceIndex=0;
  string  tempStr="";

  spaceIndex = line.find_first_of(" ", 0);
  if(spaceIndex==-1)	ERROR("  == Error - tracefile format is wrong : "<<line);
  tempStr = line.substr(0, spaceIndex);
  istringstream clock(tempStr);
  clock>>clockCycle;
  previousIndex = spaceIndex;

  spaceIndex = line.find_first_not_of(" ", previousIndex);
  if(spaceIndex==-1)	ERROR("  == Error - tracefile format is wrong : "<<line);
  tempStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);
  istringstream add(tempStr.substr(2));
  add>>hex>>addr;
  previousIndex = line.find_first_of(" ", spaceIndex);

  spaceIndex = line.find_first_not_of(" ", previousIndex);
  if(spaceIndex==-1)	ERROR("  == Error - tracefile format is wrong : "<<line);

  tempStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);
  if(tempStr.compare("READ")==0)			tranType = DATA_READ;
  else if(tempStr.compare("WRITE")==0)	tranType = DATA_WRITE;
  //Arithmetic atomic
  else if(tempStr.compare("2ADD8")==0)	tranType = ATM_2ADD8;
  else if(tempStr.compare("ADD16")==0)	tranType = ATM_ADD16;
  else if(tempStr.compare("P_2ADD8")==0)	tranType = ATM_P_2ADD8;
  else if(tempStr.compare("P_ADD16")==0)	tranType = ATM_P_ADD16;
  else if(tempStr.compare("2ADDS8R")==0)	tranType = ATM_2ADDS8R;
  else if(tempStr.compare("ADDS16R")==0)	tranType = ATM_ADDS16R;
  else if(tempStr.compare("INC8")==0)		tranType = ATM_INC8;
  else if(tempStr.compare("P_INC8")==0)	tranType = ATM_P_INC8;
  //Boolean atomic
  else if(tempStr.compare("XOR16")==0)	tranType = ATM_XOR16;
  else if(tempStr.compare("OR16")==0)		tranType = ATM_OR16;
  else if(tempStr.compare("NOR16")==0)	tranType = ATM_NOR16;
  else if(tempStr.compare("AND16")==0)	tranType = ATM_AND16;
  else if(tempStr.compare("NAND16")==0)	tranType = ATM_NAND16;
  //Comparison atomic
  else if(tempStr.compare("CASGT8")==0)	tranType = ATM_CASGT8;
  else if(tempStr.compare("CASLT8")==0)	tranType = ATM_CASLT8;
  else if(tempStr.compare("CASGT16")==0)	tranType = ATM_CASGT16;
  else if(tempStr.compare("CASLT16")==0)	tranType = ATM_CASLT16;
  else if(tempStr.compare("CASEQ8")==0)	tranType = ATM_CASEQ8;
  else if(tempStr.compare("CASZERO16")==0)tranType = ATM_CASZERO16;
  else if(tempStr.compare("EQ16")==0)		tranType = ATM_EQ16;
  else if(tempStr.compare("EQ8")==0)		tranType = ATM_EQ8;
  //Bitwise atomic
  else if(tempStr.compare("BWR")==0)		tranType = ATM_BWR;
  else if(tempStr.compare("P_BWR")==0)	tranType = ATM_P_BWR;
  else if(tempStr.compare("BWR8R")==0)	tranType = ATM_BWR8R;
  else if(tempStr.compare("SWAP16")==0)	tranType = ATM_SWAP16;
  else {
    ERROR("  == Error - Unknown command in tracefile : "<<tempStr);
  }
  previousIndex = line.find_first_of(" ", spaceIndex);

  spaceIndex = line.find_first_not_of(" ", previousIndex);
  if(spaceIndex==-1) {
    dataSize = TRANSACTION_SIZE;
    return;
  }
  tempStr = line.substr(spaceIndex, line.find_first_of(" ", spaceIndex) - spaceIndex);
  istringstream size(tempStr);
  size>>dataSize;
}
/*
int main(int argc, char **argv)
{
  //
  //parse command-line options setting
  //
  int opt;
  string pwdString = "";
  ifstream traceFile;
  string line;
  bool pendingTran = false;
  while(1) {
    static struct option long_options[] = {
      {"pwd", required_argument, 0, 'p'},
      {"cycle",  required_argument, 0, 'c'},
      {"trace",  required_argument, 0, 't'},
      {"util",  required_argument, 0, 'u'},
      {"rwratio",  required_argument, 0, 'r'},
      {"file",  required_argument, 0, 'f'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}
    };
    int option_index=0;
    opt = getopt_long (argc, argv, "p:c:t:u:r:f:h", long_options, &option_index);
    if(opt == -1) {
      break;
    }
    switch(opt) {
      case 0:
        if (long_options[option_index].flag != 0) {
          cout<<"setting flag"<<endl;
          break;
        }
        cout<<"option "<<long_options[option_index].name<<endl;
        if (optarg) {
          cout<<" with arg"<<optarg<<endl;
        }
        cout<<""<<endl;
        break;
      case 'p':
        pwdString = string(optarg);
        break;
      case 'c':
        numSimCycles = atol(optarg);
        break;
      case 't':
        traceType = string(optarg);
        if(traceType != "random" && traceType != "file" && traceType != "active" && traceType != "active2") { // Jiayi, active, 01/27
          cout<<endl<<" == -t (--trace) ERROR ==";
          cout<<endl<<"  This option must be selected by one of 'random' and 'file'"<<endl<<endl;
          exit(0);
        }
        break;
      case 'u':
        memUtil = atof(optarg);
        if(memUtil < 0 || memUtil > 1) {
          cout<<endl<<" == -u (--util) ERROR ==";
          cout<<endl<<"  This value must be in the between '0' and '1'"<<endl<<endl;
          exit(0);
        }
        break;
      case 'r':
        rwRatio = atof(optarg);
        if(rwRatio < 0 || rwRatio > 100) {
          cout<<endl<<" == -r (--rwratio) ERROR ==";
          cout<<endl<<"  This value is the percentage of reads in request stream"<<endl<<endl;
          exit(0);
        }
        break;
      case 'f':
        traceFileName = string(optarg);
        if(access(traceFileName.c_str(), 0) == -1) {
          cout<<endl<<" == -f (--file) ERROR ==";
          cout<<endl<<"  There is no trace file ["<<traceFileName<<"]"<<endl<<endl;
          exit(0);
        }
        break;
      case 'h':
      case '?':
        help();
        exit(0);
        break;
    }
  }

  srand((unsigned)time(NULL));
  //casHMCWrapper = new CasHMCWrapper();
  casHMCWrapper = NULL;
  if (SIM_TOPOLOGY == DEFAULT) {
    casHMCWrapper = Network::New(1, DEFAULT);
  } else {
    casHMCWrapper = Network::New(4, DFLY);
  }
  if (casHMCWrapper == NULL) {
    cout << "casHMCWrapper is NULL!!!!!!!!" << endl;
    assert(0);
  }

  if(traceType == "random") {
    for(uint64_t cpuCycle=0; cpuCycle<numSimCycles; cpuCycle++) {
      MakeRandomTransaction();
      if(!transactionBuffers.empty()) {
        if(casHMCWrapper->ReceiveTran(transactionBuffers[0], 0)) {
          transactionBuffers.erase(transactionBuffers.begin());
        }
      }
      casHMCWrapper->Update();
    }
  }
  else if(traceType == "file") {
    uint64_t issueClock = 0;
    traceFile.open(traceFileName.c_str());
    if(!traceFile.is_open()) {
      ERROR(" == Error - Could not open trace file ["<<traceFileName<<"]");
      exit(0);
    }
    for(uint64_t cpuCycle=0; cpuCycle<numSimCycles; cpuCycle++) {
      if(!pendingTran) {
        if(!traceFile.eof()) {
          getline(traceFile, line);
          if(line.size() > 0) {
            uint64_t addr;
            TransactionType tranType;
            unsigned dataSize;
            parseTraceFileLine(line, issueClock, addr, tranType, dataSize);
            Transaction *newTran = new Transaction(tranType, addr, dataSize, casHMCWrapper, 0, 0);

            if(cpuCycle >= issueClock) {
              if(!casHMCWrapper->ReceiveTran(newTran, 0)) {
                pendingTran = true;
                transactionBuffers.push_back(newTran);
              }
            }
            else {
              pendingTran = true;
              transactionBuffers.push_back(newTran);
            }
          }
          else {
            cout<<" ## WARNING ## Skipping line ("<<lineNumber<<") in tracefile  (CurrentClock : "<<cpuCycle<<")"<<endl;
          }
          lineNumber++;
        }
      }
      else {
        if(cpuCycle >= issueClock) {
          if(casHMCWrapper->ReceiveTran(transactionBuffers[0], 0)) {
            pendingTran = false;
            transactionBuffers.erase(transactionBuffers.begin());
          }
        }					
      }
      casHMCWrapper->Update();
    }
  } else if (traceType == "active") { // Jiayi, active, 01/27
    int *diff = new int(0);
    int *active_arr = new int[100];
    for (uint64_t i = 0; i < 100; i++) {
      active_arr[i] = i;
      cout << active_arr[i] << "(0x" << &active_arr[i] << ") ";
    }
    cout << endl;
    cout << "Active network processing......." << endl;
    //bool gather = false;
    int gather = 0;
    for (uint64_t cpuCycle=0; cpuCycle<numSimCycles; cpuCycle++) {
      if (cpuCycle < 100) {
        if (cpuCycle % 3 == 2) {
          MakeActiveTransaction(ACTIVE_ADD, (uint64_t) diff, (uint64_t) &active_arr[cpuCycle], 0, 4);
        } else if (cpuCycle % 3 == 1) {
          MakeActiveTransaction(ACTIVE_ADD, (uint64_t) diff, (uint64_t) &active_arr[cpuCycle], 0, 11);
        } else {
          MakeActiveTransaction(ACTIVE_ADD, (uint64_t) diff, (uint64_t) &active_arr[cpuCycle], 0, 15);
        }
      } else if (gather < 4) {
        ++gather;
        MakeActiveTransaction(ACTIVE_GET, (uint64_t) diff, 0, 0, 0);
      }
      if (!transactionBuffers.empty()) {
        if (casHMCWrapper->ReceiveTran(transactionBuffers[0], 0)) {
          if (transactionBuffers[0]->transactionType == ACTIVE_GET) {
            transactionBuffers[0]->nthreads = 4;
          }
          transactionBuffers.erase(transactionBuffers.begin());
        }
      }
      casHMCWrapper->Update();
    }

    cout << "After active processing, diff = " << *diff << endl;
    delete diff;
    delete [] active_arr;
  } else if (traceType == "active2") {
    int *sum = new int(0);
    int *active_arr1 = new int[100];
    int *active_arr2 = new int[100];
    for (uint64_t i = 0; i < 100; i++) {
      active_arr1[i] = i;
      active_arr2[i] = i;
      //cout << active_arr1[i] << "(0x" << &active_arr1[i] << ") " << active_arr2[i] << "(0x" << &active_arr2[i] << ") ";
    }
    cout << endl;
    cout << "Active network processing......." << endl;
    //bool gather = false;
    int gather = 0;
    for (uint64_t cpuCycle=0; cpuCycle<numSimCycles; cpuCycle++) {
      if (cpuCycle < 100) {
        if (cpuCycle % 3 == 2) {
          MakeActiveTransaction(ACTIVE_MULT, (uint64_t) sum, (uint64_t) &active_arr1[cpuCycle], (uint64_t)
              &active_arr2[cpuCycle], 0, 7, 6);
              //&active_arr2[cpuCycle], 0, 4, 6);
        } else if (cpuCycle % 3 == 1) {
          MakeActiveTransaction(ACTIVE_MULT, (uint64_t) sum, (uint64_t) &active_arr1[cpuCycle], (uint64_t)
              &active_arr2[cpuCycle], 0, 11, 9);
              //&active_arr2[cpuCycle], 0, 11, 14);
        } else {
          MakeActiveTransaction(ACTIVE_MULT, (uint64_t) sum, (uint64_t) &active_arr1[cpuCycle], (uint64_t)
              &active_arr2[cpuCycle], 0, 15, 14);
              //&active_arr2[cpuCycle], 0, 15, 12);
        }
      } else if (gather < 4) {
        ++gather;
        MakeActiveTransaction(ACTIVE_GET, (uint64_t) sum, 0, 0, 0);
      }
      if (!transactionBuffers.empty()) {
        if (casHMCWrapper->ReceiveTran(transactionBuffers[0], 0)) {
          if (transactionBuffers[0]->transactionType == ACTIVE_GET) {
            transactionBuffers[0]->nthreads = 4;
          }
          transactionBuffers.erase(transactionBuffers.begin());
        }
      }
      casHMCWrapper->Update();
    }

    cout << "After active processing, sum = " << *sum << endl;
    delete sum;
    delete [] active_arr1;
    delete [] active_arr2;
  }

  transactionBuffers.clear();
  delete casHMCWrapper;
  casHMCWrapper = NULL;

  return 0;
}
*/
