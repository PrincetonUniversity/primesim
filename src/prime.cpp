//===========================================================================
// prime.cpp contains the  MPI interfaces to processes of core models
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


#include "prime.h"
#include "system.h"

using namespace std;



// Handle receiving MPI messages and send back responses
void *msgHandler(void *t)
{
    //Declare a ping-pong buffer to allow one buffer to receive data while another is being processed
    MsgMem msg_mem[2][max_msg_size + 1];
    InsMem ins_mem;
    MPI_Status local_status;
    list<int>::iterator prog_it;
    int delay = 0, core_id = 0, index_prev = 0;
    long rec_thread = (long)t;
    memset(msg_mem[0], 0, (max_msg_size + 1)*sizeof(MsgMem));
    memset(msg_mem[1], 0, (max_msg_size + 1)*sizeof(MsgMem));
    memset(&ins_mem, 0, sizeof(ins_mem));
    while (1) {
        MPI_Recv(msg_mem[index_prev], (max_msg_size + 1)*sizeof(MsgMem), MPI_CHAR, MPI_ANY_SOURCE, (int)rec_thread , MPI_COMM_WORLD, &local_status);
        //Receive a msg indicating a new process
        if (msg_mem[index_prev][0].message_type == PROCESS_STARTING) {
            pthread_mutex_lock (&mutex);
            cout<<"[PriME] Process "<<local_status.MPI_SOURCE<<" begins"<<endl;
            prog_list.push_back(local_status.MPI_SOURCE);
            prog_list.unique();
            pthread_mutex_unlock (&mutex);
        }
        //Receive a msg indicating a process has finished
        else if (msg_mem[index_prev][0].message_type == PROCESS_FINISHING) {
            pthread_mutex_lock (&mutex);
            cout<<"[PriME] Process "<<local_status.MPI_SOURCE<<" finishes"<<endl;
            prog_list.remove(local_status.MPI_SOURCE);
            delay = prog_list.size();
            MPI_Send(&delay, 1, MPI_INT, local_status.MPI_SOURCE , 0, MPI_COMM_WORLD);
            if (prog_count >= prog_list.size()) {
                for(prog_it = prog_list.begin(); prog_it != prog_list.end(); ++prog_it) {
                    MPI_Send(&delay, 1, MPI_INT, *prog_it , 0, MPI_COMM_WORLD);
                }
                prog_count = 0; 
            }
            pthread_mutex_unlock (&mutex);
        }
        //Receive a msg for inter-process barriers
        else if (msg_mem[index_prev][0].message_type == INTER_PROCESS_BARRIERS) {
            pthread_mutex_lock (&mutex);
            prog_count++;
            if (prog_count >= prog_list.size()) {
                delay = prog_list.size();
                for (prog_it = prog_list.begin(); prog_it != prog_list.end(); ++prog_it) {
                    MPI_Send(&delay, 1, MPI_INT, *prog_it , 0, MPI_COMM_WORLD);
                }
                prog_count = 0; 
            } 
           pthread_mutex_unlock (&mutex);
        }
        //Receive a msg indicating a new thread
        else if (msg_mem[index_prev][0].message_type == NEW_THREAD) {
            pthread_mutex_lock (&mutex);
            core_id = uncore_manager.allocCore(local_status.MPI_SOURCE, msg_mem[index_prev][0].mem_size);   
            if (core_id == -1) {
                cerr<< "Not enough cores for process "<<local_status.MPI_SOURCE<<" thread "<<msg_mem[index_prev][0].mem_size<<endl;
                pthread_mutex_unlock (&mutex);
                uncore_manager.report(&result);
                result.close();
                MPI_Abort(MPI_COMM_WORLD, -1);
                pthread_exit(NULL);
            } 
            else {
                pthread_mutex_unlock (&mutex);
                delay = core_id % num_threads;
                MPI_Send(&delay, 1, MPI_INT, local_status.MPI_SOURCE, msg_mem[index_prev][0].mem_size, MPI_COMM_WORLD);
                
            }
        }
        //Receive a msg indicating a thread is finishing
        else if (msg_mem[index_prev][0].message_type == THREAD_FINISHING) {
            pthread_mutex_lock (&mutex);
            uncore_manager.deallocCore(local_status.MPI_SOURCE, msg_mem[index_prev][0].mem_size);   
            pthread_mutex_unlock (&mutex);
         }
        //Receive a msg to terminate the execution
        else if (msg_mem[index_prev][0].message_type == PROGRAM_EXITING) {
            break;
        }
        //Receive a msg of memory requests
        else {
            int thread_id = (int)msg_mem[index_prev][0].mem_size;
            int msg_len = msg_mem[index_prev][0].addr_dmem;
            delay = 0;
            core_id = uncore_manager.getCoreId(local_status.MPI_SOURCE, thread_id);
            ins_mem.prog_id = local_status.MPI_SOURCE;
            for(int i = 1; i < msg_len; i++) {
                ins_mem.mem_type = msg_mem[index_prev][i].mem_type;
                ins_mem.addr_dmem = msg_mem[index_prev][i].addr_dmem;
                delay += uncore_manager.uncore_access(core_id, &ins_mem, msg_mem[index_prev][i].timer + delay) - 1;
                if (delay < 0) {
                    cerr<<"Error: negative delay: "<<core_id<<" "<<ins_mem.prog_id<<" "<<thread_id<<" "
                        <<ins_mem.mem_type<<" "<<ins_mem.addr_dmem<<endl;
                    pthread_exit(NULL);
                }
            }
            MPI_Send(&delay, 1, MPI_INT, local_status.MPI_SOURCE , thread_id, MPI_COMM_WORLD);
        }
    }

    cout << "[PriME] Thread " << local_status.MPI_SOURCE << " finish" << endl;
    pthread_exit(NULL);
}



