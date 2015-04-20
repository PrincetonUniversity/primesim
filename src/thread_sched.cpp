//===========================================================================
// thread_sched.cpp schedules threads to cores
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


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <inttypes.h>
#include <cmath>
#include <assert.h>

#include "thread_sched.h"
#include "common.h"


void ThreadSched::init(int num_cores_in)
{
    num_cores = num_cores_in;
    core_stat = new int [num_cores];
    memset(core_stat, 0, sizeof(int) * num_cores);
    core_map.clear();
}


//Allocate a core for a thread
int ThreadSched::allocCore(int prog_id, int thread_id)
{
    for(int i = 0; i < num_cores; i++)
    {
        if(core_stat[i] == 0)
        {
            core_stat[i] = prog_id;
            core_map[Key(prog_id, thread_id)] =  i;
            return i;
        }
    }
    return -1; 
}


//Return the core id for the allocated thread
int ThreadSched::getCoreId(int prog_id, int thread_id)
{
    return core_map[Key(prog_id, thread_id)];
}

//De-allocate core for the thread
int ThreadSched::deallocCore(int prog_id, int thread_id)
{
    int core_id = core_map[Key(prog_id, thread_id)];
    if ((core_id >= 0) && (core_stat[core_id] == 1)) {
        core_stat[core_id] = 0;
        return 1;
    }
    else {
        return 0;
    }
}

int ThreadSched::getThreadCount(int prog_id) 
{
    int thread_count = 0;
    CoreMap::iterator pos;
    for (pos = core_map.begin(); pos != core_map.end(); ++pos)
    {
        if(pos->first.first == prog_id)
        {
            thread_count++;
        }
    }
    return thread_count;
}



void ThreadSched::report(ofstream *result)
{
    *result << "Core Allocation:\n";

    CoreMap::iterator pos = core_map.begin();
    for (pos = core_map.begin(); pos != core_map.end(); ++pos)
    {
        *result << "(proc ID: " << pos->first.first << " ,thread ID: " <<pos->first.second << ") => "
               << "core ID: " << pos->second << endl;
    }
    *result<<endl;

}

ThreadSched::~ThreadSched()
{
    delete [] core_stat;
}
