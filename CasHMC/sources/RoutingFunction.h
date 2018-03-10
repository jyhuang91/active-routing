#ifndef ROUTING_FUNCTION_H
#define ROUTING_FUNCTION_H

#include <vector>

#include <iostream>
#include "SimConfig.h"

using namespace std;

namespace CasHMC
{

  class RoutingFunction
  {
    protected: 
      RoutingFunction(int dim);
    public: 

      virtual ~RoutingFunction();
      virtual int findNextLink(int &link, unsigned nodeId, unsigned dest, bool req = 0) = 0;
      static RoutingFunction *New(int dim, TOPOLOGY top);
      int _nodes;
      TOPOLOGY topology;
  };

  class dFlyRF : public RoutingFunction 
  {
    public:
      dFlyRF(int dim);
      virtual int findNextLink(int &link, unsigned nodeId, unsigned dest, bool req);
      void findXY(int *x, int *y, int nodeId);
  };

  class meshRF : public RoutingFunction 
  {
    public:
      meshRF(int dim);
      virtual int findNextLink(int &link, unsigned nodeId, unsigned dest, bool req);
      void findXY(int *x, int *y, int nodeId);
  };

  class dualRF : public RoutingFunction 
  {
    public:
      dualRF(int dim);
      virtual int findNextLink(int &link, unsigned nodeId, unsigned dest, bool req);
  };

  class defaultRF : public RoutingFunction 
  {
    public:
      defaultRF(int dim);
      virtual int findNextLink(int &link, unsigned nodeId, unsigned dest, bool req);
  };
}

#endif // define ROUTING_FUNCTION_H
