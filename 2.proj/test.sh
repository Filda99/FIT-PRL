#!/bin/bash

# File:     test.sh (for Game of Life)
# Author:   Filip Jahn (xjahn00)
# Subject:  PRL
# Date:     16.04.2024

# program compile
mpic++ --prefix /usr/local/share/OpenMPI --std=c++17 -o life life.cpp

# input file
input_file=$1
# check if input file exists
if [ ! -f "$input_file" ]; then
    echo "Input file $input_file not found."
    exit 1
fi

# number of steps
num_of_steps=$2

# calculate number of rows (CPUs)
rows_count=$(wc -l < "$input_file")

# run program
mpirun --oversubscribe --prefix /usr/local/share/OpenMPI -np $rows_count life "$input_file" "$num_of_steps"

# clean up
rm -f life