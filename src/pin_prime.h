//===========================================================================
// pin_prime.h 
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

#ifndef  PIN_PRIME_H
#define  PIN_PRIME_H

#include <stdio.h>
#include "portability.H"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <syscall.h>
#include <utmpx.h>
#include <dlfcn.h>
#include "pin.H"
#include "instlib.H"
#include "mpi.h"
#include "xml_parser.h"
#include "common.h"
#include "core_manager.h"


int myrank, new_rank, num_tasks;
MPI_Status status;          /* MPI receive routine parameter */
MPI_Group  orig_group, new_group;
MPI_Comm   new_comm;
ofstream result;
CoreManager *core_manager;



KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "result", "specify output file name");

KNOB<string> KnobConfigFile(KNOB_MODE_WRITEONCE, "pintool",
    "c", "config.xml", "specify config file name");

KNOB<BOOL> KnobOnlyROI(KNOB_MODE_WRITEONCE, "pintool",
    "roi", "0", "collect data for ROI only");


#endif // PIN_PRIME_H 
