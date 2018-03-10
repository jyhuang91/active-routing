#ifndef INPUTBUFFER_H
#define INPUTBUFFER_H

#include "DualVectorObject.h"
#include "SimConfig.h"

using namespace std;

namespace CasHMC
{
  class InputBuffer : public DualVectorObject<Packet, Packet>
  {
    public:
      //
      //Functions
      //
      InputBuffer(ofstream &debugOut_, ofstream &stateOut_, unsigned id_, string headerPrefix_);
      virtual ~InputBuffer();
      void CallbackReceiveDown(Packet *downEle, bool chkReceive);
      void CallbackReceiveUp(Packet *upEle, bool chkReceive);
      void Update();
      void PrintState();

      //
      //Feilds
      //
      unsigned bufID;
      DualVectorObject<Packet, Packet> *xbar;
  };

} // namespace CasHMC
#endif // INPUTBUFFER_H
