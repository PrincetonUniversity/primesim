//===========================================================================
// core_manager.h 
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

#ifndef  CORE_MANAGER_H
#define  CORE_MANAGER_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <iostream>
#include <cmath>
#include <string>
#include "portability.H"
#include <syscall.h>
#include <utmpx.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <time.h>
#include <dlfcn.h>
#include "mpi.h"
#include "pin.H"
#include "instlib.H"
#include"xml_parser.h"
#include "common.h"



// Force each thread's data to be in its own data cache line so that
// multiple threads do not contend for the same data cache line.
// This avoids the false sharing problem.
class ThreadData
{
  public:
    ThreadData() : _count(0) {}
    double _count;
    uint8_t _pad[PADSIZE];
};

enum ThreadState
{
    DEAD    = 0,
    ACTIVE  = 1,
    SUSPEND = 2,
    FINISH  = 3,
    WAIT    = 4
};

//For futex syscalls
typedef struct SysFutex
{
    bool valid;
    int  count;
    uint64_t addr;
    uint64_t time;
} SysFutex;


class CoreManager
{
    public:
        void init(XmlSim* xml_sim, int num_procs_in, int rank_in);
        void getSimStartTime();
        void getSimFinishTime();
        void startSim(MPI_Comm *new_comm);
        void finishSim(int32_t code, void *v);
        void insCount(uint32_t ins_count_in, THREADID threadid);
        void execNonMem(uint32_t ins_count_in, THREADID threadid);
        void execMem(void * addr, THREADID threadid, uint32_t size, bool mem_type);
        void threadStart(THREADID threadid, CONTEXT *ctxt, int32_t flags, void *v);
        void threadFini(THREADID threadid, const CONTEXT *ctxt, int32_t code, void *v);
        void sysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, 
               ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, THREADID threadid);
        void sysAfter(ADDRINT ret, THREADID threadid);
        void syscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v);
        void syscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v);
        void report(ofstream *result);
        ~CoreManager();        
    private:
        double getAvgCycle(THREADID threadid);
        void barrier(THREADID threadid);
        struct timespec sim_start_time;
        struct timespec sim_finish_time;
        ThreadData cycle[THREAD_MAX];
        ThreadData ins_nonmem[THREAD_MAX];
        ThreadData ins_count[THREAD_MAX];
        MsgMem   *msg_mem[THREAD_MAX];
        MPI_Request   *send_req[THREAD_MAX];
        MPI_Request   *recv_req[THREAD_MAX];
        int delay[THREAD_MAX];
        int core[THREAD_MAX];
        int mpi_pos[THREAD_MAX];
        int thread_state[THREAD_MAX];
        SysFutex sys_wake[THREAD_MAX];
        SysFutex sys_wait[THREAD_MAX];
        uint32_t  thread_map[THREAD_MAX];
        int num_threads;
        int num_procs;
        int max_threads;
        int barrier_counter;
        uint64_t barrier_time;
        double cpi_nonmem;
        double freq;
        double sim_time;
        int syscall_count;
        int sync_syscall_count;
        int max_msg_size;
        uint32_t thread_sync_interval;
        uint32_t proc_sync_interval;
        uint32_t syscall_cost;
        int num_recv_threads;
        PIN_LOCK thread_lock;
        PIN_MUTEX mutex;
        PIN_SEMAPHORE sem;
        int rank;

};




#endif // CORE_MANAGER_H 
