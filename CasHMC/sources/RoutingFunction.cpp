
#include <math.h>
#include "RoutingFunction.h"
int inServicelink = 1;
using namespace std;
namespace CasHMC
{
  RoutingFunction::RoutingFunction(int nodes)
  {
    _nodes = nodes;
  }

  RoutingFunction *RoutingFunction::New(int dim, TOPOLOGY top)
  {
    RoutingFunction *result;
    if(top == DUAL_HMC){
      result = new dualRF(dim);
    }else if(top == MESH){
      result = new meshRF(dim);
    }else if(top == DFLY){
      result = new dFlyRF(dim);
    }else if (top == DEFAULT) {
      result  = new defaultRF(dim);
    } else {
      cout << "Wrong routing mode" << endl;
    }
    return result;
  }

  RoutingFunction::~RoutingFunction()
  {
  }
#if 0
  int RoutingFunction::findNextLink()
  {
    return 0;
  }
#endif

  defaultRF::defaultRF(int dim) : RoutingFunction(dim)
  {  
  }

  int defaultRF::findNextLink(int &link, unsigned nodeId, unsigned dest, bool req = false)
  {
    if(++link >= NUM_LINKS)
      link=0; //RAMEDIT
    return link;
  }

  dualRF::dualRF(int dim) : RoutingFunction(dim)
  {  
  }

  int dualRF::findNextLink(int &link, unsigned nodeId, unsigned dest, bool req = false)
  {
    if(dest == nodeId){
      return 0;
    }else{
      if(++link >= NUM_LINKS)
        link=1; //RAMEDIT
      return link;
    }
  }

  dFlyRF::dFlyRF(int dim) : RoutingFunction(dim)
  {  
  }


  int dFlyRF::findNextLink(int &link, unsigned nodeId, unsigned dest, bool req = false)
  {
    int next_link = -1;
    int my_node = nodeId;
    int dest_node = dest;
    int my_quad=0, dest_quad=0;
    int nodes = _nodes;
    while(nodes > 1){
      dest_node = dest_node - (dest_quad)*nodes;
      my_node = my_node - (my_quad)*nodes;
      nodes = nodes/4;
      dest_quad = dest_node/nodes;
      my_quad = my_node/nodes;
      if(my_quad == dest_quad){
        //go ahead
      }else{
        next_link = dest_quad; 
        break;
      }
      // cout <<"NODEID:" <<nodeId<<"DEST:"<<dest<<"nodes:"<<nodes<<"my_node: "<< my_node<<"dest_node: "<<dest_node<<"next_link:"<<next_link<< endl; 
    }
    if(next_link == -1){
      next_link = my_quad;
    }

    return next_link;
  }
  meshRF::meshRF(int dim) : RoutingFunction(dim)
  {  
  }

  int meshRF::findNextLink(int &link, unsigned nodeId, unsigned dest, bool req = false)
  {
    int myX = -1, myY = -1, destX = -1, destY = -1;
    findXY(&myX, &myY, nodeId);
    findXY(&destX, &destY, dest);
    if(req == false){  //X-Y routing
      if(myX == destX){
        if(myY == destY){
          if(myX == 0){
            link = LEFT_LINK;
          }else{
            link = RIGHT_LINK;
          }
        }else{
          if(myY > destY){
            link = TOP_LINK;
          }else{
            link = BOT_LINK;
          }
        }
      }else{
        if(myX > destX){
          link = LEFT_LINK;
        }else{
          link = RIGHT_LINK;
        }
      }
    }else{      //Y-X routing
      if(myY == destY){   
        if(myX == destX){
          if(myX == 0){
            link = LEFT_LINK;
          }else{
            link = RIGHT_LINK;
          }
        }else{
          if(myX > destX){
            link = LEFT_LINK;
          }else{
            link = RIGHT_LINK;
          }
        }
      }else{
        if(myY > destY){
          link = TOP_LINK;
        }else{
          link = BOT_LINK;
        }
      }
    }
    return link;
  }

  void meshRF::findXY(int *x, int *y, int nodeId)
  {
    int dimension = sqrt(_nodes);
    *x = (int) (nodeId%dimension);
    *y = (int) (nodeId/dimension);
  }

}
