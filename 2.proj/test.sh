#!/bin/bash

# File: test.sh (for Game of Life)
# Author: Michal Luner (xluner01)
# Subject: PRL
# Date: 12.04.2024

# program compile
mpic++ -std=c++11 -o life life.cpp

# input file
input_file="$1"

# number of steps
num_of_steps="$2"

# check if input file exists
if [ ! -f "$input_file" ]; then
    echo "Input file $input_file not found."
    exit 1
fi

# calculate number of rows (CPUs)
num_of_cpus=$(wc -l < "$input_file")

# run program
mpirun -np $num_of_cpus life $input_file $num_of_steps