-------------------------
PriME Developer Guide
-------------------------

Author: Yaosheng Fu



PriME is a software simulator for manycore processors based on dynamic binary translation with Intel Pin tool. The src folder contains all the source code written in C++ for PriME. Different modules are implemented with different classes, and their relationship can be described as follows:


                                ---------------
                                | queue_model |
                                ---------------
                                      |
                -------               v                --------
                | bus |<------------------------------>| link | 
                -------                                --------
                   |                                       | 
                   v                                       v
               ---------   --------------   --------   -----------
               | cache |   | page_table |   | dram |   | network |
               ---------   --------------   --------   -----------
                   |             |             |           |
                   -----------------------------------------
                                       |
                                       v
                                  ----------                                                    
                                  | system |                                           
                                  ----------                                             
                                       |                                                                     
                                       v                                                             
      ----------------         ------------------              --------------                 ----------------
      | thread_sched |-------->| uncore_manager |<-------------| xml_parser |---------------->| core_manager |
      ---------------          ------------------              --------------                 ----------------   
                                       |                                                             |
                                       v                                                             v
                                  ---------                     OpenMPI                        -------------           =======
                                  | prime |<==================================================>| pin_prime |<----------| PIN | 
                                  ---------                                                    -------------           =======


There are two main modules: prime and pin_prime, which are running as two different processes communicating through OpenMPI. The prime module is responsible for simulating all uncore components including cache hierarchies, TLB/page tables, on-chip networks and DRAM, while the pin_prime module collects instruction stream from PIN at runtime and simulates the core. The system configuration file is inputted as an XML file which is parsed by the xml_parser module and fed into both processes.

The main function of the prime module is to handle memory requests from pin_prime by feeding them to the uncore manager module and send back the responses. The uncore manager manages the entire uncore system and also calls the thread_sched module to schedule workload threads to simulated cores. The system module simulates the entire uncore system, including data caches, directory caches, TLB caches, cache coherence protocols, on-chip networks and DRAM. There are two types of interconnects: bus and on-chip network. Buses are attached to caches at the next level as shared resources, while on-chip networks are based on network links between different nodes. Both interconnect types use queue_model adopted from the MIT Graphite simulator to simulate the contention delay. 

The pin_prime module interferes with the PIN tool to collect the instruction stream and system calls, and feeds that information to the core_manager module. The core_manager module simulates the core model and synchronize the local clocks across different cores with global barriers.

Each module is illustrated in detail below:


prime
-----
The prime module has a main function and msgHandler function. The code in the main function sets up the configuration for the OpenMPI environment, then generates multiple working threads executing the msgHandler function based on the number of receiving threads defined in the input XML file. The msgHandler keeps receiving MPI messages with a ping-pong buffer and sending back responses after processing. There are multiple message types such as messages to indicate the beginning or end of a process or thread, each of which is handled accordingly. The most important message type is MEM_REQUESTS which indicates a sequence of memory requests to the memory system. Those requests are fed into the uncore_manager module one by one and the total delay is sent back to the other process after all requests have been processed.


uncore_manager
--------------
The uncore manager module itself does not implement much functionality, but is primarily used to integrate other modules including thread_sched and system together. It also generates the main report for the prime process. 


thread_sched
------------
The thread_sched module is used to schedule application threads to simulated cores. It uses a map structure called CoreMap to map from a pair of program ID and thread ID into a core ID. Each core can only run a thread at any given time. Currently the scheduling algorithm is simply to iterating through all cores and schedule the thread to the first one that is available. If you would like to explore more advanced scheduling algorithm, it should be enough to just modify this module mostly. It might also be possible to schedule multiple threads on one core and running in a time-multiplexing manner, but it might require more changes in other modules as well.


system
------
The is the key module that implements the entire memory system for the PriME simulator. It implements cache hierarchies and the cache coherence protocol, as well as integrates other uncore components together. The cache hierarchies include three cache types: TLB cache, data cache and directory cache. TLB caches are accessed fist for TLB translation if necessary, then multiple levels of data caches are accessed recursively. If the system contains directory caches, they are accessed after data cache accesses. In order to save memory usage for manycore processors, data caches and directory caches are initialized on demand upon the first time being accessed. This is implemented with function init_caches and init_directories. Since there could be multiple threads accessing the same function at the same time, we have carefully designed mutex locks per data and directory cache to avoid conflict.

Since there could be multiple levels of data caches with different sharing patterns, traveling up and down across cache hierarchies are implemented with pointers. Each data cache has one parent cache which is the next level data cache it connects to and a number of children caches which are one or more data caches it connects in the previous level. For L1 caches, their children caches are NULL and for last-level data caches, their parent caches are NULL.

