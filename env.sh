#!/bin/bash

# home directory for PriME, set it to the correct path
export PRIME_PATH=/home/yfu/research/primesim

# home directory for Pin, set it to the correct path
export PINPATH=/home/yfu/softwares/pin/pin-2.14-71313-gcc.4.4.7-linux

# path to libmpi.so in OpenMPI library, set it to the correct path with the same 
# format as below (including those backslashes and quotes). Besides, you need to 
# make sure the library file and the default mpic++ used in the Makefile are of 
# the same version
export OPENMPI_LIB_PATH=\"\\\"/usr/local/lib/libmpi.so\\\"\"

# path to libxml2 , set it to the correct path
export LIBXML2_PATH=/usr/include/libxml2

# path to PARSEC benchmarks, set it to the correct path if you want to run PARSEC
export PARSEC_PATH=/home/yfu/research/parsec-3.0

# set path
export PATH=$PRIME_PATH/tools:$PATH
