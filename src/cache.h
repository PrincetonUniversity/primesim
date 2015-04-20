//===========================================================================
// cache.h 
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

#ifndef  CACHE_H
#define  CACHE_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <sstream>
#include <pthread.h> 
#include <set>
#include <map>
#include "xml_parser.h"
#include "bus.h"
#include "common.h"

typedef struct Addr
{
    uint64_t    index;
    uint64_t    tag;
    uint64_t    offset;
} Addr;



typedef enum State
{
    I     =   0, //invalid
    S     =   1, //shared
    E     =   2, //exclusive
    M     =   3, //modified
    V     =   4, //valid 
    B     =   5  //broadcast
} State;

typedef enum CacheType
{
    DATA_CACHE         = 0,
    DIRECTORY_CACHE    = 1,
    TLB_CACHE          = 2,
} CacheType;





typedef set<int> IntSet;

typedef struct Line
{
    char        state;
    int         id;
    int         set_num;
    int         way_num;
    uint64_t    tag;
    int64_t     timestamp;
    uint64_t    ppage_num;  //only for tlb cache
    IntSet      sharer_set; //only for directory cache
} Line;


typedef map<int, Line*> LineMap;

typedef struct InsMem
{
    char        mem_type; // 2 means writeback, 1 means write, 0 means read
    int         prog_id;
    int         thread_id;
    uint64_t    addr_dmem; 
} InsMem;


class Cache
{
    public:
        int         num_children;
        Cache*      parent;
        Cache**     child;
        Bus*        bus;
        void init(XmlCache* xml_cache, CacheType cache_type_in, int bus_latency, int page_size_in);
        Line* accessLine(InsMem* ins_mem);
        Line* directAccess(int set, int way, InsMem* ins_mem);
        Line* replaceLine(InsMem* ins_mem_old, InsMem* ins_mem);
        void flushAll();
        Line* flushLine(int set, int way, InsMem* ins_mem_old);
        Line* flushAddr(InsMem* ins_mem);
        Line* initSet(int index);
        Line* findSet(int index);
        void incInsCount();
        void incMissCount();
        void incEvictCount();
        void incWbCount();
        void lockUp(InsMem* ins_mem);
        void unlockUp(InsMem* ins_mem);
        void lockDown(InsMem* ins_mem);
        void unlockDown(InsMem* ins_mem);
        int  getAccessTime();
        uint64_t getSize();
        uint64_t getNumSets();
        uint64_t getNumWays();
        uint64_t getBlockSize();
        int getOffsetBits();
        int getIndexBits();
        uint64_t getInsCount();
        uint64_t getMissCount();
        uint64_t getEvictCount();
        uint64_t getWbCount();
        void addrParse(uint64_t addr_in, Addr* addr_out);
        void addrCompose(Addr* addr_in, uint64_t* addr_out);
        int lru(uint64_t index);
        void report(ofstream* result);
        ~Cache();
    private:
        int reverseBits(int num, int size);
        Line              **line;
        LineMap           line_map;
        pthread_mutex_t   *lock_up;
        pthread_mutex_t   *lock_down;
        CacheType         cache_type;
        int               access_time;
        uint64_t          ins_count;
        uint64_t          miss_count;
        uint64_t          evict_count;
        uint64_t          wb_count;
        uint64_t*         set_access_count;
        uint64_t*         set_evict_count;
        uint64_t          size;
        uint64_t          num_ways;
        uint64_t          block_size;
        uint64_t          num_sets;
        int               offset_bits;
        int               index_bits;  
        int               page_size;
        uint64_t          offset_mask;
        uint64_t          index_mask;
        Addr              addr_temp;

};

#endif //CACHE_H
