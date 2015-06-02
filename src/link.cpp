//===========================================================================
// link.cpp implements a link with fix propogation delay. The 
// contention delay is based on analytical M/G/1 queueing model from 
// the MIT Graphite Simulator
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
#include <cmath>
#include <string>
#include <cstring>
#include <inttypes.h>

#include "link.h"


using namespace std;

bool Link::init(uint64_t delay_in)
{
    pthread_mutex_init(&mutex, NULL);
    delay = delay_in;
    link_queue = QueueModel::create("history_tree", delay);
    return true;
}

// This function returns link delay including contention delay

uint64_t Link::access(uint64_t timer, int packet_len)
{
    pthread_mutex_lock(&mutex);
    uint64_t contention_delay = link_queue->computeQueueDelay(timer, packet_len);
    pthread_mutex_unlock(&mutex);
    return (contention_delay + delay);
}

Link::~Link()
{
    pthread_mutex_destroy(&mutex);
    delete link_queue;
}
