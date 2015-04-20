//===========================================================================
// page_table.h 
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

#ifndef  PAGETABLE_H
#define  PAGETABLE_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <stdio.h>
#include <iostream>
#include <cmath>
#include <set>
#include <map>
#include <vector>
#include "common.h"
#include "cache.h"
#include <pthread.h> 


typedef pair<int, uint64_t> UKey;
typedef map<UKey, uint64_t> PageMap; 


class PageTable
{
    public:
        void init(int page_size_in, int delay_in);
        uint64_t getPageId(uint64_t addr);
        uint64_t translate(InsMem* ins_mem);
        int getTransDelay();
        void report(ofstream* result);
        IntSet prog_set;
        ~PageTable();        
    private:
        int page_size;
        int delay;
        uint64_t empty_page_num;
        pthread_mutex_t   *lock;
        PageMap page_map;
};


#endif //PAGETABLE_H
