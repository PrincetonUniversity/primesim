//===========================================================================
// uncore_manager.h 
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

#ifndef  UNCORE_MANAGER_H
#define  UNCORE_MANAGER_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <sstream>
#include <list>
#include <pthread.h> 
#include "system.h"
#include "thread_sched.h"
#include "xml_parser.h"
#include "cache.h"
#include "network.h"
#include "common.h"

#include "mpi.h"



class UncoreManager
{
    public:
        void init(XmlSim* xml_sim);
        void getSimStartTime();
        void getSimFinishTime();
        int allocCore(int prog_id, int thread_id);
        int deallocCore(int prog_id, int thread_id);
        int getCoreId(int prog_id, int thread_id);
        int uncore_access(int core_id, InsMem* ins_mem, int64_t timer);
        void report(ofstream *result);
        ~UncoreManager();        
    private:
        struct timespec sim_start_time;
        struct timespec sim_finish_time;
        System sys;
        ThreadSched thread_sched;
};




#endif // UNCORE_MANAGER_H 
