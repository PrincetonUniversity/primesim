//===========================================================================
// network.cpp implements 2D and 3D mesh network. Only link contention is 
// calculated by queue model. The routing algorithm is horizontal direction first. 
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
    * Neither the name of Princeton University nor the
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

#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <cstring>
#include <inttypes.h>
#include <assert.h>

#include "network.h"
#include "common.h"


using namespace std;

bool Network::init(int num_nodes_in, XmlNetwork* xml_net)
{
    int i, j, k;
    num_nodes = num_nodes_in;
    net_type = xml_net->net_type;
    if (net_type == MESH_3D) {
        net_width = (int)ceil(cbrt(num_nodes));
    }
    else {
        net_width = (int)ceil(sqrt(num_nodes));
    }
    header_flits = xml_net->header_flits;
    data_width = xml_net->data_width;
    router_delay = xml_net->router_delay;
    link_delay = xml_net->link_delay;
    inject_delay = xml_net->inject_delay;
    if (net_type == MESH_3D) {
        link = new Link** [net_width-1];
        for (i = 0; i < net_width-1; i++) {
            link[i] = new Link* [net_width];
            for (j = 0; j < net_width; j++) {
                link[i][j] = new Link [3*net_width];
                for (k = 0; k < 3*net_width; k++) {
                    link[i][j][k].init(xml_net->link_delay);
                }
            }
        }
    }
    else {
        link = new Link** [net_width-1];
        for (i = 0; i < net_width-1; i++) {
            link[i] = new Link* [2*net_width];
            for (j = 0; j < 2*net_width; j++) {
                link[i][j] = new Link();
                link[i][j]->init(xml_net->link_delay);
            }
        }

    }
    num_access = 0;
    total_delay = 0;
    total_router_delay = 0;
    total_link_delay = 0;
    total_inject_delay = 0;
    total_distance = 0;
    avg_delay = 0;
    pthread_mutex_init(&mutex, NULL);
    return true;
}

//Calculate packet communication latency
uint64_t Network::transmit(int sender, int receiver, int data_len, uint64_t timer)
{
    if(sender == receiver) {
        return 0;
    }
    assert(sender >= 0 && sender < num_nodes);
    assert(receiver >= 0 && receiver < num_nodes);
    int packet_len = header_flits + (int)ceil((double)data_len/data_width); 
    Coord loc_sender = getLoc(sender); 
    Coord loc_receiver = getLoc(receiver); 
    Coord loc_cur = loc_sender;
    Direction direction;
    uint64_t    local_timer = timer;
    uint64_t    local_distance = 0;
    Link* link_cur;

    //Injection delay
    local_timer += inject_delay;
    
    direction = loc_receiver.x > loc_cur.x ? EAST : WEST;
    local_distance += abs(loc_receiver.x - loc_cur.x);
    while (loc_receiver.x != loc_cur.x) {
        local_timer += router_delay;
        link_cur = getLink(loc_cur, direction);
        assert(link_cur != NULL);
        local_timer += link_cur->access(local_timer, packet_len);
        loc_cur.x = (direction == EAST) ? (loc_cur.x + 1) : (loc_cur.x - 1);
    }

    direction = loc_receiver.y > loc_cur.y ? SOUTH : NORTH;
    local_distance += abs(loc_receiver.y - loc_cur.y);
    while (loc_receiver.y != loc_cur.y) {
        local_timer += router_delay;
        link_cur = getLink(loc_cur, direction);
        assert(link_cur != NULL);
        local_timer += link_cur->access(local_timer, packet_len);
        loc_cur.y = (direction == SOUTH) ? (loc_cur.y + 1) : (loc_cur.y - 1);
    }
 
    direction = loc_receiver.z > loc_cur.z ? UP : DOWN;
    local_distance += abs(loc_receiver.z - loc_cur.z);
    while (loc_receiver.z != loc_cur.z) {
        local_timer += router_delay;
        link_cur = getLink(loc_cur, direction);
        assert(link_cur != NULL);
        local_timer += link_cur->access(local_timer, packet_len);
        loc_cur.z = (direction == UP) ? (loc_cur.z + 1) : (loc_cur.z - 1);
    }

    local_timer += router_delay;
    //Pipe delay 
    local_timer += packet_len - 1; 
 

    pthread_mutex_lock(&mutex);
    num_access++;
    total_delay += local_timer - timer;
    total_router_delay += (local_distance+1) * router_delay;
    total_link_delay += local_timer - timer - (local_distance+1)*router_delay - (packet_len-1) - inject_delay;
    total_inject_delay += inject_delay;
    total_distance += local_distance;
    pthread_mutex_unlock(&mutex);
    return (local_timer - timer);
}


Coord Network::getLoc(int node_id)
{
    Coord loc;
    if (net_type == MESH_3D) {
        loc.x = (node_id % (net_width * net_width)) % net_width;
        loc.y = (node_id % (net_width * net_width)) / net_width;
        loc.z = node_id / (net_width * net_width);
    }
    else {
        loc.x = node_id % net_width;
        loc.y = node_id / net_width;
        loc.z = 0;
    }
    return loc;
}

