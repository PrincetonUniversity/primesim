//===========================================================================
// system.cpp simulates inclusive multi-level cache system with NoC and memory
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

#include "system.h"
#include "common.h"




void System::init(XmlSys* xml_sys_in)
{
    int i,j;
    xml_sys = xml_sys_in;
    sys_type = xml_sys->sys_type;
    protocol_type = xml_sys->protocol_type;
    num_cores = xml_sys->num_cores;
    cpi_nonmem = xml_sys->cpi_nonmem;
    dram_access_time = xml_sys->dram_access_time;
    page_size = xml_sys->page_size;
    tlb_enable = xml_sys->tlb_enable;
    shared_llc = xml_sys->shared_llc;
    num_levels = xml_sys->num_levels;
    verbose_report = xml_sys->verbose_report;
    total_bus_contention = 0;
    total_num_broadcast = 0;
    max_num_sharers = xml_sys->max_num_sharers;
    directory_cache = NULL;
    tlb_cache = NULL;

    hit_flag = new bool [num_cores];
    delay = new int [num_cores];
    for (i=0; i<num_cores; i++) {
        hit_flag[i] = false;
        delay[i] = 0;
    }
    cache_level = new CacheLevel [num_levels];
    for (i=0; i<num_levels; i++) {
        cache_level[i].level = xml_sys->cache[i].level;
        cache_level[i].share = xml_sys->cache[i].share;
        cache_level[i].num_caches = (int)ceil((double)num_cores / cache_level[i].share);
        cache_level[i].access_time = xml_sys->cache[i].access_time;
        cache_level[i].size = xml_sys->cache[i].size;
        cache_level[i].block_size = xml_sys->cache[i].block_size;
        cache_level[i].num_ways = xml_sys->cache[i].num_ways;
        cache_level[i].ins_count = 0;
        cache_level[i].miss_count = 0;
        cache_level[i].miss_rate = 0;
        cache_level[i].lock_time = 0;
    }
    cache = new Cache** [num_levels];
    for (i=0; i<num_levels; i++) {
        cache[i] = new Cache* [cache_level[i].num_caches];
        for (j=0; j<cache_level[i].num_caches; j++) {
            cache[i][j] = NULL;
        }
    }

    network.init(cache_level[num_levels-1].num_caches, &(xml_sys->network));
    home_stat = new int [network.getNumNodes()];
    for (i=0; i<network.getNumNodes(); i++) {
        home_stat[i] = 0;
    }
    if (xml_sys->directory_cache.size > 0) {
        directory_cache = new Cache* [network.getNumNodes()];
        for (int i = 0; i < network.getNumNodes(); i++) {
            directory_cache[i] = NULL;
        }
    }

    if (tlb_enable && xml_sys->tlb_cache.size > 0) {
        tlb_cache = new Cache [num_cores];
        for (int i = 0; i < num_cores; i++) {
            tlb_cache[i].init(&(xml_sys->tlb_cache), TLB_CACHE, 0, page_size);
        }
    }
    page_table.init(page_size, xml_sys->page_miss_delay);
    dram.init(dram_access_time);
}

// This function models an access to memory system and returns the delay.
int System::access(int core_id, InsMem* ins_mem, int64_t timer)
{
    int cache_id;
    if (core_id >= num_cores) {
        cerr << "Not enough cores!\n";
        return -1;
    }

    hit_flag[core_id] = false;
    delay[core_id] = 0;
    cache_id = core_id / cache_level[0].share;
 
    if (tlb_enable) {
        delay[core_id] = tlb_translate(ins_mem, core_id, timer);
    }
 
    if (sys_type == DIRECTORY) {
        mesi_directory(cache[0][core_id], 0, cache_id, core_id, ins_mem, timer + delay[core_id]);
    }
    else {
        mesi_bus(cache[0][core_id], 0, cache_id, core_id, ins_mem, timer + delay[core_id]);
    }

    return delay[core_id];
}


