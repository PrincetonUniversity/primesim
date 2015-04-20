//===========================================================================
// xml_parser.cpp provides parser functions for input xml config file 
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

#include "xml_parser.h"

using namespace std;

XmlParser::XmlParser()
{
    xml_sim.max_msg_size = 0;
    xml_sim.num_recv_threads = 1;
    xml_sim.thread_sync_interval = 0;
    xml_sim.proc_sync_interval = 0;
    xml_sim.syscall_cost = 0;
    xml_sim.sys.sys_type = 0;
    xml_sim.sys.protocol_type = 0;
    xml_sim.sys.max_num_sharers = 0;
    xml_sim.sys.page_size = 0;
    xml_sim.sys.tlb_enable = 0;
    xml_sim.sys.shared_llc = 0;
    xml_sim.sys.verbose_report = 0;
    xml_sim.sys.cpi_nonmem = 0;
    xml_sim.sys.dram_access_time = 0;
    xml_sim.sys.num_levels = 0;
    xml_sim.sys.freq = 0;
    xml_sim.sys.num_cores = 0;
    xml_sim.sys.bus_latency = 0;
    xml_sim.sys.page_miss_delay = 0;


    xml_sim.sys.directory_cache.level = 0;
    xml_sim.sys.directory_cache.share = 0;
    xml_sim.sys.directory_cache.access_time = 0;
    xml_sim.sys.directory_cache.size = 0;
    xml_sim.sys.directory_cache.block_size = 0;
    xml_sim.sys.directory_cache.num_ways = 0;

    xml_sim.sys.tlb_cache.level = 0;
    xml_sim.sys.tlb_cache.share = 0;
    xml_sim.sys.tlb_cache.access_time = 0;
    xml_sim.sys.tlb_cache.size = 0;
    xml_sim.sys.tlb_cache.block_size = 0;
    xml_sim.sys.tlb_cache.num_ways = 0;


    xml_sim.sys.network.net_type = 0;
    xml_sim.sys.network.data_width = 0;
    xml_sim.sys.network.header_flits = 0;
    xml_sim.sys.network.inject_delay = 0;
    xml_sim.sys.network.router_delay = 0;
    xml_sim.sys.network.link_delay = 0;
}


XmlSim* XmlParser::getXmlSim()
{
    return &xml_sim;
}
        

//Get the xml pointer of a xml file
bool XmlParser::getDoc (const char *docname) 
{
	doc = xmlParseFile(docname);
	
	if (doc == NULL ) {
		cerr << "Document not parsed successfully. \n";
		return false;
	}

	return true;
}

//Get the node set of a xml file
xmlXPathObjectPtr XmlParser::getNodeSet (xmlChar *xpath)
{
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;

	context = xmlXPathNewContext(doc);
	if (context == NULL) {
		cerr << "Error in xmlXPathNewContext\n";
		return NULL;
	}
	result = xmlXPathEvalExpression(xpath, context);
	xmlXPathFreeContext(context);
	if (result == NULL) {
		cerr << "Error in xmlXPathEvalExpression\n";
		return NULL;
	}
	if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		xmlXPathFreeObject(result);
        cerr << "No result\n";
		return NULL;
	}
	return result;
}


//Parse the simulator structure and store the result 
bool XmlParser::parseSim()
{
    xmlXPathObjectPtr sim_node;
    sim_node = getNodeSet((xmlChar*) "//simulator");
     
    if (sim_node->nodesetval->nodeNr != 1) {
        xmlXPathFreeObject(sim_node);
        return false;
    }
    xmlNodePtr cur;
    xmlChar*   key;
    stringstream   convert;
    int        i, item_count = 0;
    for (i = 0; i < sim_node->nodesetval->nodeNr; i++) {
        cur = sim_node->nodesetval->nodeTab[i]->xmlChildrenNode;
	    while (cur != NULL) {
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"max_msg_size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.max_msg_size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"num_recv_threads"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.num_recv_threads;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"thread_sync_interval"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.thread_sync_interval;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"proc_sync_interval"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.proc_sync_interval;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"syscall_cost"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.syscall_cost;
                xmlFree(key);
                item_count++;
 	        }
	    cur = cur->next;
        }
	}
    xmlXPathFreeObject(sim_node);
    //Check if # of items is corret
    return (item_count == 5);
}


