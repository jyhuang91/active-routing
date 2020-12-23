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

#include "Transaction.h"

using namespace std;

namespace CasHMC
{

  Transaction::Transaction(TransactionType tranType, uint64_t addr, unsigned size, TranStatistic *statis, int src = -1,
      int dest = -1):
    transactionType(tranType),
    address(addr),
    dataSize(size),
    src_cube(src),
    dest_cube1(dest)
  {
    LNG = 1;
    transactionID = tranGlobalID++;
    trace = new TranTrace(statis);

    dest_address = 0;
    src_address1 = 0;
    src_address2 = 0;
    lines = 0;
    nthreads = -1;
    dest_cube2 = -1;
  }

  // Jiayi, 01/31
  Transaction::Transaction(TransactionType tranType, uint64_t dest_addr, uint64_t src_addr, unsigned size, TranStatistic
      *statis, int src = -1, int dest = -1):
    transactionType(tranType),
    dest_address(dest_addr),
    src_address1(src_addr),
    dataSize(size),
    src_cube(src),
    dest_cube1(dest)
  {
    LNG = 1;
    transactionID = tranGlobalID++;
    trace = new TranTrace(statis);

    src_address2 = 0;
    lines = 0;
    nthreads = -1;
    dest_cube2 = -1;
  }

  Transaction::Transaction(TransactionType tranType, uint64_t dest_addr, uint64_t src_addr1, uint64_t src_addr2, unsigned size, TranStatistic
      *statis, int src = -1, int dest1 = -1, int dest2 = -1):
    transactionType(tranType),
    dest_address(dest_addr),
    src_address1(src_addr1),
    src_address2(src_addr2),
    dataSize(size),
    src_cube(src),
    dest_cube1(dest1),
    dest_cube2(dest2)
  {
    LNG = 1;
    transactionID = tranGlobalID++;
    trace = new TranTrace(statis);
    lines = 0;
    nthreads = -1;
}

  Transaction::~Transaction()
  {
  }

  //
  //Reduction tranGlobalID
  //
  void Transaction::ReductGlobalID()
  {
    tranGlobalID--;
  }

  unsigned long long Transaction::return_transac_id(){
      return transactionID;
  }

  //
  //Defines "<<" operation for printing
  //
  ostream& operator<<(ostream &out, const Transaction &t)
  {
    string header;
    stringstream id;
    id << t.transactionID;

    switch(t.transactionType) {
      case DATA_READ:   header = "[T" + id.str() + "-Read]";  break;
      case DATA_WRITE:  header = "[T" + id.str() + "-Write]"; break;
      case RETURN_DATA: header = "[T" + id.str() + "-Data]";  break;
      //ATOMICS commands for HMC
      case ATM_2ADD8:   header = "[T" + id.str() + "-2ADD8]";   break;
      case ATM_ADD16:   header = "[T" + id.str() + "-ADD16]";   break;
      case ATM_P_2ADD8: header = "[T" + id.str() + "-P_2ADD8]"; break;
      case ATM_P_ADD16: header = "[T" + id.str() + "-P_ADD16]"; break;
      case ATM_2ADDS8R: header = "[T" + id.str() + "-2ADDS8R]"; break;
      case ATM_ADDS16R: header = "[T" + id.str() + "-ADDS16R]"; break;
      case ATM_INC8:    header = "[T" + id.str() + "-INC8]";    break;
      case ATM_P_INC8:  header = "[T" + id.str() + "-P_INC8]";  break;
      case ATM_XOR16:   header = "[T" + id.str() + "-XOR16]";   break;
      case ATM_OR16:    header = "[T" + id.str() + "-OR16]";    break;
      case ATM_NOR16:   header = "[T" + id.str() + "-NOR16]";   break;
      case ATM_AND16:   header = "[T" + id.str() + "-AND16]";   break;
      case ATM_NAND16:  header = "[T" + id.str() + "-NAND16]";  break;
      case ATM_CASGT8:  header = "[T" + id.str() + "-CASGT8]";  break;
      case ATM_CASLT8:  header = "[T" + id.str() + "-CASLT8]";  break;
      case ATM_CASGT16: header = "[T" + id.str() + "-CASGT16]"; break;
      case ATM_CASLT16: header = "[T" + id.str() + "-CASLT16]"; break;
      case ATM_CASEQ8:  header = "[T" + id.str() + "-CASEQ8]";  break;
      case ATM_CASZERO16: header = "[T" + id.str() + "-CASZR16]"; break;
      case ATM_EQ16:  header = "[T" + id.str() + "-EQ16]";  break;
      case ATM_EQ8:   header = "[T" + id.str() + "-EQ8]";   break;
      case ATM_BWR:   header = "[T" + id.str() + "-BWR]";   break;
      case ATM_P_BWR: header = "[T" + id.str() + "-P_BWR]"; break;
      case ATM_BWR8R: header = "[T" + id.str() + "-BWR8R]"; break;
      case ATM_SWAP16:  header = "[T" + id.str() + "-SWAP16]";  break;
      // ACTIVE commands, Jiayi, 01/27
      case ACTIVE_GET:  header = "[T" + id.str() + "-ACT_GET]"; break;
      case ACTIVE_ADD:  header = "[T" + id.str() + "-ACT_ADD]"; break;
      case ACTIVE_MULT: header = "[T" + id.str() + "-ACT_MULT]"; break;
      case ACTIVE_DOT:  header = "[T" + id.str() + "-ACT_DOT]"; break;
      case PIM_DOT:     header = "[T" + id.str() + "-PEI_DOT]"; break;
      case PIM_ATOMIC:  header = "[T" + id.str() + "-PEI_ATOMIC"; break;
      default:
        ERROR(" (TS) == Error - Trying to print unknown kind of transaction type");
        ERROR("         T"<<t.transactionID<<" [?"<<t.transactionType<<"?] [0x"<<hex<<setw(16)<<setfill('0')<<t.address<<dec<<"]");
        exit(0);
    }
    out<<header;
    return out;
  }

} //namespace CasHMC
