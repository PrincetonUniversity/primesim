//===========================================================================
// pin_prime.cpp utilizes PIN to feed instructions into the core model 
// at runtime
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

#include "pin_prime.h"

using namespace std;

// This function is called before every instruction is executed to calculate the instruction count
void PIN_FAST_ANALYSIS_CALL insCount(uint32_t ins_count, THREADID threadid)
{
    core_manager->insCount(ins_count, threadid);
}

// Handle non-memory instructions
void PIN_FAST_ANALYSIS_CALL execNonMem(uint32_t ins_count, THREADID threadid)
{
    core_manager->execNonMem(ins_count, threadid);
}


// Handle a memory instruction
void execMem(void * addr, THREADID threadid, uint32_t size, bool mem_type)
{
    core_manager->execMem(addr, threadid, size, mem_type);
}

// This routine is executed every time a thread starts.
void ThreadStart(THREADID threadid, CONTEXT *ctxt, int32_t flags, void *v)
{
    core_manager->threadStart(threadid, ctxt, flags, v);
}


// This routine is executed every time a thread is destroyed.
void ThreadFini(THREADID threadid, const CONTEXT *ctxt, int32_t code, void *v)
{
    core_manager->threadFini(threadid, ctxt, code, v);
}


// Inserted before syscalls
void SysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, 
               ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, THREADID threadid)
{
    core_manager->sysBefore(ip, num, arg0, arg1, arg2, arg3, arg4, arg5, threadid);
}

// Inserted after syscalls
void SysAfter(ADDRINT ret, THREADID threadid)
{
    core_manager->sysAfter(ret, threadid);
}


// Enter a syscall
void SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
    core_manager->syscallEntry(threadIndex, ctxt, std, v);
}

// Exit a syscall
void SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
    core_manager->syscallExit(threadIndex, ctxt, std, v);
}




// Pin calls this function every time a new basic block is encountered
void Trace(TRACE trace, void *v)
{
     uint32_t nonmem_count;

    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        // Insert a call to insCount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)insCount, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, BBL_NumIns(bbl), IARG_THREAD_ID, IARG_END);

        nonmem_count = 0;
        INS ins = BBL_InsHead(bbl);
        for (; INS_Valid(ins); ins = INS_Next(ins)) {

            // Instruments memory accesses using a predicated call, i.e.
            // the instrumentation is called iff the instruction will actually be executed.
            //
            // The IA-64 architecture has explicitly predicated instructions. 
            // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
            // prefixed instructions appear as predicated instructions in Pin.
            if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) { 
                uint32_t memOperands = INS_MemoryOperandCount(ins);

                // Iterate over each memory operand of the instruction.
                for (uint32_t memOp = 0; memOp < memOperands; memOp++) {
                    const uint32_t size = INS_MemoryOperandSize(ins, memOp);
                    // Note that in some architectures a single memory operand can be 
                    // both read and written (for instance incl (%eax) on IA-32)
                    // In that case we instrument it once for read and once for write.
                    if (INS_MemoryOperandIsRead(ins, memOp)) {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)execMem,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, RD,
                            IARG_END);

                    }
                    if (INS_MemoryOperandIsWritten(ins, memOp)) {
                        INS_InsertPredicatedCall(
                            ins, IPOINT_BEFORE, (AFUNPTR)execMem,
                            IARG_MEMORYOP_EA, memOp,
                            IARG_THREAD_ID,
                            IARG_UINT32, size,
                            IARG_BOOL, WR,
                            IARG_END);
                    }
                }
            }
            else {
                nonmem_count++;
            }
        }
        // Insert a call to execNonMem before every bbl, passing the number of nonmem instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)execNonMem, IARG_FAST_ANALYSIS_CALL, 
                       IARG_UINT32, nonmem_count, IARG_THREAD_ID, IARG_END);

	} // End Ins For

}



void Start(void *v)
{
}

void Fini(int32_t code, void *v)
{
    core_manager->getSimFinishTime();
    stringstream ss_rank;
    ss_rank << rank;
    result.open((KnobOutputFile.Value() + "_" + ss_rank.str()).c_str());
    //Report results
    core_manager->report(&result);
    result.close();
    core_manager->finishSim(code, v);
    delete core_manager;
}


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
int32_t Usage()
{
    PIN_ERROR( "This Pintool simulates a many-core cache system\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    //Link to the OpenMPI library
    dlopen(OPENMPI_PATH, RTLD_NOW | RTLD_GLOBAL);
    int prov, rc;
    rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &prov);
    if (rc != MPI_SUCCESS) {
        cerr << "Error starting MPI program. Terminating.\n";
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
    if(prov != MPI_THREAD_MULTIPLE) {
        cerr << "Provide level of thread supoort is not required: " << prov << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MPI_Comm_size(MPI_COMM_WORLD,&num_tasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    int rank_excl[1] = {0};
    // Extract the original group handle *
    MPI_Comm_group(MPI_COMM_WORLD, &orig_group);

    MPI_Group_excl(orig_group, 1, rank_excl, &new_group);

    // Create new new communicator and then perform collective communications 
    MPI_Comm_create(MPI_COMM_WORLD, new_group, &new_comm);
    MPI_Comm_rank(new_comm, &new_rank);
    
    if (PIN_Init(argc, argv)) return Usage();

    XmlParser xml_parser;
    XmlSim*   xml_sim;

    if(!xml_parser.parse(KnobConfigFile.Value().c_str())) {
		cerr<< "XML file parse error!\n";
        MPI_Abort(MPI_COMM_WORLD, -1);
		return -1;
    }
    xml_sim = xml_parser.getXmlSim();

    PIN_InitSymbols();

    core_manager = new CoreManager;
    //# of core processes equals num_tasks-1 because there is a uncore process
    core_manager->init(xml_sim, num_tasks-1, rank);
    core_manager->getSimStartTime();

 
   // Register Instruction to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);	
    PIN_AddThreadFiniFunction(ThreadFini, 0);	
    PIN_AddApplicationStartFunction(Start, 0);
    PIN_AddFiniFunction(Fini, 0);

    core_manager->startSim(&new_comm);

    PIN_StartProgram();
    
    return 0;
}
