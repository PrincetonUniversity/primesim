//===========================================================================
// xml_parser.h 
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

#ifndef XML_PARSER_H
#define XML_PARSER_H

#include <string>
#include <inttypes.h>
#include <fstream>
#include <sstream>
#include <assert.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>


typedef struct XmlCore
{
    xmlChar*   path;
} XmlCore;

typedef struct XmlCache
{
    int         level;
    int         share;
    int         access_time;
    uint64_t    size;
    uint64_t    block_size;
    uint64_t    num_ways;
} XmlCache;

typedef struct XmlNetwork
{
    int data_width;
    int header_flits;
    int net_type;
    uint64_t router_delay;
    uint64_t link_delay;
    uint64_t inject_delay;
} XmlNetwork;



typedef struct XmlSys
{
    int        sys_type;
    int        protocol_type;
    int        max_num_sharers;
    int        page_size;
    int        tlb_enable;
    int        shared_llc;
    int        verbose_report;
    double     cpi_nonmem;
    int        dram_access_time;
    int        num_levels;
    int        num_cores;
    double     freq;
    int        bus_latency;
    int        page_miss_delay;
    XmlNetwork network;
    XmlCache   directory_cache;
    XmlCache   tlb_cache;
    XmlCache*  cache;
} XmlSys;

typedef struct XmlSim
{
    int        max_msg_size;
    int        num_recv_threads;
    int        thread_sync_interval;
    int        proc_sync_interval;
    int        syscall_cost;
    XmlSys     sys; 
} XmlSim;


class XmlParser 
{
    public:
        XmlParser();
        XmlSim*   getXmlSim();
        bool getDoc(const char *docname);
        xmlXPathObjectPtr getNodeSet(xmlChar *xpath);
        bool parseCache(); 
        bool parseNetwork(); 
        bool parseDirectoryCache(); 
        bool parseTlbCache(); 
        bool parseSys(); 
        bool parseSim(); 
        bool parse(const char *docname);
        ~XmlParser();
    private:
        XmlSim   xml_sim;
        xmlDocPtr doc;
};

#endif  // XML_PARSER_H
