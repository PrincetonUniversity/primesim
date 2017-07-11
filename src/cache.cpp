//===========================================================================
// cache.cpp simulates a single cache with LRU replacement policy. 
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
#include <sys/time.h>
#include <time.h>

#include "cache.h"
#include "common.h"

using namespace std;




void Cache::init(XmlCache* xml_cache, CacheType cache_type_in, int bus_latency, int page_size_in, int level_in, int cache_id_in)
{
    ins_count = 0;;
    miss_count = 0;
    evict_count = 0;
    wb_count = 0;
    size = xml_cache->size;
    num_ways = xml_cache->num_ways;
    block_size = xml_cache->block_size;
    num_sets = size / (block_size*num_ways);
    access_time = xml_cache->access_time;
    cache_type = cache_type_in;
    page_size = page_size_in;
    level = level_in;
    cache_id = cache_id_in;

    if (cache_type == TLB_CACHE) {
        offset_bits = (int) (log2(page_size));
        offset_mask = (uint64_t)(page_size - 1);
    }
    else {
        offset_bits = (int) (log2(block_size));
        offset_mask = (uint64_t)(block_size - 1);
    }
    index_bits  = (int) (log2(num_sets));
    index_mask  = (uint64_t)(num_sets - 1);

    if (cache_type == DATA_CACHE || cache_type == DIRECTORY_CACHE) {
        line = new Line* [num_sets];
        lock_up = new pthread_mutex_t [num_sets];
        lock_down = new pthread_mutex_t [num_sets];
        for (uint64_t i = 0; i < num_sets; i++ ) {
            pthread_mutex_init(&lock_up[i], NULL);
            pthread_mutex_init(&lock_down[i], NULL);
            line[i] = new Line [num_ways];
            for (uint64_t j = 0; j < num_ways; j++ ) {
                line[i][j].state = I;
                line[i][j].id = 0;
                line[i][j].set_num = i;
                line[i][j].way_num = j;
                line[i][j].tag = 0;
                line[i][j].ppage_num = 0;
                line[i][j].timestamp = 0;
            }
        }
    }
    else if (cache_type == TLB_CACHE) {
        line = new Line* [num_sets];
        for (uint64_t i = 0; i < num_sets; i++ ) {
            line[i] = new Line [num_ways];
            for (uint64_t j = 0; j < num_ways; j++ ) {
                line[i][j].state = I;
                line[i][j].id = 0;
                line[i][j].set_num = i;
                line[i][j].way_num = j;
                line[i][j].tag = 0;
                line[i][j].ppage_num = 0;
                line[i][j].timestamp = 0;
            }
        }
    }
    else {
        cerr << "Error: Undefined cache type!\n";
    }
    if (xml_cache->share > 1) {
        bus = new Bus;
        bus->init(bus_latency);
    }
    else {
        bus = NULL;
    }
}



Line* Cache::findSet(int index)
{
    assert(index >= 0 && index < num_sets);
    return line[index];
}

int Cache::reverseBits(int num, int size)
{
    int reverse_num = 0;
    for (int i = 0; i < size; i++) {
        if((num & (1 << i))) {
           reverse_num |= 1 << ((size - 1) - i);  
        }
   }
   return reverse_num;
}

// This function parses each memory reference address into three separate parts
// according to the cache configuration: offset, index and tag.

void Cache::addrParse(uint64_t addr_in, Addr* addr_out)
{

    addr_out->offset =  addr_in & offset_mask;
    addr_out->tag    =  addr_in >> (offset_bits + index_bits);
    //hash
    addr_out->index  = (addr_in >> offset_bits) % num_sets;
}

// This function composes three separate parts: offset, index and tag into
// a memory address.

void Cache::addrCompose(Addr* addr_in, uint64_t* addr_out)
{
    //recover
    *addr_out = addr_in->offset 
              | (addr_in->index << offset_bits) 
              | (addr_in->tag << (offset_bits+index_bits));
}

// This function implements the LRU replacement policy in a cache set.

int Cache::lru(uint64_t index)
{
    uint64_t i; 
    int min_time = 0;
    Line* set_cur = findSet(index);
    assert(set_cur != NULL);
    for (i = 1; i < num_ways; i++) {
        if (set_cur[i].timestamp < set_cur[min_time].timestamp) {
            min_time = i;
        }
    }
    return min_time;
}


// This function returns a pointer to the cache line the instruction want to access,
// NULL is returned upon a cache miss.
Line* Cache::accessLine(InsMem* ins_mem)
{
    uint64_t i;
    Line* set_cur;
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    set_cur = findSet(addr_temp.index);
    assert(set_cur != NULL);
    for (i = 0; i < num_ways; i++) {
        if( (set_cur[i].id == ins_mem->prog_id)
        &&  (set_cur[i].tag == addr_temp.tag)
        &&   set_cur[i].state) {
            return &set_cur[i];
        }
    }
    return NULL;
}

// This function replaces the cache line the instruction want to access and returns
// a pointer to this line, the replaced content is copied to ins_mem_old.
Line* Cache::replaceLine(InsMem* ins_mem_old, InsMem* ins_mem)
{
    Addr addr_old;
    Addr addr_temp;
    int way_rp;
    uint64_t i, addr_dmem_old;
    Line* set_cur;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    set_cur = findSet(addr_temp.index);
    
    for (i = 0; i < num_ways; i++) {
        if (set_cur[i].state == I) {
           set_cur[i].id = ins_mem->prog_id; 
           set_cur[i].tag = addr_temp.tag;
           return &set_cur[i]; 
        }
    }
    way_rp = lru(addr_temp.index);
    addr_old.index = addr_temp.index;
    addr_old.tag = set_cur[way_rp].tag;
    addr_old.offset = 0;
    addrCompose(&addr_old, &addr_dmem_old);
    
    ins_mem_old->prog_id = set_cur[way_rp].id;
    ins_mem_old->addr_dmem = addr_dmem_old;

    set_cur[way_rp].id = ins_mem->prog_id; 
    set_cur[way_rp].tag = addr_temp.tag;


    return &set_cur[way_rp]; 
}