//Initialize caches on demand
Cache* System::init_caches(int level, int cache_id)
{
    int k;
    if (cache[level][cache_id] == NULL) {
        cache[level][cache_id] = new Cache();
        cache[level][cache_id]->init(&(xml_sys->cache[level]), DATA_CACHE, xml_sys->bus_latency, page_size);
        if (level == 0) {
            cache[level][cache_id]->num_children = 0;
            cache[level][cache_id]->child = NULL;
        }
        else {
            cache[level][cache_id]->num_children = cache_level[level].share/cache_level[level-1].share;
            cache[level][cache_id]->child = new Cache* [cache[level][cache_id]->num_children];
            for (k=0; k<cache[level][cache_id]->num_children; k++) {
                cache[level][cache_id]->child[k] = cache[level-1][cache_id*cache[level][cache_id]->num_children+k];
            }
        }

        if (level == num_levels-1) {
            cache[level][cache_id]->parent = NULL;
        }
        else {
            init_caches(level+1, cache_id*cache_level[level].share/cache_level[level+1].share);
            cache[level][cache_id]->parent = cache[level+1][cache_id*cache_level[level].share/cache_level[level+1].share];
        }
    }
    else {
        for (k=0; k<cache[level][cache_id]->num_children; k++) {
            cache[level][cache_id]->child[k] = cache[level-1][cache_id*cache[level][cache_id]->num_children+k];
        }
    }
    return cache[level][cache_id];
}

void System::init_directories(int home_id)
{
    if (xml_sys->directory_cache.size > 0) {
        directory_cache[home_id] = new Cache();
        directory_cache[home_id]->init(&(xml_sys->directory_cache), DIRECTORY_CACHE, xml_sys->bus_latency, page_size);
    }
}




// This function models an acess to a multi-level cache sytem with bus-based
// MESI coherence protocol
char System::mesi_bus(Cache* cache_cur, int level, int cache_id, int core_id, InsMem* ins_mem, int64_t timer)
{
    int i, shared_line;
    int delay_bus;
    Line*  line_cur;
    Line*  line_temp;
    InsMem ins_mem_old;
    
    if (cache_cur == NULL) {
        cache_cur = init_caches(level, cache_id); 
    }
    if (cache_cur->bus != NULL) {
        delay_bus = cache_cur->bus->access(timer+delay[core_id]);
        total_bus_contention += delay_bus;
        delay[core_id] += delay_bus;
    }
    delay[core_id] += cache_level[level].access_time;
    if(!hit_flag[core_id]) {
        cache_cur->incInsCount();
    }

    cache_cur->lockUp(ins_mem);
    line_cur = cache_cur->accessLine(ins_mem);
    //Cache hit
    if (line_cur != NULL) {
        line_cur->timestamp = timer;
        hit_flag[core_id] = true; 
        //Write
        if (ins_mem->mem_type == WR) {
           if (level != num_levels-1) {
               if (line_cur->state != M) {
                   line_cur->state = I;
                   line_cur->state = mesi_bus(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer+delay[core_id]);
               }
           }
           else {
               if (line_cur->state == S) {
                   shared_line = 0;
                   for (i=0; i < cache_level[num_levels-1].num_caches; i++) {
                       if (i != cache_id) {
                           line_temp = cache[num_levels-1][i]->accessLine(ins_mem);
                           if(line_temp != NULL) {
                               shared_line = 1;
                               line_temp->state = I;
                               inval_children(cache[num_levels-1][i], ins_mem);
                           }
                       }
                   }
               }
               line_cur->state = M;
           }
           inval_children(cache_cur, ins_mem);
           line_cur->timestamp = timer;
           cache_cur->unlockUp(ins_mem);
           return M;
        }
        //Read
        else {
            if (line_cur->state != S) {
                share_children(cache_cur, ins_mem);
            }
            line_cur->timestamp = timer;
            cache_cur->unlockUp(ins_mem);
            return S;
        }
    }
    //Cache miss
    else
    {
        //Evict old line
        line_cur = cache_cur->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            inval_children(cache_cur, &ins_mem_old);
        }
        line_cur->timestamp = timer;
        if (level != num_levels-1) {
            line_cur->state = mesi_bus(cache_cur->parent, level+1, 
                                   cache_id*cache_level[level].share/cache_level[level+1].share, core_id, 
                                   ins_mem, timer+delay[core_id]);
        }
        else {
            //Write miss
            if (ins_mem->mem_type == WR) {
                shared_line = 0;
                for (i=0; i < cache_level[num_levels-1].num_caches; i++) {
                    if (i != cache_id) {
                        line_temp = cache[num_levels-1][i]->accessLine(ins_mem);
                        if (line_temp != NULL) {
                            shared_line = 1;
                            inval_children(cache[num_levels-1][i], ins_mem);
                            if (line_temp->state == M || line_temp->state == E) {
                                line_temp->state = I;
                                break;
                            }
                            else {
                                line_temp->state = I;
                            }
                         }
                    }
                 }
                 line_cur->state = M;
             }   
             //Read miss
             else {
                 shared_line = 0;
                 for (i=0; i < cache_level[num_levels-1].num_caches; i++) {
                    if (i != cache_id) {
                         line_temp = cache[num_levels-1][i]->accessLine(ins_mem);
                         if(line_temp != NULL)
                         {
                             shared_line = 1;
                             share_children(cache[num_levels-1][i], ins_mem);
                             if (line_temp->state == M || line_temp->state == E) {
                                 line_temp->state = S;
                                 break;
                             }
                             else {
                                 line_temp->state = S;
                             }
                         }
                    }
                 }
                 if (shared_line) {
                     line_cur->state = S;
                 }
                 else {
                     line_cur->state = E;
                 }
             }
             delay[core_id] += dram.access(ins_mem);                    
        }  
        cache_cur->incMissCount();        
        cache_cur->unlockUp(ins_mem);
        return line_cur->state; 
    }
}

