#include "InputBuffer.h"

namespace CasHMC
{
  InputBuffer::InputBuffer(ofstream &debugOut_, ofstream &stateOut_, unsigned id_, string headerPrefix_)
    : DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_CROSS_BUF, MAX_CROSS_BUF),
    bufID(id_)
  {
    classID << bufID;
    header = "         (";
    header += headerPrefix_ + "_BUF" + classID.str() + ")";
  }

  InputBuffer::~InputBuffer()
  {
  }

  //
  //Callback Adding packet
  //
  void InputBuffer::CallbackReceiveDown(Packet *downEle, bool chkReceive)
  {
  }
  void InputBuffer::CallbackReceiveUp(Packet *upEle, bool chkReceive)
  {
  }

  void InputBuffer::Update()
  {
  }

  //
  //Print current state in state log file
  //
  void InputBuffer::PrintState()
  {
    int realInd = 0;
    if(downBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN("Down ");
      for(int i=0; i<downBufferMax; i++) {
        if(i>0 && i%8==0) {
          STATEN(endl<<"                      ");
        }
        if(i < downBuffers.size()) {
          if(downBuffers[i] != NULL)	realInd = i;
          STATEN(*downBuffers[realInd]);
        }
        else if(i == downBufferMax-1) {
          STATEN("[ - ]");
        }
        else {
          STATEN("[ - ]...");
          break;
        }
      }
      STATEN(endl);
    }

    if(upBuffers.size()>0) {
      STATEN(ALI(17)<<header);
      STATEN(" Up  ");
      if(upBuffers.size() < upBufferMax) {
        for(int i=0; i<upBufferMax; i++) {
          if(i>0 && i%8==0) {
            STATEN(endl<<"                      ");
          }
          if(i < upBuffers.size()) {
            if(upBuffers[i] != NULL)	realInd = i;
            STATEN(*upBuffers[realInd]);
          }
          else if(i == upBufferMax-1) {
            STATEN("[ - ]");
          }
          else {
            STATEN("[ - ]...");
            break;
          }
        }
      }
      else {
        for(int i=0; i<upBuffers.size(); i++) {
          if(i>0 && i%8==0) {
            STATEN(endl<<"                      ");
          }
          if(upBuffers[i] != NULL)	realInd = i;
          STATEN(*upBuffers[realInd]);
        }
      }
      STATEN(endl);
    }
  }

} // namespace CasHMC
