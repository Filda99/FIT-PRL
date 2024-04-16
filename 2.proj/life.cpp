/********************************************************************************
 * @file    life.cpp
 * @author  Filip Jahn (xjahnf00)
 * @date    2024-04-16
 * Subject  PRL
 * @brief   Implementation of the GameOfLife class, which simulates Conway's Game of Life using MPI.
 * 
 * @note This program implements Conway's Game of Life, a cellular automaton, using the Message Passing Interface (MPI) for parallelization.
 * It reads an input file containing the initial state of the grid, runs the simulation for a specified number of time steps,
 * and prints the final state of the grid. The simulation is parallelized across multiple MPI processes.
 * The program is limited bu the number of CPUs. The number of CPUs must be equal to the number of rows in the input file.
 * 
 * @note To compile the program, use a C++ compiler with MPI support. For example:
 *   mpic++ --prefix /usr/local/share/OpenMPI --std=c++17 -o life life.cpp
 * 
 * @note To run the program, use the mpirun command with the desired number of MPI processes:
 *   mpirun --oversubscribe --prefix /usr/local/share/OpenMPI -np <rows_count> life <input_file> <num_of_steps>
 * 
 * @note This program requires a text file containing the initial state of the grid and the number of time steps for the simulation.
 * The input file should contain rows of 0s and 1s, where 0 represents a dead cell and 1 represents a live cell.
 * 
 * @note The program uses the following rules to determine the next state of each cell in the grid:
 * - Any live cell with fewer than two live neighbours dies (underpopulation).
 * - Any live cell with two or three live neighbours lives on to the next generation.
 * - Any live cell with more than three live neighbours dies (overpopulation).
 * - Any dead cell with exactly three live neighbours becomes a live cell (reproduction).
 * 
 * @note The program prints the final state of the grid to the standard output, where each row represents a row in the grid.
 * The final state of the grid is printed in the same format as the input file, with 0s and 1s representing dead and live cells, respectively.
 * 
 * @note The program was tested using generated test cases for 4x4 to 12x12 grids with various initial configurations with different numbers of time steps.
 * 
 * @cite This code is based on the following source:
 *  - http://www.shodor.org/media/content/petascale/materials/UPModules/GameOfLife/Life_Module_Document_pdf.pdf
 ********************************************************************************/

#include "life.h"


/**
 * Constructor for the GameOfLife class.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @brief Initializes MPI and retrieves the game time from command-line arguments.
 */
GameOfLife::GameOfLife(int argc, char **argv)
{
    initializeMPI(argc, argv);

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <file.txt> <game time>\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Get time argument from command line
    gameTime = atoi(argv[2]);
}


/**
 * Destructor for the GameOfLife class.
 *
 * @brief Finalizes MPI.
 */
GameOfLife::~GameOfLife()
{
    MPI_Finalize();
}


/**
 * Initializes MPI.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @brief Initializes MPI environment and retrieves the rank and size of the MPI communicator.
 */
void GameOfLife::initializeMPI(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &noRanks);
}


/**
 * Reads the input file and initializes the simulation grid (by creating
 *  the number of processes as the count of the rows).
 *
 * @param filename Name of the input file.
 * @brief Reads the input file and initializes the simulation grid based on the contents of the file.
 */