// This function models an acess to a multi-level cache sytem with directory-
// based MESI coherence protocol
char System::mesi_directory(Cache* cache_cur, int level, int cache_id, int core_id, InsMem* ins_mem, int64_t timer)
{
    int id_home, delay_bus;
    char state_tmp;
    Line*  line_cur;
    InsMem ins_mem_old;
  
    if (cache_cur == NULL) {
        cache_cur = init_caches(level, cache_id); 
    }

    assert(cache_cur != NULL);
    if (cache_cur->bus != NULL) {
        delay_bus = cache_cur->bus->access(timer+delay[core_id]);
        total_bus_contention += delay_bus;
        delay[core_id] += delay_bus;
    }

    if (!hit_flag[core_id]) {
        cache_cur->incInsCount();
    }

    cache_cur->lockUp(ins_mem);
    delay[core_id] += cache_level[level].access_time;
    line_cur = cache_cur->accessLine(ins_mem);
    //Cache hit
    if (line_cur != NULL) {
        line_cur->timestamp = timer + delay[core_id];
        hit_flag[core_id] = true; 
        //Write
        if (ins_mem->mem_type == WR) {
           if (level != num_levels-1) {
               if (line_cur->state != M) {
                   line_cur->state = I;
                   line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                          cache_id*cache_level[level].share/cache_level[level+1].share, 
                                          core_id, ins_mem, timer + delay[core_id]);
               }
           }
           else {
               if (line_cur->state == S) {
                   id_home = getHomeId(ins_mem);
                   delay[core_id] += network.transmit(cache_id, id_home, 0, timer+delay[core_id]);
                   if (shared_llc) {
                       delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp);
                    
                   }    
                   else {
                       delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp);
                   }
                   delay[core_id] += network.transmit(id_home, cache_id, 0, timer+delay[core_id]);

               }
               line_cur->state = M;
           }
           cache_cur->unlockUp(ins_mem);
           return M;
        }
        //Read
        else {
            if (line_cur->state != S) {
                delay[core_id] += share_children(cache_cur, ins_mem);
            }
            cache_cur->unlockUp(ins_mem);
            return S;
        }
    }
    //Cache miss
    else {
        line_cur = cache_cur->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            cache_cur->incEvictCount();
            delay[core_id] += inval_children(cache_cur, &ins_mem_old);
            if(line_cur->state == M || line_cur->state == E) {
                cache_cur->incWbCount();
                if (level == num_levels-1) {
                    id_home = getHomeId(&ins_mem_old);
                    ins_mem_old.mem_type = WB; 
                    network.transmit(cache_id, id_home, cache_level[num_levels-1].block_size,  timer+delay[core_id]);
                    if (shared_llc) {
                        accessSharedCache(cache_id, id_home, &ins_mem_old, timer+delay[core_id], &state_tmp);
                    }
                    else {
                        accessDirectoryCache(cache_id, id_home, &ins_mem_old, timer+delay[core_id], &state_tmp);
                    }
                }
            }
        }
        line_cur->timestamp = timer + delay[core_id];
        if (level != num_levels-1) {
            line_cur->state = mesi_directory(cache_cur->parent, level+1, 
                                   cache_id*cache_level[level].share/cache_level[level+1].share, core_id, 
                                   ins_mem, timer);
        }
        else {
            id_home = getHomeId(ins_mem);
            delay[core_id] += network.transmit(cache_id, id_home, 0,  timer+delay[core_id]);
            if (shared_llc) {
                delay[core_id] += accessSharedCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp);
            }
            else {
                delay[core_id] += accessDirectoryCache(cache_id, id_home, ins_mem, timer+delay[core_id], &state_tmp);
            }
            line_cur->state = state_tmp;
            delay[core_id] += network.transmit(id_home, cache_id, cache_level[num_levels-1].block_size, timer+delay[core_id]);
        }  
        cache_cur->incMissCount();        
        cache_cur->unlockUp(ins_mem);
        return line_cur->state; 
    }
}