The system implements a MESI cache coherence protocol, which can be either snoopy-based (implemented with the function mesi_bus) or directory-based (implemented with the function mesi_directory). The snoopy-based coherence protocol uses buses as the interconnect, while the directory-based coherence protocol uses on-chip networks before the last-level directory cache instead. The last-level directory cache can be either also a data cache integrated with directories or just a directory cache without storing data. Those two cases are implemented with two separate functions accessSharedCache and accessDirectoryCache. The coherence protocol can also be configured as either full-map or limited pointers. For limited pointers, the line state will become B (broadcast) if the number of shares goes beyond the maximum limit and an invalidation of that line will be broadcast globally.

The homing algorithm implemented in function allocHomeId uses low-order bits interleaving by default, but it should be easy to modify to other algorithms.


cache
-----
There are three types of caches: TLB cache, data cache and directory cache. The main difference is that TLB caches do not require set-based locks for parallel accesses since they are private, while data and directory caches adopt bi-directional set-based locks for parallel accesses by multiple threads. The cache module implements a true LRU replacement algorithm in the lru function because each line keeps the accurate timestamp information for the most recent access with a 64-bit integer variable. Other replacement policies can be added by modifying this lru function.


page_table
----------
The page_table module is responsible for page translation from virtual pages to physical pages. The key data structure is called PageMap which maps a pair of program ID and virtual page number into a physical page number. The page mapping algorithm is implemented in the translate function. The current algorithm simply chooses the first available physical page with the smallest page number during page mapping. More advanced algorithms can be implemented by modifying the translate function.


dram
----
The dram model is very simple with fixed access latency. More advanced dram model can be implemented by modifying this module.


network
-------
The network module implements on-chip networks with either 2D-mesh or 3D-mesh topologies. The routing algorithm is X-Y routing or X-Y-Z routing accordingly. The network traversal latency can be decomposed into three parts: injection latency, link latency and router latency. The first part is independent of the communication distance and the other two are proportional to the communication distance. The link latency might also contain additional congestion delays which will be explained in detail in the link module. Other typologies and routing algorithms can be implemented by modifying this module.


bus
---
The bus is attached to the next-level cache and its access latency is already included in the cache access time. Therefore, in the bus module only contention delay is modeled with the analytical contention model (queue_model) adopted from the MIT Graphite simulator.


link
----
The link module implements an on-chip network link with both unit access delay and contention delay. The contention delay is also modeled with queue_model.


pin_prime
---------
The pin_prime module forms another process to simulate processors core utilizing Intel PIN. The main function sets up the environment for both OpenMPI and PIN. For PIN, it adds instrument functions to keep track of instruction trace, system calls, thread start/finish and application start/finish. The instruction trace function visits instruction trace based on per basic block. For each basic block, it inserts a call to insCount in order to count the total number of instructions. In addition, it walks through all instructions and inserts execMem function for memory instructions and execNonMem function for non-memory instructions. Those functions are passed into and handled in the core_manager module.


core_manager
------------
The core_manager implement the core model and is responsible for communicating with the prime process through OpenMPI. There is a 2D-array msg_mem which stores memory requests to the uncore process on a per-thread basis. Each thread can buffer a number of memory requests up to max_msg_size. Each thread also has a separate instruction counter ins_count[threadid]._count and local timer cycle[threadid]._count that are updated locally. Function exeNonMem updates the local timer for a given thread assuming all non-memory instruction take a constant number of cycles. For memory instructions, they are batched into the msg_mem buffer before sending them to the uncore process all at once in the exeMem function. When a new thread starts, its local timer is set to be the same as its parent thread. If the parent thread cannot be found, it is set to be the same as the first thread. A two-level periodic barrier is implemented to synchronize local timers across all simulated threads. The first-level barrier is used to synchronize threads within the same process and is implemented in the function barrier with Semaphores. The second-level barrier across all application processes is implemented through sending MPI messages to the uncore process for synchronization. Notice that PIN is only able to instrument user code, so if an application thread jumps into the kernel, there is no way to instrument kernel code. This might cause deadlocks if a thread is waiting for locks using by another thread through the system call while the other thread is waiting for the first thread to reach the barrier. In order to solve this issue, we detect all lock-related system calls and remove the corresponding thread from the barrier when it enters the system call and adds it back when it exits. The local timer of that thread is assigned to the average cycle of all other threads when it exits local-related system calls. All other system calls, a fixed amount of latency is added based on the input parameter syscall_cost.


xml_parser
----------
The xml_parse module is used to parse the XML input configuration file into an internal struct XmlSim. Additional input parameters can be enabled by adding similar code into this module. For optional parameters, make sure to remove the line item_count++ so that missing it will not cause parse errors.


common
------
This header file defines common data structures and variables that are shared by multiple other modules.