//Parse the system structure and store the result 
bool XmlParser::parseSys() 
{
    xmlXPathObjectPtr sys_node;
    sys_node = getNodeSet((xmlChar*) "//system");
     
    if (sys_node->nodesetval->nodeNr != 1) {
        xmlXPathFreeObject(sys_node);
        return false;
    }
    
    xmlNodePtr cur;
    xmlChar*   key;
    stringstream   convert;
    int        i, item_count = 0;
    for (i = 0; i < sys_node->nodesetval->nodeNr; i++) {
        cur = sys_node->nodesetval->nodeTab[i]->xmlChildrenNode;
	    while (cur != NULL) {
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"sys_type"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.sys_type;
                xmlFree(key);
                item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"protocol_type"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.protocol_type;
                xmlFree(key);
                item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"max_num_sharers"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.max_num_sharers;
                xmlFree(key);
                //optional item is not checked
                //item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"page_size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.page_size;
                xmlFree(key);
                item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"tlb_enable"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_enable;
                xmlFree(key);
                item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"shared_llc"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.shared_llc;
                xmlFree(key);
                item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"verbose_report"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.verbose_report;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"cpi_nonmem"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> xml_sim.sys.cpi_nonmem;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"dram_access_time"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.dram_access_time;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"num_levels"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.num_levels;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"num_cores"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.num_cores;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"bus_latency"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.bus_latency;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"page_miss_delay"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.page_miss_delay;
                xmlFree(key);
                item_count++;
 	        }

            if ((!xmlStrcmp(cur->name, (const xmlChar *)"freq"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> xml_sim.sys.freq;
                xmlFree(key);
                item_count++;
 	        }
	    cur = cur->next;
        }
	}
    xml_sim.sys.cache = new XmlCache [xml_sim.sys.num_levels]; 
    xmlXPathFreeObject(sys_node);
    return (item_count == 13);
}

 //Parse the network structure and store the result 
bool XmlParser::parseNetwork()
{
    xmlXPathObjectPtr net_node;
    net_node = getNodeSet((xmlChar*) "//network");
    
   
    if (net_node->nodesetval->nodeNr != 1) {
        xmlXPathFreeObject(net_node);
        return false;
    }
    
    xmlNodePtr cur;
    xmlChar*   key;
    stringstream   convert;
    int        i, item_count = 0;
    for (i = 0; i < net_node->nodesetval->nodeNr; i++) {
        cur = net_node->nodesetval->nodeTab[i]->xmlChildrenNode;
	    while (cur != NULL) {
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"net_type"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.network.net_type;
                xmlFree(key);
                //item_count++;
 	        }
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"data_width"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.network.data_width;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"header_flits"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.network.header_flits;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"router_delay"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.network.router_delay;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"link_delay"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.network.link_delay;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"inject_delay"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.network.inject_delay;
                xmlFree(key);
                //item_count++;
 	        }
 	    cur = cur->next;
        }
	}
    xmlXPathFreeObject(net_node);
    return (item_count == 4);
}



 //Parse the directory cache structure and store the result 
bool XmlParser::parseDirectoryCache()
{
    xmlXPathObjectPtr cache_node;
    cache_node = getNodeSet((xmlChar*) "//directory_cache");
    
   
    if (cache_node->nodesetval->nodeNr != 1) {
        xmlXPathFreeObject(cache_node);
        return false;
    }
    
    xmlNodePtr cur;
    xmlChar*   key;
    stringstream   convert;
    int        i, item_count = 0;
    for (i = 0; i < cache_node->nodesetval->nodeNr; i++) {
        cur = cache_node->nodesetval->nodeTab[i]->xmlChildrenNode;
	    while (cur != NULL) {
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"level"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.directory_cache.level;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"share"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.directory_cache.share;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"access_time"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.directory_cache.access_time;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.directory_cache.size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"block_size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.directory_cache.block_size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"num_ways"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.directory_cache.num_ways;
                xmlFree(key);
                item_count++;
 	        }
 	    cur = cur->next;
        }
	}
    xmlXPathFreeObject(cache_node);
    return (item_count == 6);
}

 //Parse the TLB cache structure and store the result 