// This function propagates down shared state

int System::share(Cache* cache_cur, InsMem* ins_mem)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;
    Line*  line_cur;

    if (cache_cur != NULL) {
        cache_cur->lockDown(ins_mem);
        line_cur = cache_cur->accessLine(ins_mem);
        delay += cache_cur->getAccessTime();
        if (line_cur != NULL && (line_cur->state == M || line_cur->state == E)) {
            line_cur->state = S;
            for (i=0; i<cache_cur->num_children; i++) {
                delay_tmp = share(cache_cur->child[i], ins_mem);
                if (delay_tmp > delay_max) {
                    delay_max = delay_tmp;
                }
            }
            delay += delay_max;
        }
        cache_cur->unlockDown(ins_mem);
    }
    return delay;
}

// This function propagates down shared state starting from childern nodes

int System::share_children(Cache* cache_cur, InsMem* ins_mem)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;

    if (cache_cur != NULL) {
        for (i=0; i<cache_cur->num_children; i++) {
            delay_tmp = share(cache_cur->child[i], ins_mem);
            if (delay_tmp > delay_max) {
                delay_max = delay_tmp;
            }
        }
        delay += delay_max;
    }
    return delay;
}



// This function propagates down invalidate state
int System::inval(Cache* cache_cur, InsMem* ins_mem)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;
    Line* line_cur;

    if (cache_cur != NULL) {
        cache_cur->lockDown(ins_mem);
        line_cur = cache_cur->accessLine(ins_mem);
        delay += cache_cur->getAccessTime();
        if (line_cur != NULL) {
            line_cur->state = I;
            for (i=0; i<cache_cur->num_children; i++) {
                delay_tmp = inval(cache_cur->child[i], ins_mem);
                if (delay_tmp > delay_max) {
                    delay_max = delay_tmp;
                }
            }
            delay += delay_max;
        }
        cache_cur->unlockDown(ins_mem);
    }
    return delay;
}

// This function propagates down invalidate state starting from children nodes
int System::inval_children(Cache* cache_cur, InsMem* ins_mem)
{
    int i, delay = 0, delay_tmp = 0, delay_max = 0;

    if (cache_cur != NULL) {
        for (i=0; i<cache_cur->num_children; i++) {
            delay_tmp = inval(cache_cur->child[i], ins_mem);
            if (delay_tmp > delay_max) {
                delay_max = delay_tmp;
            }
        }
        delay += delay_max;
    }
    return delay;
}