int main(int argc, char *argv[])
{
    //For debugging MPI
    /*
    {
        volatile int i = 0;
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        printf("PID %d on %s ready for attach\n", getpid(), hostname);
        fflush(stdout);
        while (0 == i)
            sleep(5);
    }
    */
    int rc, prov = 0;
    rc = MPI_Init_thread(&argc,&argv, MPI_THREAD_MULTIPLE, &prov);
    if (rc != MPI_SUCCESS) {
        cerr << "Error starting MPI program. Terminating." << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
    if(prov != MPI_THREAD_MULTIPLE) {
        cerr << "Provide level of thread supoort is not required: " << prov << endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
    }

    MPI_Comm   new_comm;
    MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_create(MPI_COMM_WORLD, MPI_GROUP_EMPTY, &new_comm);

    XmlParser xml_parser;
    XmlSim*   xml_sim;
    long t;
    pthread_t thread[numtasks-1];
    pthread_attr_t attr;
    void *status;

    if(argc != 3) {
        cerr<<"usage: "<< argv[0] <<" config_file output_file\n";
        cerr<<"argc is:" << argc <<endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
        return -1;
    }

    if( !xml_parser.parse(argv[1]) ) {
		cerr<< "XML file parse error!\n";
        MPI_Abort(MPI_COMM_WORLD, -1);
		return -1;
    }
    xml_sim = xml_parser.getXmlSim();
    max_msg_size = xml_sim->max_msg_size;
    num_threads = xml_sim->num_recv_threads;
    uncore_manager.init(xml_sim);
    pthread_mutex_init(&mutex, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    stringstream ss_rank;
    ss_rank << rank;
    result.open((string(argv[2])+ "_" + ss_rank.str()).c_str());

    uncore_manager.getSimStartTime();
    for(t = 0; t < num_threads; t++) {
        cout << "[PriME] Main: creating thread " << t <<endl;
        rc = pthread_create(&thread[t], &attr, msgHandler, (void *)t); 
        if (rc) {
            cerr << "Error return code from pthread_create(): " << rc << endl;
            MPI_Finalize();
            pthread_mutex_destroy(&mutex);
            exit(-1);
        }
    }

    // Free attribute and wait for the other threads 
    pthread_attr_destroy(&attr);
    for(t = 0; t < num_threads; t++) {
        rc = pthread_join(thread[t], &status);
        if (rc) {
            cerr << "Error return code from pthread_join(): " << rc << endl;
            MPI_Finalize();
            pthread_mutex_destroy(&mutex);
            exit(-1);
       }
       cout << "[PriME] Main: completed join with thread " << t << " having a status of "<< status << endl;
    }
      
    uncore_manager.getSimFinishTime();
    uncore_manager.report(&result);
    result.close();
    MPI_Finalize();
    pthread_mutex_destroy(&mutex);
    pthread_exit(NULL);
}