bool XmlParser::parseTlbCache()
{
    xmlXPathObjectPtr cache_node;
    cache_node = getNodeSet((xmlChar*) "//tlb_cache");
    
   
    if (cache_node->nodesetval->nodeNr != 1) {
        xmlXPathFreeObject(cache_node);
        return false;
    }
    
    xmlNodePtr cur;
    xmlChar*   key;
    stringstream   convert;
    int        i, item_count = 0;
    for (i = 0; i < cache_node->nodesetval->nodeNr; i++) {
        cur = cache_node->nodesetval->nodeTab[i]->xmlChildrenNode;
	    while (cur != NULL) {
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"level"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_cache.level;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"share"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_cache.share;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"access_time"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_cache.access_time;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_cache.size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"block_size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_cache.block_size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"num_ways"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.tlb_cache.num_ways;
                xmlFree(key);
                item_count++;
 	        }
 	    cur = cur->next;
        }
	}
    xmlXPathFreeObject(cache_node);
    return (item_count == 6);
}

       
 
//Parse the cache structure and store the result 
bool XmlParser::parseCache()
{
    xmlXPathObjectPtr cache_node;
    cache_node = getNodeSet((xmlChar*) "//cache");
     
    if (cache_node->nodesetval->nodeNr != xml_sim.sys.num_levels) {
        xmlXPathFreeObject(cache_node);
        return false;
    }
    
    xmlNodePtr cur;
    xmlChar*   key;
    stringstream   convert;
    int        i, item_count = 0;
    for (i = 0; i < cache_node->nodesetval->nodeNr; i++) {
        cur = cache_node->nodesetval->nodeTab[i]->xmlChildrenNode;
	    while (cur != NULL) {
	        if ((!xmlStrcmp(cur->name, (const xmlChar *)"level"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.cache[i].level;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"share"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.cache[i].share;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"access_time"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.cache[i].access_time;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.cache[i].size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"block_size"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.cache[i].block_size;
                xmlFree(key);
                item_count++;
 	        }
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"num_ways"))) {
		        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                convert.clear();
                convert.str("");
		        convert << key;
                convert >> dec >> xml_sim.sys.cache[i].num_ways;
                xmlFree(key);
                item_count++;
 	        }
	    cur = cur->next;
        }
	}
    xmlXPathFreeObject(cache_node);
    return (item_count == 6 * xml_sim.sys.num_levels);
}

//Parse the entire xml file and store the result
bool XmlParser::parse(const char *docname)
{
    if(getDoc(docname)){
        if (!parseSim()) {
            cerr << "Error in parsing simulator structure!\n";
            return false;
        }
        else if (!parseSys()) {
            cerr << "Error in parsing system structure!\n";
            return false;
        }
        else if (!parseNetwork()) {
            cerr << "Error in parsing network structure!\n";
            return false;
        }
        else if (!parseDirectoryCache()) {
            cerr << "Error in parsing directory cache structure!\n";
            return false;
        }
        else if (!parseTlbCache()) {
            cerr << "Error in parsing TLB cache structure!\n";
            return false;
        }
        else if (!parseCache()) {
            cerr << "Error in parsing cache structure!\n";
            return false;
        }
        else
            return true;
    }
    else {
        cout << "Error in get xml document!\n";
        return false;
    }
}

XmlParser::~XmlParser()
{
    if (xml_sim.sys.num_levels > 0) {
        delete [] xml_sim.sys.cache;
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