//Access inclusive directory cache without backup directory
int System::accessDirectoryCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state)
{

    if (directory_cache[home_id] == NULL) {
        init_directories(home_id); 
    }
    int delay = 0, delay_temp =0, delay_pipe =0, delay_max = 0;
    InsMem ins_mem_old;
    IntSet::iterator pos;
    Line* line_cur;
    home_stat[home_id] = 1;
    directory_cache[home_id]->lockUp(ins_mem);
    line_cur = directory_cache[home_id]->accessLine(ins_mem);
    directory_cache[home_id]->incInsCount();
    delay += directory_cache[home_id]->getAccessTime();
    //Directory cache miss
    if ((line_cur == NULL) && (ins_mem->mem_type != WB)) {
        line_cur = directory_cache[home_id]->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            directory_cache[home_id]->incEvictCount();
            if (line_cur->state == M || line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                dram.access(&ins_mem_old);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], &ins_mem_old);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }   
            //Broadcast
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for (int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], &ins_mem_old);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 

        }
        if (ins_mem->mem_type == WR) {
            line_cur->state = M;
        }
        else {
            line_cur->state = E;
        }

        directory_cache[home_id]->incMissCount();
        line_cur->sharer_set.clear();
        line_cur->sharer_set.insert(cache_id);
        delay += dram.access(ins_mem);
    }  
    //Directoy cache hit 
    else {
        if (ins_mem->mem_type == WR) {
            if (line_cur->state == M || line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], ins_mem);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
                delay += dram.access(ins_mem);
            }
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for (int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], ins_mem);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
                delay += dram.access(ins_mem);
            } 
            line_cur->state = M;
            line_cur->sharer_set.clear();
            line_cur->sharer_set.insert(cache_id);
        }
        else if (ins_mem->mem_type == RD) {
             if (line_cur->state == M || line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += share(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                line_cur->state = S;
            }
            else if (line_cur->state == S) {
                delay += dram.access(ins_mem);
                if ((protocol_type == LIMITED_PTR) && ((int)line_cur->sharer_set.size() >= max_num_sharers)) {
                    line_cur->state = B;
                }
                else {
                    line_cur->state = S;
                }

            } 
            else if (line_cur->state == B) {
                delay += dram.access(ins_mem);
                line_cur->state = B;
            } 
            line_cur->sharer_set.insert(cache_id);
        } 
        else {
            line_cur->state = I;
            line_cur->sharer_set.clear();
            dram.access(ins_mem);
        }
    }
    if(line_cur->state == B) {
        (*state) = S;
    }
    else {
        (*state) = line_cur->state;
    }
    line_cur->timestamp = timer;
    directory_cache[home_id]->unlockUp(ins_mem);
    return delay;
}

