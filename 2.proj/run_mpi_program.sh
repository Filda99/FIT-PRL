#!/bin/bash

#preklad zdrojoveho souboru
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

rows_count=$(wc -l < "$input_file")

#spusteni programu
mpirun --oversubscribe --prefix /usr/local/share/OpenMPI -np $rows_count life "$input_file" "$num_of_steps"

#uklid
rm -f life