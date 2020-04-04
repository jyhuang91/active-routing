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

#ifndef TRANSACTION_H
#define TRANSACTION_H

//Transaction.h

#include <stdint.h>		//uint64_t
#include <stdlib.h>		//exit(0)
#include <iomanip>		//setw()
#include <iostream> 	//ostream
#include <sstream>		//stringstream

#include "SimConfig.h"
#include "TranTrace.h"
#include "TranStatistic.h"

using namespace std;

namespace CasHMC
{

  static unsigned tranGlobalID=0;

  enum TransactionType
  {
    DATA_READ,
    DATA_WRITE,
    RETURN_DATA,
    //ATOMICS commands for HMC
    ATM_2ADD8, ATM_ADD16, ATM_P_2ADD8, ATM_P_ADD16, ATM_2ADDS8R, ATM_ADDS16R, ATM_INC8, ATM_P_INC8, //ARITHMETIC ATOMICS
    ATM_XOR16, ATM_OR16, ATM_NOR16, ATM_AND16, ATM_NAND16, 											//BOOLEAN ATOMICS
    ATM_CASGT8, ATM_CASLT8, ATM_CASGT16, ATM_CASLT16, ATM_CASEQ8, ATM_CASZERO16, ATM_EQ16, ATM_EQ8, //COMPARISON ATOMICS
    ATM_BWR, ATM_P_BWR, ATM_BWR8R, ATM_SWAP16, 														//BITWISE ATOMICS
    // ActiveRouting commands, Jiayi, 01/27
    ACTIVE_GET, ACTIVE_ADD, ACTIVE_MULT, ACTIVE_DOT,
    PIM_DOT, PIM_ATOMIC
  };

  class Transaction
  {
    public:
      //
      //Functions
      //
      Transaction(TransactionType tranType, uint64_t addr, unsigned size, TranStatistic *statis, int src, int dest);
      // Jiayi, 01/31
      Transaction(TransactionType tranType, uint64_t dest_addr, uint64_t src_addr, unsigned size, TranStatistic *statis,
          int src, int dest);
      // Jiayi, 03/23/17
      Transaction(TransactionType tranType, uint64_t dest_addr, uint64_t src_addr1, uint64_t src_addr2, unsigned size,
          TranStatistic *statis, int src, int dest1, int dest2);
      virtual ~Transaction();
      void ReductGlobalID();
      unsigned long long return_transac_id();
      bool coalesce(uint64_t src_addr2);

      //
      //Fields
      //
      TranTrace *trace;
      TransactionType transactionType;	//Type of transaction (defined above)
      uint64_t address;					//Physical address of request
      unsigned dataSize;					//[byte] Size of data
      unsigned transactionID;				//Unique identifier
      unsigned LNG;

      // Jiayi, Extend for active router, under develop
      uint64_t dest_address;
      uint64_t src_address1;
      uint64_t src_address2;
      std::vector<uint64_t> src_addresses;
      // Ram & Jiayi, src and dest cubeID
      int src_cube;
      int dest_cube1, dest_cube2;
      int nthreads; // Jiayi, 03/31/17
      int bufPopDelay;//FIXME: for dualvector compile only, won't be used
  };

  ostream& operator<<(ostream &out, const Transaction &t);
}

#endif