//Access distributed shared LLC with embedded directory cache
int System::accessSharedCache(int cache_id, int home_id, InsMem* ins_mem, int64_t timer, char* state)
{
    if (directory_cache[home_id] == NULL) {
        init_directories(home_id); 
    }
    int delay = 0, delay_temp =0, delay_pipe =0, delay_max = 0;
    InsMem ins_mem_old;
    IntSet::iterator pos;
    Line* line_cur;
    home_stat[home_id] = 1;
    directory_cache[home_id]->lockUp(ins_mem);
    line_cur = directory_cache[home_id]->accessLine(ins_mem);
    directory_cache[home_id]->incInsCount();
    delay += directory_cache[home_id]->getAccessTime();
    //Shard llc miss
    if ((line_cur == NULL) && (ins_mem->mem_type != WB)) {
        line_cur = directory_cache[home_id]->replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            directory_cache[home_id]->incEvictCount();
            if (line_cur->state == M) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                dram.access(&ins_mem_old);
            }
            else if (line_cur->state == E) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], &ins_mem_old);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id , 0, timer+delay);
                dram.access(&ins_mem_old);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], &ins_mem_old);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }
            //Broadcast
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for(int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], &ins_mem_old);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if (delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 
        }
        if(ins_mem->mem_type == WR) {
            line_cur->state = M;
        }
        else {
            line_cur->state = E;
        }

        directory_cache[home_id]->incMissCount();
        line_cur->sharer_set.clear();
        line_cur->sharer_set.insert(cache_id);
        delay += dram.access(ins_mem);
    }   
    //Shard llc hit
    else {
        if (ins_mem->mem_type == WR) {
            if ((line_cur->state == M) || (line_cur->state == E)) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += inval(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
            }
            else if (line_cur->state == S) {
                delay_pipe = 0;
                delay_max = 0;
                for (pos = line_cur->sharer_set.begin(); pos != line_cur->sharer_set.end(); ++pos){
                    delay_temp = delay_pipe; 
                    delay_temp += network.transmit(home_id, (*pos), 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][(*pos)], ins_mem);
                    delay_temp += network.transmit((*pos), home_id, 0, timer+delay+delay_temp);
                              
                    if(delay_temp > delay_max) {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            }
            else if (line_cur->state == B) {
                total_num_broadcast++;
                for(int i = 0; i < num_cores; i++) { 
                    delay_temp = delay_pipe;
                    delay_temp += network.transmit(home_id, i, 0, timer+delay+delay_temp);
                    delay_temp += inval(cache[num_levels-1][i], ins_mem);
                    delay_temp += network.transmit(i, home_id, 0, timer+delay+delay_temp);
                    if(delay_temp > delay_max) 
                    {
                        delay_max = delay_temp;
                    }
                    delay_pipe += network.getHeaderFlits();
                }
                delay += delay_max;
            } 
            else if(line_cur->state == V) {
            }
            line_cur->state = M;
            line_cur->sharer_set.clear();
            line_cur->sharer_set.insert(cache_id);
        }
        else if (ins_mem->mem_type == RD) {
             if ((line_cur->state == M) || (line_cur->state == E)) {
                delay += network.transmit(home_id, *line_cur->sharer_set.begin(), 0, timer+delay);
                delay += share(cache[num_levels-1][(*line_cur->sharer_set.begin())], ins_mem);
                delay += network.transmit(*line_cur->sharer_set.begin(), home_id, cache_level[num_levels-1].block_size, timer+delay);
                line_cur->state = S;
            }
            else if (line_cur->state == S) {
                if ((protocol_type == LIMITED_PTR) && ((int)line_cur->sharer_set.size() >= max_num_sharers)) {
                    line_cur->state = B;
                }
                else {
                    line_cur->state = S;
                }
            } 
            else if (line_cur->state == B) {
                line_cur->state = B;
            } 
            else if (line_cur->state == V) {
                line_cur->state = E;
            } 
            line_cur->sharer_set.insert(cache_id);
        }
        //Writeback 
        else {
            line_cur->state = V;
            line_cur->sharer_set.clear();
            dram.access(ins_mem);
        }
    }
    if (line_cur->state == B) {
        (*state) = S;
    }
    else {
        (*state) = line_cur->state;
    }
    line_cur->timestamp = timer;
    directory_cache[home_id]->unlockUp(ins_mem);
    return delay;
}


//TLB translation from virtual addresses into physical addresses
int System::tlb_translate(InsMem *ins_mem, int core_id, int64_t timer)
{
    int delay = 0;
    Line* line_cur;
    InsMem ins_mem_old;
    line_cur = tlb_cache[core_id].accessLine(ins_mem);
    tlb_cache[core_id].incInsCount();
    delay += tlb_cache[core_id].getAccessTime();
    if (line_cur == NULL) {
        line_cur = tlb_cache[core_id].replaceLine(&ins_mem_old, ins_mem);
        if (line_cur->state) {
            tlb_cache[core_id].incEvictCount();
        }
        tlb_cache[core_id].incMissCount();
        line_cur->state = V;
        line_cur->ppage_num = page_table.translate(ins_mem);
        delay += page_table.getTransDelay();
    }
    line_cur->timestamp = timer;
    ins_mem->addr_dmem = (line_cur->ppage_num << (int)log2(page_size)) | (ins_mem->addr_dmem % page_size);
    return delay;
}

