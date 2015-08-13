//===========================================================================
// core_manager.cpp manages the cores
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
#include <sstream>
#include <string>
#include <cstring>
#include <inttypes.h>
#include <cmath>
#include <assert.h>

#include "common.h"
#include "core_manager.h"

using namespace std;


void CoreManager::init(XmlSim* xml_sim, int num_procs_in, int rank_in)
{
    num_threads = 0;
    max_threads = 0;
    barrier_counter = 0;
    barrier_time = 0;
    sim_time = 0;
    syscall_count = 0;
    sync_syscall_count = 0;


    cpi_nonmem = xml_sim->sys.cpi_nonmem;
    max_msg_size = xml_sim->max_msg_size;
    thread_sync_interval = xml_sim->thread_sync_interval;
    proc_sync_interval = xml_sim->proc_sync_interval;
    syscall_cost = xml_sim->syscall_cost;
    freq = xml_sim->sys.freq;
    num_recv_threads = xml_sim->num_recv_threads;
    num_procs = num_procs_in;
    rank = rank_in;


    for(int i = 0; i < THREAD_MAX; i++) {
        msg_mem[i] = new MsgMem [max_msg_size + 1];
        send_req[i] = new MPI_Request [max_msg_size];
        recv_req[i] = new MPI_Request [max_msg_size];
        memset(msg_mem[i], 0, (max_msg_size + 1) * sizeof(MsgMem));
        memset(send_req[i], 0, max_msg_size * sizeof(MPI_Request));
        memset(recv_req[i], 0, max_msg_size * sizeof(MPI_Request));
        thread_state[i] = DEAD;
        thread_map[i] = -1;
        mpi_pos[i] = 1;
    }
    memset(sys_wake, 0, sizeof(sys_wake));
    memset(sys_wait, 0, sizeof(sys_wait));

    PIN_InitLock(&thread_lock);
    PIN_MutexInit(&mutex);
    PIN_SemaphoreInit(&sem);
    PIN_SemaphoreClear(&sem);

}

