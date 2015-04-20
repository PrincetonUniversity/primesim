//===========================================================================
// network.h 
//===========================================================================
/*
Copyright (c) 2015 Princeton University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY PRINCETON UNIVERSITY "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <inttypes.h>
#include <map>
#include <set>
#include <vector>
#include <pthread.h> 
#include "link.h"
#include "cache.h"

class Link;



enum Direction 
{
    EAST = 0,
    WEST = 1,
    NORTH = 2,
    SOUTH = 3,
    UP = 4,
    DOWN = 5
};

enum NetworkType
{
    MESH_2D = 0,
    MESH_3D = 1
};

typedef struct Coord
{
    int x;
    int y;
    int z;
} Coord;


class Network
{
    public:
       ~Network();
       bool init(int num_nodes_in, XmlNetwork* xml_net);
       uint64_t transmit(int sender, int receiver, int data_len, uint64_t timer);
       int getNumNodes();
       int getNetType();
       int getNetWidth();
       int getHeaderFlits();
       Coord getLoc(int node_id); 
       int getNodeId(Coord loc);
       Link* getLink(Coord node_id, Direction direction);
       void report(ofstream* result);
   private:
       int net_type;
       int num_nodes;
       int net_width;
       int data_width;
       int header_flits;
       uint64_t router_delay;
       uint64_t link_delay;
       uint64_t inject_delay;
       Link*** link;
       uint64_t num_access;
       uint64_t total_delay;
       uint64_t total_router_delay;
       uint64_t total_link_delay;
       uint64_t total_inject_delay;
       uint64_t total_distance;
       double avg_delay;
       pthread_mutex_t mutex;
        
};


#endif //NETWORK_H