// Home allocation uses low-order bits interleaving
int System::allocHomeId(int num_homes, uint64_t addr)
{
    int offset;
    int home_bits;
    int home_mask;
    //Home bits
    offset = (int) log2(xml_sys->directory_cache.block_size );
    home_mask = (int)ceil(log2(num_homes));
    home_bits = (int)(((addr >> offset)) % (1 << home_mask));
    if (home_bits < num_homes) {
        return home_bits;
    }   
    else {
        return home_bits % (1 << (home_mask-1));
    }
}

int System::getHomeId(InsMem *ins_mem)
{
    int home_id;
    home_id = allocHomeId(network.getNumNodes(), ins_mem->addr_dmem);
    if (home_id < 0) {
        cerr<<"wrong home id\n";
    }
    return home_id;
}



int System::getCoreCount()
{
    return num_cores;
}


void System::report(ofstream* result)
{
    int i, j;
    uint64_t ins_count = 0, miss_count = 0, evict_count = 0, wb_count = 0;
    double miss_rate = 0;
   
    network.report(result); 
    dram.report(result); 
    *result << endl <<  "Simulation result for cache system: \n\n";
    
    if (verbose_report) {
        *result << "Home Occupation:\n";
        if (network.getNetType() == MESH_3D) {
            *result << "Allocated home locations in 3D coordinates:" << endl;
            for (int i = 0; i < network.getNumNodes(); i++) {
                if (home_stat[i]) {
                    Coord loc = network.getLoc(i); 
                    *result << "("<<loc.x<<", "<<loc.y<<", "<<loc.z<<")\n";
                }
            }
        }
        else {
            *result << "Allocated home locations in 2D coordinates:" << endl;
            for (int i = 0; i < network.getNumNodes(); i++) {
                if (home_stat[i]) {
                    Coord loc = network.getLoc(i); 
                    *result << "("<<loc.x<<", "<<loc.y<<")\n";
                }
             }
        }
        *result << endl;
    }
    *result << endl;

    if (tlb_enable) {
        ins_count =0;   
        miss_count = 0;
        evict_count = 0;
        miss_rate = 0;
        for (int i = 0; i < num_cores; i++) {
            ins_count += tlb_cache[i].getInsCount();
            miss_count += tlb_cache[i].getMissCount();
            wb_count += tlb_cache[i].getWbCount();
        }
        miss_rate = (double)miss_count / (double)ins_count; 
           
        *result << "TLB Cache"<<"===========================================================\n";
        *result << "Simulation results for "<< xml_sys->tlb_cache.size << " Bytes " << xml_sys->tlb_cache.num_ways
                   << "-way set associative cache model:\n";
        *result << "The total # of TLB access instructions: " << ins_count << endl;
        *result << "The # of cache-missed instructions: " << miss_count << endl;
        *result << "The # of replaced instructions: " << evict_count << endl;
        *result << "The cache miss rate: " << 100 * miss_rate << "%" << endl;
        *result << "=================================================================\n\n";
        
        if (verbose_report) {
            page_table.report(result);
        }
    } 

    *result << endl;
    *result << "Total delay caused by bus contention: " << total_bus_contention <<" cycles\n";
    *result << "Total # of broadcast: " << total_num_broadcast <<"\n\n";

    for (i=0; i<num_levels; i++) {
        ins_count =0;   
        miss_count = 0;
        evict_count = 0;
        wb_count = 0;
        miss_rate = 0;
        for (j=0; j<cache_level[i].num_caches; j++) {
            if (cache[i][j] != NULL) {
                ins_count += cache[i][j]->getInsCount();
                miss_count += cache[i][j]->getMissCount();
                evict_count += cache[i][j]->getEvictCount();
                wb_count += cache[i][j]->getWbCount();
            }
        }
        miss_rate = (double)miss_count / (double)ins_count; 

        *result << "LEVEL"<<i<<"===========================================================\n";
        *result << "Simulation results for "<< cache_level[i].size << " Bytes " << cache_level[i].num_ways
                   << "-way set associative cache model:\n";
        *result << "The total # of memory instructions: " << ins_count << endl;
        *result << "The # of cache-missed instructions: " << miss_count << endl;
        *result << "The # of evicted instructions: " << evict_count << endl;
        *result << "The # of writeback instructions: " << wb_count << endl;
        *result << "The cache miss rate: " << 100 * miss_rate << "%" << endl;
        *result << "=================================================================\n\n";
    }
    
    ins_count =0;   
    miss_count = 0;
    evict_count = 0;
    wb_count = 0;
    miss_rate = 0;
    for (int i = 0; i < network.getNumNodes(); i++) {
        if (directory_cache[i] != NULL) {
            ins_count += directory_cache[i]->getInsCount();
            miss_count += directory_cache[i]->getMissCount();
            evict_count += directory_cache[i]->getEvictCount();
            wb_count += directory_cache[i]->getWbCount();
        }
    }
    miss_rate = (double)miss_count / (double)ins_count; 
       
    *result << "Directory Cache"<<"===========================================================\n";
    *result << "Simulation results for "<< xml_sys->directory_cache.size << " Bytes " << xml_sys->directory_cache.num_ways
               << "-way set associative cache model:\n";
    *result << "The total # of memory instructions: " << ins_count << endl;
    *result << "The # of cache-missed instructions: " << miss_count << endl;
    *result << "The # of replaced instructions: " << evict_count << endl;
    *result << "The cache miss rate: " << 100 * miss_rate << "%" << endl;
    *result << "=================================================================\n\n";


    if (verbose_report) {
        *result << "Statistics for each cache with non-zero accesses: \n\n";
        for (i=0; i<num_levels; i++) {
            *result << "LEVEL"<<i<<"*****************************************************\n\n";
            for (j=0; j<cache_level[i].num_caches; j++) {
                if (cache[i][j] != NULL) {
                    if (cache[i][j]->getInsCount() > 0) {
                        *result << "The " <<j<<"th cache:\n";
                        cache[i][j]->report(result);
                    }
                }
             }
            *result << "************************************************************\n\n";
        }

        if (directory_cache != NULL) {
            *result << "****************************************************" <<endl;
            *result << "Statistics for each directory caches with non-zero accesses" <<endl;
            for (int i = 0; i < network.getNumNodes(); i++) {
                if (directory_cache[i] != NULL) {
                    if (directory_cache[i]->getInsCount() > 0) {
                        *result << "Report for directory cache "<<i<<endl;
                        directory_cache[i]->report(result);
                    }
                }
            }
        }

        if (tlb_cache != NULL) {
            *result << "****************************************************" <<endl;
            *result << "Statistics for each TLB cache with non-zero accesses" <<endl;
            for (int i = 0; i < num_cores; i++) {
                if (tlb_cache[i].getInsCount() > 0) {
                    *result << "Report for tlb cache "<<i<<endl;
                    tlb_cache[i].report(result);
                }
            }
        }
    }
}

System::~System()
{
        int i, j;
        for (i=0; i<num_levels; i++) {
            for (j=0; j<cache_level[i].num_caches; j++) {
                if (cache[i][j] != NULL) {
                    if (i != 0) {
                        delete [] cache[i][j]->child;
                    }
                    delete cache[i][j];
                }
            }
        }
        for (i=0; i<num_levels; i++) {
            delete [] cache[i];
        }
        for (i = 0; i < network.getNumNodes(); i++) {
            if (directory_cache[i] != NULL) {
                delete directory_cache[i];
            }   
        }
        delete [] hit_flag;
        delete [] delay;
        delete [] home_stat;
        delete [] cache;
        delete [] cache_level;
        delete [] directory_cache;
        delete [] tlb_cache;
}