void CoreManager::startSim(MPI_Comm * new_comm)
{
    msg_mem[0][0].message_type = PROCESS_STARTING;
    MPI_Send(&msg_mem[0][0], sizeof(MsgMem), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    
    MPI_Barrier(*new_comm);
    barrier_time = thread_sync_interval;
}


// This function is called before every instruction is executed
void CoreManager::insCount(uint32_t ins_count_in, THREADID threadid)
{
    ins_count[threadid]._count += ins_count_in;
}


// This function implements periodic barriers across all threads within one process and all processes 
void CoreManager::barrier(THREADID threadid)
{
    int barrier_flag;
    uint32_t  backoff_time;
    if ((num_threads > 1 || num_procs > 1) && thread_state[threadid] == ACTIVE 
    && (cycle[threadid]._count >= barrier_time))
    {
        //Send out all remaining memory requests in the buffer
        if (mpi_pos[threadid] > 1) {
            msg_mem[threadid][0].mem_type = 0;
            msg_mem[threadid][0].addr_dmem = mpi_pos[threadid];
            msg_mem[threadid][0].mem_size = threadid;
            msg_mem[threadid][0].message_type = MEM_REQUESTS;
            MPI_Send(msg_mem[threadid], mpi_pos[threadid] * sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);
            MPI_Recv(&delay[threadid], 1, MPI_INT, 0, threadid, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (delay[threadid] == -1) {
                cerr<<"An error occurs in cache system\n";
                MPI_Abort(MPI_COMM_WORLD, -1);
                PIN_ExitApplication(-1);
            }
            cycle[threadid]._count += delay[threadid];
            mpi_pos[threadid] = 1;
        }

        PIN_MutexLock(&mutex);
        thread_state[threadid] = WAIT;
        if (barrier_counter == 0) {
            //Release all threads that are still waiting in the previous barrier 
            PIN_SemaphoreSet(&sem);
            barrier_flag = 1;
            while(barrier_flag) {
                barrier_flag = 0;
                for(int i = 0; i< max_threads; i++) {
                    if(((uint32_t)i != threadid) && thread_state[i] == WAIT) {
                        barrier_flag = 1;
                    }    
                }
            }
            PIN_SemaphoreClear(&sem);
        }
        barrier_counter++;
        if (barrier_counter == num_threads) {
            if ((num_procs > 1) && ((int)(barrier_time/thread_sync_interval) % (int)(proc_sync_interval/thread_sync_interval) == 0)) {
                msg_mem[threadid][0].message_type = INTER_PROCESS_BARRIERS;

                MPI_Send(&msg_mem[threadid][0], sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);
                MPI_Recv(&delay[threadid], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                num_procs = delay[threadid];
            }
            barrier_time += thread_sync_interval;
            barrier_counter = 0;
            PIN_SemaphoreSet(&sem);
        }
        PIN_MutexUnlock(&mutex);
        backoff_time = 1; 
        while (!PIN_SemaphoreTimedWait(&sem, backoff_time)) {
            //Leave barrier once the Semaphore is set
            if (!PIN_MutexTryLock(&mutex)) {
                if (PIN_SemaphoreIsSet(&sem)) {
                    break;
                }
            }
            else if (PIN_SemaphoreIsSet(&sem)) {
                PIN_MutexUnlock(&mutex);
                break;
            }
            else if (barrier_counter == num_threads) {
                if ((num_procs > 1) && (barrier_time % (proc_sync_interval) == 0)) {
                    msg_mem[threadid][0].message_type = INTER_PROCESS_BARRIERS;

                    MPI_Send(&msg_mem[threadid][0], sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);
                    MPI_Recv(&delay[threadid], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    num_procs = delay[threadid];

                }
                barrier_time += thread_sync_interval;
                barrier_counter = 0;
                PIN_SemaphoreSet(&sem);
                PIN_MutexUnlock(&mutex);
                break;
            }
            else {
                PIN_MutexUnlock(&mutex);
                PIN_Yield();
            }
        }
        thread_state[threadid] = ACTIVE;
        
    }
    //Update barrier time in the initial single-threaded phase
    else if (num_threads == 1 && num_procs == 1) {
        barrier_time = cycle[threadid]._count;
    }
}



// This function returns the average timer accoss all cores
double CoreManager::getAvgCycle(THREADID threadid)
{
    double avg_cycle = 0, avg_count = 0;
    int i;
    for (i = 0; i < max_threads; i++) {
        if (thread_state[i] == ACTIVE) {
            avg_cycle += cycle[i]._count;
            avg_count ++;
        }
    }
    if (avg_count == 0) {
        for (i = 0; i < max_threads; i++) {
            if (cycle[i]._count > avg_cycle) {
                avg_cycle = cycle[i]._count;
            }
        }
    }
    else {
        avg_cycle = avg_cycle / avg_count;
    }
    return avg_cycle;
}



// Handle non-memory instructions
void CoreManager::execNonMem(uint32_t ins_count_in, THREADID threadid)
{
    cycle[threadid]._count += cpi_nonmem * ins_count_in;
    ins_nonmem[threadid]._count += ins_count_in;
    barrier(threadid); 
}




// Handle a memory instruction
void CoreManager::execMem(void * addr, THREADID threadid, uint32_t size, BOOL mem_type)
{

    delay[threadid] = 0;
    msg_mem[threadid][mpi_pos[threadid]].mem_type = mem_type;
    msg_mem[threadid][mpi_pos[threadid]].addr_dmem = (uint64_t) addr;
    msg_mem[threadid][mpi_pos[threadid]].mem_size = size;
    msg_mem[threadid][mpi_pos[threadid]].timer = (int64_t)(cycle[threadid]._count);
    
    cycle[threadid]._count += 1;
    mpi_pos[threadid]++;
    if (mpi_pos[threadid] >= (max_msg_size + 1)) {
        msg_mem[threadid][0].mem_type = 0;
        msg_mem[threadid][0].addr_dmem = mpi_pos[threadid];
        msg_mem[threadid][0].mem_size = threadid;
        msg_mem[threadid][0].message_type = MEM_REQUESTS;
        MPI_Send(msg_mem[threadid], mpi_pos[threadid] * sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);

        MPI_Recv(&delay[threadid], 1, MPI_INT, 0, threadid, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if(delay[threadid] == -1) {
            cerr<<"An error occurs in cache system\n";
            MPI_Abort(MPI_COMM_WORLD, -1);
            PIN_ExitApplication(-1);
        }
        cycle[threadid]._count += delay[threadid];
        mpi_pos[threadid] = 1;
    }
    barrier(threadid);
}

// This routine is executed every time a thread starts.
void CoreManager::threadStart(THREADID threadid, CONTEXT *ctxt, int32_t flags, void *v)
{
    int i;
    PIN_GetLock(&thread_lock, threadid+1);

    if( threadid >= THREAD_MAX ) {
        cerr << "Error: the number of threads exceeds the limit!\n";
    }
    for(i = 0; i < max_threads; i++) {
        if(PIN_GetParentTid() == thread_map[i]) {
            cycle[threadid]._count = cycle[i]._count;
            break;
        }
    }
    if(i == max_threads) {
        cycle[threadid]._count = cycle[0]._count;
    }
    
    num_threads++; 
    max_threads++; 
    thread_state[threadid] = ACTIVE;
    thread_map[threadid] = PIN_GetTid();
    cout << "[PriME] Thread " << threadid << " begins at cycle "<< (uint64_t)cycle[threadid]._count << endl;

    PIN_ReleaseLock(&thread_lock);

    msg_mem[threadid][0].mem_size = threadid;
    msg_mem[threadid][0].message_type = NEW_THREAD;

    MPI_Send(&msg_mem[threadid][0], sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);
    MPI_Recv(&core[threadid], 1, MPI_INT, 0, threadid, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}


// This routine is executed every time a thread is destroyed.
void CoreManager::threadFini(THREADID threadid, const CONTEXT *ctxt, int32_t code, void *v)
{
    //Send out all remaining memory requests in the buffer
    if(mpi_pos[threadid] > 1) {
        msg_mem[threadid][0].mem_type = 0;
        msg_mem[threadid][0].addr_dmem = mpi_pos[threadid];
        msg_mem[threadid][0].mem_size = threadid;
        msg_mem[threadid][0].message_type = MEM_REQUESTS;

        MPI_Send(msg_mem[threadid], mpi_pos[threadid] * sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);
        MPI_Recv(&delay[threadid], 1, MPI_INT, 0, threadid, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if(delay[threadid] == -1) {
            cerr<<"An error occurs in cache system\n";
            MPI_Abort(MPI_COMM_WORLD, -1);
            PIN_ExitApplication(-1);
        }
        cycle[threadid]._count += delay[threadid];
        mpi_pos[threadid] = 1;
    }

    PIN_GetLock(&thread_lock, threadid+1);
    num_threads--;
    thread_state[threadid] = FINISH;
    cout << "[PriME] Thread " << threadid << " finishes at cycle "<< (uint64_t)cycle[threadid]._count <<endl;
    PIN_ReleaseLock(&thread_lock);

    msg_mem[threadid][0].mem_size = threadid;
    msg_mem[threadid][0].message_type = THREAD_FINISHING;

    MPI_Send(&msg_mem[threadid][0], sizeof(MsgMem), MPI_CHAR, 0, core[threadid], MPI_COMM_WORLD);

}


// Called before syscalls
void CoreManager::sysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, 
               ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, THREADID threadid)
{

    if((num == SYS_futex &&  ((arg1&FUTEX_CMD_MASK) == FUTEX_WAIT || (arg1&FUTEX_CMD_MASK) == FUTEX_LOCK_PI))
    ||  num == SYS_wait4
    ||  num == SYS_waitid) {
        PIN_GetLock(&thread_lock, threadid+1);

        if(thread_state[threadid] == ACTIVE) {
            thread_state[threadid] = SUSPEND;
            num_threads--;
            sync_syscall_count++;
        }
        PIN_ReleaseLock(&thread_lock);
    }
    syscall_count++;
}

// Called after syscalls
void CoreManager::sysAfter(ADDRINT ret, THREADID threadid)
{
    PIN_GetLock(&thread_lock, threadid+1);
    if (thread_state[threadid] == SUSPEND) {
        cycle[threadid]._count = getAvgCycle(threadid);
        thread_state[threadid] = ACTIVE;
        num_threads++;
    }
    else {
        cycle[threadid]._count += syscall_cost;
    }
    PIN_ReleaseLock(&thread_lock);
}



// Enter a syscall
void CoreManager::syscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
    sysBefore(PIN_GetContextReg(ctxt, REG_INST_PTR),
        PIN_GetSyscallNumber(ctxt, std),
        PIN_GetSyscallArgument(ctxt, std, 0),
        PIN_GetSyscallArgument(ctxt, std, 1),
        PIN_GetSyscallArgument(ctxt, std, 2),
        PIN_GetSyscallArgument(ctxt, std, 3),
        PIN_GetSyscallArgument(ctxt, std, 4),
        PIN_GetSyscallArgument(ctxt, std, 5),
        threadIndex);
}

// Exit a syscall
void CoreManager::syscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
    sysAfter(PIN_GetSyscallReturn(ctxt, std), threadIndex);
}

void CoreManager::getSimStartTime()
{
    clock_gettime(CLOCK_REALTIME, &sim_start_time);
}

void CoreManager::getSimFinishTime()
{
    clock_gettime(CLOCK_REALTIME, &sim_finish_time);
}



void CoreManager::report(ofstream *result)
{
    uint64_t total_cycles = 0, total_ins_counts = 0,  total_nonmem_ins_counts = 0;
    double total_cycles_nonmem = 0;
    total_cycles = (uint64_t)(cycle[0]._count);
    int i;
    for (i = 0; i < max_threads; i++) {
        total_ins_counts += ins_count[i]._count;
        total_nonmem_ins_counts += (uint64_t)ins_nonmem[i]._count;
        cout << "[PriME] Thread " <<i<< " runs " << (uint64_t)ins_count[i]._count <<" instructions\n";
    }
    total_cycles_nonmem = total_nonmem_ins_counts * cpi_nonmem;
    sim_time = (sim_finish_time.tv_sec - sim_start_time.tv_sec) + (double) (sim_finish_time.tv_nsec - sim_start_time.tv_nsec) / 1000000000.0; 
    *result << "*********************************************************\n";
    *result << "*                   PriME Simulator                     *\n";
    *result << "*********************************************************\n\n";
    *result << "Application "<<rank<<endl<<endl;
    *result << "Elapsed time "<< sim_time <<" seconds\n";
    *result << "Simulation speed: " << (total_ins_counts/1000000.0)/sim_time << " MIPS\n"; 
    *result << "Simulated slowdown : " << sim_time/(total_cycles/(freq*pow(10.0,9))) <<"X\n";
    *result << "Total Execution time = " << total_cycles/(freq*pow(10.0,9)) <<" seconds\n";
    *result << "System frequence = "<< freq <<" GHz\n";
    *result << "Simulation runs " << total_cycles <<" cycles\n";
    for(i = 0; i < max_threads; i++) {
        *result << "Thread " <<i<< " runs " << (uint64_t)cycle[i]._count <<" cycles\n";
        *result << "Thread " <<i<< " runs " << (uint64_t)ins_count[i]._count <<" instructions\n";
    }
    *result << "Simulation runs " << total_ins_counts <<" instructions\n\n";
    *result << "Simulation result:\n\n";
    *result << "The average IPC / core = "<< (double)total_ins_counts / total_cycles << "\n";
    *result << "Total memory instructions: "<< total_ins_counts - total_nonmem_ins_counts <<endl;
    *result << "Total memory access cycles: "<< (uint64_t)(total_cycles - total_cycles_nonmem) <<endl;
    *result << "Total non-memory instructions: "<< total_nonmem_ins_counts <<endl;
    *result << "Total non-memory access cycles: "<< (uint64_t)(total_cycles_nonmem) <<endl;
    *result << "System call count : " << syscall_count <<endl;
    *result << "Sync System call count : " << sync_syscall_count <<endl;

}

void CoreManager::finishSim(int32_t code, void *v)
{

    uint64_t total_ins_counts = 0;
    for (int i = 0; i < max_threads; i++) {
        total_ins_counts += ins_count[i]._count;
    }
    sim_time = (sim_finish_time.tv_sec - sim_start_time.tv_sec) + (double) (sim_finish_time.tv_nsec - sim_start_time.tv_nsec) / 1000000000.0; 
    cout << "*********************************************************\n";
    cout << "Application "<<rank<<" completes\n\n";
    cout << "Elapsed time : "<< sim_time <<" seconds\n";
    cout << "Simulation speed : " << (total_ins_counts/1000000.0)/sim_time << " MIPS\n"; 
    cout << "Simulated time : " << (uint64_t)(cycle[0]._count)/(freq*pow(10.0,9)) <<" seconds\n";
    cout << "Simulated slowdown : " << sim_time/(cycle[0]._count/(freq*pow(10.0,9))) <<"X\n";
    cout << "*********************************************************\n";
    PIN_MutexFini(&mutex);
    PIN_SemaphoreFini(&sem);

    msg_mem[0][0].mem_type = 0;
    msg_mem[0][0].addr_dmem = 0;
    msg_mem[0][0].mem_size = 0;
    msg_mem[0][0].message_type = PROCESS_FINISHING;

    MPI_Send(&msg_mem[0][0], sizeof(MsgMem), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    MPI_Recv(&num_procs, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (num_procs == 0) {
        for (int i = 0; i < num_recv_threads; i++) {
            msg_mem[0][0].message_type = PROGRAM_EXITING;
            MPI_Send(&msg_mem[0][0], sizeof(MsgMem), MPI_CHAR, 0, i, MPI_COMM_WORLD);
        }
    }
    MPI_Finalize();
}

CoreManager::~CoreManager()
{
    for(int i = 0; i < THREAD_MAX; i++) {
        delete [] msg_mem[i];
        delete [] send_req[i];
        delete [] recv_req[i];
    }
}       