int Network::getNodeId(Coord loc)
{
    int node_id;
    if (net_type == MESH_3D) {
        node_id = loc.x + loc.y * net_width + loc.z * net_width * net_width;
    }
    else {
        node_id = loc.x + loc.y * net_width;
    }
    return node_id;
}

int Network::getNumNodes()
{
    return num_nodes;
}

int Network::getNetType()
{
    return net_type;
}

int Network::getNetWidth()
{
    return net_width;
}


int Network::getHeaderFlits()
{
    return header_flits;
}

//Find the pointer to the link of a specific location
Link* Network::getLink(Coord node_id, Direction direction)
{
    Coord link_id;
    
    if(net_type == MESH_3D) {
        switch(direction) {
            case EAST:
               link_id.x = node_id.x; 
               link_id.y = node_id.y;
               link_id.z = node_id.z;
               break;
            case WEST:
               link_id.x = node_id.x - 1; 
               link_id.y = node_id.y;
               link_id.z = node_id.z;
               break;
            case NORTH:
               link_id.x = node_id.y - 1; 
               link_id.y = node_id.z;
               link_id.z = node_id.x + net_width;
               break;
            case SOUTH:
               link_id.x = node_id.y; 
               link_id.y = node_id.z;
               link_id.z = node_id.x + net_width;
               break;
            case UP:
               link_id.x = node_id.z; 
               link_id.y = node_id.x;
               link_id.z = node_id.y + 2 * net_width;
               break;
            case DOWN:
               link_id.x = node_id.z - 1; 
               link_id.y = node_id.x;
               link_id.z = node_id.y + 2 * net_width;
               break;
            default:
               link_id.x = node_id.x; 
               link_id.y = node_id.y;
               link_id.z = node_id.z;
               break;
        }
        if((link_id.x >= 0) && (link_id.x < net_width-1)
         &&(link_id.y >= 0) && (link_id.y < net_width)
         &&(link_id.z >= 0) && (link_id.y < 3*net_width)) {
            return &link[link_id.x][link_id.y][link_id.z];
        }
        else {
            cerr <<"# of nodes: "<<num_nodes<<endl;
            cerr <<"Network width: "<<net_width<<endl;
            cerr <<"Direction: "<<direction<<endl;
            cerr <<"Node coordinate: ("<<node_id.x<<", "<<node_id.y<<", "<<node_id.z<<")\n";
            cerr <<"Link coordinate: ("<<link_id.x<<", "<<link_id.y<<", "<<link_id.z<<")\n";
            cerr <<"Can't find correct link id!\n";
            return NULL;
        }
    }
    else {
        switch(direction) {
            case EAST:
               link_id.x = node_id.x; 
               link_id.y = node_id.y;
               break;
            case WEST:
               link_id.x = node_id.x - 1; 
               link_id.y = node_id.y;
               break;
            case NORTH:
               link_id.x = node_id.y - 1; 
               link_id.y = node_id.x + net_width;
               break;
            case SOUTH:
               link_id.x = node_id.y; 
               link_id.y = node_id.x + net_width;
               break;
            default:
               link_id.x = node_id.x; 
               link_id.y = node_id.y;
               break;
        }
        if((link_id.x >= 0) && (link_id.x < net_width-1)
         &&(link_id.y >= 0) && (link_id.y < 2*net_width)) {
            return link[link_id.x][link_id.y];
        }
        else {
            cerr <<"# of nodes: "<<num_nodes<<endl;
            cerr <<"Network width: "<<net_width<<endl;
            cerr <<"Direction: "<<direction<<endl;
            cerr <<"Node coordinate: ("<<node_id.x<<", "<<node_id.y<<")\n";
            cerr <<"Link coordinate: ("<<link_id.x<<", "<<link_id.y<<")\n";
            cerr <<"Can't find correct link id!\n";
            return NULL;
        }
    }
}


void Network::report(ofstream* result)
{

    avg_delay = (double)total_delay / num_access; 
    *result << "Network Stat:\n";
    *result << "# of accesses: " << num_access <<endl;
    *result << "Total network communication distance: " << total_distance <<endl;
    *result << "Total network delay: " << total_delay <<endl;
    *result << "Total router delay: " << total_router_delay <<endl;
    *result << "Total link delay: " << total_link_delay <<endl;
    *result << "Total inject delay: " << total_inject_delay <<endl;
    *result << "Total contention delay: " << total_link_delay - total_distance*link_delay <<endl;
    *result << "Average network delay: " << avg_delay <<endl <<endl;
}

Network::~Network()
{
    int i, j;
    if (net_type == MESH_3D) {
        for (i = 0; i < net_width-1; i++) {
            for (j = 0; j < net_width; j++) {
                delete [] link[i][j];
            }
            delete [] link[i];
        } 
        delete [] link;
    }
    else {
        for (i = 0; i < net_width-1; i++) {
            for (j = 0; j < 2*net_width; j++) {
                delete link[i][j];
            }
            delete [] link[i];
        } 
        delete [] link;

    }
    pthread_mutex_destroy(&mutex);
}
