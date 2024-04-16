/********************************************************************************
 * @file    life.h
 * @author  Filip Jahn (xjahnf00)
 * @date    2024-04-16
 * @brief   Declaration of the GameOfLife class, which simulates Conway's Game of Life using MPI.
 * 
 * @note This header file contains the declaration of the GameOfLife class, which simulates Conway's Game of Life using MPI for parallelization.
 * The class provides methods to initialize the simulation, run the simulation for multiple time steps, and print the final state of the grid.
 * 
 * @note To use this class, include this header file in your source code:
 *   #include "GameOfLife.h"
 * 
 * @note Then, create an instance of the GameOfLife class and call its methods to run the simulation.
 ********************************************************************************/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mpi.h>

#define ALIVE_CELL 1
#define DEAD_CELL 0
#define N_ROWS_LOCAL_W_GHOST    3           // Number of rows for each process with ghost rows

class GameOfLife {
public:
    GameOfLife(int argc, char** argv);
    ~GameOfLife();

    void runSimulation();
    void readInputFile(const std::string& filename);

private:
    int rank, noRanks;
    int line_length;
    int gameTime;
    std::vector<int> current_line_in_ints;
    std::vector<std::vector<int>> currGrid;
    std::vector<std::vector<int>> nextGrid;

    void initializeMPI(int argc, char** argv);
    void communicateBoundaryRows();
    void calculateNextGrid();
    void swapGrids();
    void printGrid();
};

