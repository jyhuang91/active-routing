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

#include "DRAMCommand.h"

namespace CasHMC
{

  DRAMCommand::DRAMCommand(DRAMCommandType cmdtype, unsigned tag, unsigned bnk, unsigned col, unsigned rw, unsigned dSize,
      bool pst, TranTrace *lat, bool last, PacketCommandType pktCMD, bool atm, bool seg):
    commandType(cmdtype),
    packetTAG(tag),
    bank(bnk),
    column(col),
    row(rw),
    dataSize(dSize),
    posted(pst),
    trace(lat),
    lastCMD(last),
    packetCMD(pktCMD),
    atomic(atm),
    segment(seg),
    addr(0),
    tran_tag(0),
    srcAddr1(0),
    srcAddr2(0),
    destAddr(0),
    src_cube(0),
    dest_cube(0),
    operandBufID(-1),
    VoperandBufID(-1)
  {
  }

  DRAMCommand::DRAMCommand(const DRAMCommand &dc)
  {
    trace = dc.trace;
    commandType = dc.commandType;
    packetTAG = dc.packetTAG;
    bank = dc.bank;
    column = dc.column;
    row = dc.row;
    dataSize = dc.dataSize;
    posted = dc.posted;
    lastCMD = dc.lastCMD;
    packetCMD = dc.packetCMD;
    atomic = dc.atomic;
    segment = dc.segment;

    // Jiayi, 03/13/17, 03/24/17
    addr = dc.addr;
    tran_tag = dc.tran_tag;
    srcAddr1 = dc.srcAddr1;
    srcAddr2 = dc.srcAddr2;
    destAddr = dc.destAddr;
    src_cube = dc.src_cube;
    dest_cube = dc.dest_cube;
    operandBufID = dc.operandBufID;
    VoperandBufID = dc.VoperandBufID;
  }

  DRAMCommand::~DRAMCommand()
  {
  }

  //
  //Defines "<<" operation for printing
  //
  ostream& operator<<(ostream &out, const DRAMCommand &dc)
  {
    string header;
    stringstream id;
    id << dc.packetTAG;

    switch(dc.commandType) {
      case ACTIVATE:
        header = "[C" + id.str() + "-ACT]";
        break;
      case READ:
        header = "[C" + id.str() + "-READ]";
        break;
      case READ_P:
        header = "[C" + id.str() + "-READ_P]";
        break;
      case WRITE:
        header = "[C" + id.str() + "-WRITE]";
        break;
      case WRITE_P:
        header = "[C" + id.str() + "-WRITE_P]";
        break;
      case PRECHARGE:
        header = "[C" + id.str() + "-PRE]";
        break;
      case REFRESH:
        header = "[C" + id.str() + "-REF]";
        break;
      case READ_DATA:
        header = "[C" + id.str() + "-R_DATA]";
        break;
      case WRITE_DATA:
        header = "[C" + id.str() + "-W_DATA]";
        break;
      case POWERDOWN_ENTRY:
        header = "[C" + id.str() + "-PD_ENT]";
        break;
      case POWERDOWN_EXIT:
        header = "[C" + id.str() + "-PD_EXT]";
        break;
      default:
        ERROR(" (DC) == Error - Trying to print unknown kind of bus packet");
        ERROR("         Type : "<<dc.commandType);
        //	exit(0);
    }
    out<<header;
    return out;
  }

} //namespace CasHMC
