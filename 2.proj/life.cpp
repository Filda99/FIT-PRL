// nacteni matice -> posilani zprav dalsim procesum
// zasilani radku nad sebe a pod sebe
// vypis matice (gather posles 0. a ten to printne)
// vypocet radky po procesech

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mpi.h>

#define FILENAME                argv[1]
#define N_ROWS_LOCAL_W_GHOST    3           // Number of rows for each process with ghost rows

#define ALIVE_CELL              1
#define DEAD_CELL               0

using namespace std;

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <file.txt> <game time>\n", argv[0]);
        exit(1);
    }
    // Get time argument from command line
    int gameTime = atoi(argv[2]);

    int rank, noRanks;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &noRanks);

    vector<int> current_line_in_ints;
    int line_length = 0;

    if (rank == 0)
    {
        // Read the file and store lines in a vector
        ifstream file(argv[1]);
        if (!file.is_open())
        {
            cerr << "Error opening file" << endl;
            return 1;
        }

        vector<string> lines;
        string line;
        while (getline(file, line))
        {
            lines.push_back(line);
        }
        file.close();

        // Send the length of the lines and save the first line to the zero process
        if (lines.size() > 1)
        {
            const string &line = lines[0];
            line_length = line.size();

            // Save the first line to the current_line_in_ints
            current_line_in_ints.push_back(0); // Add a ghost cell
            for (char c : line) 
            {
                current_line_in_ints.push_back(c - '0');
            }
            current_line_in_ints.push_back(0); // Add a ghost cell

            
            // Broadcast the length of the line to all processes
            MPI_Bcast(&line_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
        }

        // Send each line to the corresponding process, leave the 0'th row for 0'th process
        for (int i = 1; i < lines.size(); i++)
        {
            // The lines has to be same length
            if ((lines[i].size()) == line_length) {
                const string &line = lines[i];
                
                // Convert string line to vector of ints
                vector<int> line_data;
                for (char c : line) 
                {
                    line_data.push_back(c - '0');
                }
                // Remove the last element (newline character)
                line_data.pop_back();
                
                // Send the vector data
                MPI_Send(line_data.data(), line_length, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
            else
            {
                // When the lines are not the same length, print an error message
                // and exit the program
                cerr << "0: [Error]: The lines are not the same length." << endl;
                cerr << "0: The error occurred at line " << i << " it's len is: " << lines[i].size() << endl;
                // Should cancel the MPI job and cancel all processes
                MPI_Abort(MPI_COMM_WORLD, 1);
                return 1;
            }
        } // lines loop
    } // rank 0
    else
    {
        // Receive the length of the line from process 0
        MPI_Bcast(&line_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Allocate memory to receive the line
        vector<int> received_line(line_length, 0);

        // Receive the line from process 0
        MPI_Recv(received_line.data(), line_length, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Push 0 at the beginning
        current_line_in_ints.push_back(0);

        // Insert values from received_line into current_line_in_ints
        current_line_in_ints.insert(current_line_in_ints.end(), received_line.begin(), received_line.end());

        // Push 0 at the end
        current_line_in_ints.push_back(0);

    } // rank != 0


    // Create a 2D space for each process with init value of it's row
    vector<vector<int> > currGrid(N_ROWS_LOCAL_W_GHOST, current_line_in_ints);
    vector<vector<int> > nextGrid(N_ROWS_LOCAL_W_GHOST, current_line_in_ints);

    auto upperNeighbour = (rank - 1 + noRanks) % noRanks;
    auto lowerNeighbour = (rank + 1) % noRanks;

    // Main time loop
    for (auto currTime = 0; currTime < gameTime; currTime++)
    {
        // Send my row to the upper neighbour
        MPI_Send(&currGrid[1][1], line_length, MPI_INT, upperNeighbour, 0, MPI_COMM_WORLD);
        // Send my row to the lower neighbour
        MPI_Send(&currGrid[1][1], line_length, MPI_INT, lowerNeighbour, 0, MPI_COMM_WORLD);

        // Receive the first row from the upper neighbour
        MPI_Recv(&currGrid[0][1], line_length, MPI_INT, upperNeighbour, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // Receive the last row from the lower neighbour
        MPI_Recv(&currGrid[2][1], line_length, MPI_INT, lowerNeighbour, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Ghost columns
        currGrid[0][0] = currGrid[0][line_length];
        currGrid[0][line_length + 1] = currGrid[0][1];
        currGrid[1][0] = currGrid[1][line_length];
        currGrid[1][line_length + 1] = currGrid[1][1];
        currGrid[2][0] = currGrid[2][line_length];
        currGrid[2][line_length + 1] = currGrid[2][1];

      
        // Calculate the next grid
        for (auto j = 1; j <= line_length; j++)
        {
            // Count the number of alive neighbours
            int aliveNeighbours = 0;
            // The current cell is at (1, j)
            // Check the 8 neighbours
            // The neighbours are at (i + x, j + y) where x and y are in [-1, 0, 1]
            for (auto x = -1; x <= 1; x++)
            {
                for (auto y = -1; y <= 1; y++)
                {
                    if (x == 0 && y == 0)
                    {
                        continue;
                    }

                    if (currGrid[1 + x][j + y] == ALIVE_CELL)
                    {
                        aliveNeighbours++;
                    }
                }
            }

            // Apply the rules of the game
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

        // Swap the current and next grids
        currGrid.swap(nextGrid);
    } // time loop

    // Send my row to zero process
    if (rank != 0)
    {
        MPI_Send(&currGrid[1][1], line_length, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    else
    {
        // Gather the rows from all processes
        vector<int> allRows(line_length, 0);
        // Copy my row to the 0'th row from index 1 to line_length
        for (size_t i = 0; i < line_length; ++i) {
            allRows[i] = currGrid[1][i + 1];
        }
        // Print the gathered row
        printf("0: ");
        for (auto cell : allRows)
        {
            cout << cell;
        }
        cout << endl;

        for (auto i = 1; i < noRanks; i++)
        {
            MPI_Recv(allRows.data(), line_length, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // Print the gathered row
            printf("%i: ", i);
            for (auto cell : allRows)
            {
                cout << cell;
            }
            cout << endl;
        }
    }
    
    MPI_Finalize();
    return 0;
}
