#!/usr/bin/env bash

size=${1-64} 
cpu=${2-ex5_LITTLE}

make clean && make compile_gem5 SVE=1 OUTPUT=1 && make run_gem5 S=$size CPU=$cpu
#diff output_gem5.out files_output/output_gem5_${size}.out > ${size}.diff
#cat ${size}.diff