//Direct access to a specific line
Line* Cache::directAccess(int set, int way, InsMem* ins_mem)
{
    assert(set >= 0 && set < (int)num_sets);
    assert(way >= 0 && way < (int)num_ways);
    Line* set_cur;
    set_cur = findSet(set);
    if (set_cur == NULL) {
        return NULL;
    }
    else if (set_cur[way].state) {
        Addr addr;
        uint64_t addr_dmem;
        addr.index = set;
        addr.tag = set_cur[way].tag;
        addr.offset = 0;
        addrCompose(&addr, &addr_dmem);
        
        ins_mem->prog_id = set_cur[way].id;
        ins_mem->addr_dmem = addr_dmem;
        return &set_cur[way];
    }
    else {
        return NULL;
    }
}

void Cache::flushAll()
{
    for (uint64_t i = 0; i < num_sets; i++) {
        for (uint64_t j = 0; j < num_sets; j++) {
            line[i][j].state = I;
            line[i][j].id = 0;
            line[i][j].tag = 0;
            line[i][j].sharer_set.clear();
        }
    }
}

Line* Cache::flushLine(int set, int way, InsMem* ins_mem_old)
{
    assert(set >= 0 && set < (int)num_sets);
    assert(way >= 0 && way < (int)num_ways);
    Line* set_cur;
    set_cur = findSet(set);
    if (set_cur == NULL) {
        return NULL;
    }
    else if(set_cur[way].state) {
        set_cur[way].state = I;
        set_cur[way].id = 0;
        set_cur[way].tag = 0;
        set_cur[way].sharer_set.clear();
        Addr addr_old;
        uint64_t addr_dmem_old;
        addr_old.index = set;
        addr_old.tag = set_cur[way].tag;
        addr_old.offset = 0;
        addrCompose(&addr_old, &addr_dmem_old);
        
        ins_mem_old->prog_id = set_cur[way].id;
        ins_mem_old->addr_dmem = addr_dmem_old;

        return &set_cur[way];
    }
    else {
        return NULL;
    }
}

Line* Cache::flushAddr(InsMem* ins_mem)
{
    Line* cur_line;
    cur_line = accessLine(ins_mem);
    if (cur_line != NULL) {
        cur_line->state = I;
        cur_line->id = 0;
        cur_line->tag = 0;
        cur_line->sharer_set.clear();
        return cur_line;
    }
    else {
        return NULL;
    }
}

void Cache::lockUp(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    assert(addr_temp.index >= 0 && addr_temp.index < num_sets);
    pthread_mutex_lock(&lock_up[addr_temp.index]);
}

void Cache::unlockUp(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    assert(addr_temp.index >= 0 && addr_temp.index < num_sets);
    pthread_mutex_unlock(&lock_up[addr_temp.index]);
}

void Cache::lockDown(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    pthread_mutex_lock(&lock_down[addr_temp.index]);
}

void Cache::unlockDown(InsMem* ins_mem)
{
    Addr addr_temp;
    addrParse(ins_mem->addr_dmem, &addr_temp);
    pthread_mutex_unlock(&lock_down[addr_temp.index]);
}

int  Cache::getAccessTime()
{
    return access_time;
}

uint64_t Cache::getSize()
{
    return size;
}

uint64_t Cache::getNumSets()
{
    return num_sets;
}
        
uint64_t Cache::getNumWays()
{
    return num_ways;
}

uint64_t Cache::getBlockSize()
{
    return block_size;
}

int Cache::getOffsetBits()
{
    return offset_bits;
}
        
int Cache::getIndexBits()
{
    return index_bits;
}

void Cache::incInsCount()
{
    ins_count++;
}

void Cache::incMissCount()
{
    miss_count++;
}

void Cache::incWbCount()
{
    wb_count++;
}

void Cache::incEvictCount()
{
    evict_count++;
}

uint64_t Cache::getInsCount()
{
    return ins_count;
}

uint64_t Cache::getMissCount()
{
    return miss_count;
}

uint64_t Cache::getWbCount()
{
    return wb_count;
}

uint64_t Cache::getEvictCount()
{
    return evict_count;
}



void Cache::report(ofstream* result)
{
    *result << "=================================================================\n";
    *result << "Simulation results for "<< size << " Bytes " << num_ways
            << "-way set associative cache model:\n";
    *result << "The total # of memory instructions: " << ins_count << endl;
    *result << "The # of cache-missed instructions: " << miss_count << endl;
    *result << "The # of evicted instructions: " << evict_count << endl;
    *result << "The # of writeback instructions: " << wb_count << endl;
    *result << "The cache miss rate: " << 100 * (double)miss_count/ (double)ins_count << "%" << endl;
    *result << "=================================================================\n\n";
}

Cache::~Cache()
{
    if (cache_type == DATA_CACHE || cache_type == DIRECTORY_CACHE) {
        for (uint64_t i = 0; i < num_sets; i++ ) {
            pthread_mutex_destroy(&lock_up[i]);
            pthread_mutex_destroy(&lock_down[i]);
            delete [] line[i];
        }
        delete [] lock_up;
        delete [] lock_down;
        delete [] line;
    }
    else if (cache_type == TLB_CACHE) {
        for (uint64_t i = 0; i < num_sets; i++ ) {
            delete [] line[i];
        }
        delete [] line;
    }
    else {
        cerr << "Error: Undefined cache type!\n";
    }
    if (bus != NULL) {
        delete bus;
    }
}





