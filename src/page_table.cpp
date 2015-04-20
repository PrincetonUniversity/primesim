//===========================================================================
// page_table.cpp contains a simple page translation model to allocate 
// physical pages sequentially starting from 0
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


#include "page_table.h"

using namespace std;



void PageTable::init(int page_size_in, int delay_in)
{
    page_size = page_size_in;
    delay = delay_in;
    empty_page_num = 0;
    page_map.clear();
    lock = new pthread_mutex_t;
    pthread_mutex_init(lock, NULL);
}

// Return page number
uint64_t PageTable::getPageId(uint64_t addr)
{
    return addr >> (int)log2(page_size);
}

//Translate virtual page number into physical page number
uint64_t PageTable::translate(InsMem* ins_mem)
{
    uint64_t ppage_num;
    PageMap::iterator it;
    pthread_mutex_lock(lock);
    it = page_map.find(UKey(ins_mem->prog_id, getPageId(ins_mem->addr_dmem)));
    if(it == page_map.end()) {
        page_map[UKey(ins_mem->prog_id, getPageId(ins_mem->addr_dmem))] = empty_page_num;
        ppage_num = empty_page_num;
        empty_page_num++;
    }
    else {
        ppage_num = it->second;
    }
    pthread_mutex_unlock(lock);
    return ppage_num;
}

int PageTable::getTransDelay()
{
    return delay;
}

void PageTable::report(ofstream* result)
{
    *result << "Page translation:\n";
    *result << "Total # of pages: " << page_map.size() <<endl;
    for (PageMap::iterator pos_page_table = page_map.begin(); pos_page_table != page_map.end(); ++pos_page_table) {
        *result <<dec<< "(proc ID: " << pos_page_table->first.first << " ,vpage Num: " <<hex<<pos_page_table->first.second << ") => "
                << "ppage Num: " << pos_page_table->second <<dec<< endl;
    }
}

PageTable::~PageTable()
{
    pthread_mutex_destroy(lock);
    delete lock;
}
