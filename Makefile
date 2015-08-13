
###########################################################################
#Copyright (c) 2015 Princeton University
#All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of Princeton University nor the
#      names of its contributors may be used to endorse or promote products
#      derived from this software without specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY PRINCETON UNIVERSITY "AS IS" AND
#ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
#DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##############################################################################



SIM_ROOT ?= $(PRIME_PATH)
PIN_VERSION_SCRIPT = $(shell find $(PINPATH) -name pintool.ver)
GRAPHITE_PATH = $(SIM_ROOT)/src/Graphite

TOP_LEVEL_PROGRAM_NAME := bin/prime.so bin/prime
CXX_FILES := $(wildcard src/*.cpp src/Graphite/*.cpp)
DEP_FILES := $(CXX_FILES:src/%.cpp=dep/%.d)
O_FILES := $(filter-out obj/core_manager.o, $(filter-out obj/pin_prime.o, $(CXX_FILES:src/%.cpp=obj/%.o)))
PIN_O_FILES := obj/pin_prime.o obj/pin_xml_parser.o obj/pin_core_manager.o


PIN_CXX_FLAGS := -Wall -Werror -Wno-unknown-pragmas  -O3 -fomit-frame-pointer \
           -DBIGARRAY_MULTIPLIER=1 -DUSING_XED  -fno-strict-aliasing \
           -I$(LIBXML2_PATH) -g3 -I$(PINPATH)/source/tools/Include \
           -I$(PINPATH)/source/tools/InstLib -I$(PINPATH)/extras/xed-intel64/include \
           -I$(PINPATH)/extras/components/include -I$(PINPATH)/source/include \
           -I$(PINPATH)/source/include/pin \
           -I$(PINPATH)/source/include/pin/gen \
           -I$(PINPATH)/source/include/gen -fno-stack-protector -DTARGET_IA32E -DHOST_IA32E \
           -fPIC -DTARGET_LINUX 

PIN_LD_FLAGS :=  -Wl,--hash-style=sysv -shared -Wl,-Bsymbolic \
           -Wl,--version-script=$(PIN_VERSION_SCRIPT)  \
           -L$(PINPATH)/source/tools/Lib/ -lxml2 -lz -lm -g3 -O3 \
           -L$(PINPATH)/source/tools/ExtLib/ -L$(PINPATH)/extras/xed-intel64/lib -L$(PINPATH)/intel64/lib \
           -L$(PINPATH)/intel64/lib-ext  -lpin  -lxed -lpindwarf -ldl -lrt 

CXX_FLAGS := -Wall -Werror -Wno-unknown-pragmas -O3 -g3 -lrt -I$(LIBXML2_PATH) -I$(GRAPHITE_PATH)

LD_FLAGS := -lxml2 -lz -lm -g3 -O3 -ldl -lrt 
           

.PHONY: clean remove 

all: $(TOP_LEVEL_PROGRAM_NAME)


obj/pin_prime.o: src/pin_prime.cpp dep/pin_prime.d
	mpic++ -c $< -o $@ $(PIN_CXX_FLAGS) -D OPENMPI_PATH=$(OPENMPI_LIB_PATH)

obj/pin_xml_parser.o: src/xml_parser.cpp dep/xml_parser.d
	mpic++ -c $< -o $@ $(PIN_CXX_FLAGS)

obj/pin_core_manager.o: src/core_manager.cpp dep/core_manager.d
	mpic++ -c $< -o $@ $(PIN_CXX_FLAGS)


obj/Graphite/%.o: src/Graphite/%.cpp dep/Graphite/%.d 
	mpic++ -c $< -o $@ $(CXX_FLAGS)

obj/%.o: src/%.cpp dep/%.d 
	mpic++ -c $< -o $@ $(CXX_FLAGS)

dep/pin_prime.d: src/pin_prime.cpp
	mpic++ -MM $(PIN_CXX_FLAGS) -MT '$(patsubst src/%.cpp,obj/%.o,$<)' $< -MF $@


dep/core_manager.d: src/core_manager.cpp
	mpic++ -MM $(PIN_CXX_FLAGS) -MT '$(patsubst src/%.cpp,obj/%.o,$<)' $< -MF $@

dep/Graphite/%.d: src/Graphite/%.cpp
	mpic++ -MM $(CXX_FLAGS) -MT '$(patsubst src/Graphite/%.cpp,obj/Graphite/%.o,$<)' $< -MF $@


dep/%.d: src/%.cpp
	mpic++ -MM $(CXX_FLAGS) -MT '$(patsubst src/%.cpp,obj/%.o,$<)' $< -MF $@


bin/prime: $(O_FILES)
	mpic++ $^ -o $@ $(LD_FLAGS)

bin/prime.so: $(PIN_O_FILES)
	mpic++ $^ -o $@ $(PIN_LD_FLAGS)

clean:
	rm -f dep/*.d dep/Graphite/*.d obj/*.o obj/Graphite/*.o bin/* 

remove:
	rm -f dep/*.d dep/Graphite/*.d obj/*.o obj/Graphite/*.o bin/* xml/*.xml cmd/*