void GameOfLife::readInputFile(const std::string &filename)
{
    if (rank == 0)
    {
        // Read the file and store lines in a vector
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Error opening file" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        std::vector<std::string> lines;
        std::string line;
        while (getline(file, line))
        {
            lines.push_back(line);
        }
        file.close();

        if (lines.size() > 1)
        {
            const std::string &line = lines[0];
            line_length = line.size();

            current_line_in_ints.push_back(0); // Add a ghost cell
            for (char c : line)
            {
                current_line_in_ints.push_back(c - '0');
            }
            current_line_in_ints.push_back(0); // Add a ghost cell

            MPI_Bcast(&line_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
        }

        for (int i = 1; i < lines.size(); i++)
        {
            if (lines[i].size() == line_length)
            {
                const std::string &line = lines[i];
                std::vector<int> line_data;
                for (char c : line)
                {
                    line_data.push_back(c - '0');
                }
                line_data.pop_back();
                MPI_Send(line_data.data(), line_length, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
            else
            {
                std::cerr << "0: [Error]: The lines are not the same length." << std::endl;
                std::cerr << "0: The error occurred at line " << i << " it's len is: " << lines[i].size() << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }
    else
    {
        MPI_Bcast(&line_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
        std::vector<int> received_line(line_length, 0);
        MPI_Recv(received_line.data(), line_length, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        current_line_in_ints.push_back(0);
        current_line_in_ints.insert(current_line_in_ints.end(), received_line.begin(), received_line.end());
        current_line_in_ints.push_back(0);
    }

    currGrid.assign(N_ROWS_LOCAL_W_GHOST, current_line_in_ints);
    nextGrid.assign(N_ROWS_LOCAL_W_GHOST, current_line_in_ints);
}


/**
 * Communicates boundary rows between MPI processes.
 * 
 * @brief Sends and receives boundary rows between adjacent MPI processes.
 */
void GameOfLife::communicateBoundaryRows()
{
    auto upperNeighbour = (rank - 1 + noRanks) % noRanks;
    auto lowerNeighbour = (rank + 1) % noRanks;

    MPI_Send(&currGrid[1][1], line_length, MPI_INT, upperNeighbour, 0, MPI_COMM_WORLD);
    MPI_Send(&currGrid[1][1], line_length, MPI_INT, lowerNeighbour, 0, MPI_COMM_WORLD);

    MPI_Recv(&currGrid[0][1], line_length, MPI_INT, upperNeighbour, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&currGrid[2][1], line_length, MPI_INT, lowerNeighbour, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    currGrid[0][0] = currGrid[0][line_length];
    currGrid[0][line_length + 1] = currGrid[0][1];
    currGrid[1][0] = currGrid[1][line_length];
    currGrid[1][line_length + 1] = currGrid[1][1];
    currGrid[2][0] = currGrid[2][line_length];
    currGrid[2][line_length + 1] = currGrid[2][1];
}


/**
 * Calculates the next state of the grid based on the current state.
 * 
 * @brief Applies the rules of the Game of Life to calculate the next state of the grid.
 */
void GameOfLife::calculateNextGrid()
{
    for (auto j = 1; j <= line_length; j++)
    {
        int aliveNeighbours = 0;
        for (auto x = -1; x <= 1; x++)
        {
            for (auto y = -1; y <= 1; y++)
            {
                if (x == 0 && y == 0)
                    continue;
                if (currGrid[1 + x][j + y] == ALIVE_CELL)
                    aliveNeighbours++;
            }
        }

        if (currGrid[1][j] == ALIVE_CELL)
        {
            if (aliveNeighbours < 2 || aliveNeighbours > 3)
            {
                nextGrid[1][j] = DEAD_CELL;
            }
            else
            {
                nextGrid[1][j] = ALIVE_CELL;
            }
        }
        else
        {
            if (aliveNeighbours == 3)
            {
                nextGrid[1][j] = ALIVE_CELL;
            }
            else
            {
                nextGrid[1][j] = DEAD_CELL;
            }
        }
    }
}


/**
 * Swaps the current grid with the next grid.
 * 
 * @brief Swaps the current grid with the next grid for the next iteration of the simulation.
 */
void GameOfLife::swapGrids()
{
    currGrid.swap(nextGrid);
}


/**
 * Prints the grid state.
 * 
 * @brief Prints the current state of the grid to the standard output.
 */
void GameOfLife::printGrid()
{
    if (rank != 0)
    {
        MPI_Send(&currGrid[1][1], line_length, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    else
    {
        std::vector<int> allRows(line_length, 0);
        for (size_t i = 0; i < line_length; ++i)
        {
            allRows[i] = currGrid[1][i + 1];
        }
        std::cout << "0: ";
        for (auto cell : allRows)
        {
            std::cout << cell;
        }
        std::cout << std::endl;

        for (auto i = 1; i < noRanks; i++)
        {
            MPI_Recv(allRows.data(), line_length, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::cout << i << ": ";
            for (auto cell : allRows)
            {
                std::cout << cell;
            }
            std::cout << std::endl;
        }
    }
}


/**
 * Runs the Game of Life simulation.
 * 
 * @brief Executes the main simulation loop for the specified number of time steps.
 */
void GameOfLife::runSimulation()
{
    for (auto currTime = 0; currTime < gameTime; currTime++)
    {
        communicateBoundaryRows();
        calculateNextGrid();
        swapGrids();
    }

    printGrid();
}


/**
 * Entry point for the Game of Life program.
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return Returns 0 upon successful execution of the program.
 * @brief Parses command-line arguments, initializes the GameOfLife object, runs the simulation, and returns the result.
 */
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <file.txt> <game time>\n", argv[0]);
        return 1;
    }

    GameOfLife game(argc, argv);
    game.readInputFile(argv[1]);
    game.runSimulation();

    return 0;
}