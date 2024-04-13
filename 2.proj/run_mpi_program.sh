#!/bin/bash

#preklad zdrojoveho souboru
mpic++ --prefix /usr/local/share/OpenMPI --std=c++17 -o life life.cpp

inputFile="gameItself.txt"

rows_count=$(cat $inputFile | wc -l)
rows_count=$((rows_count+1))

#spusteni programu
mpirun --oversubscribe --prefix /usr/local/share/OpenMPI -np $rows_count life $inputFile

#uklid
rm -f life numbers