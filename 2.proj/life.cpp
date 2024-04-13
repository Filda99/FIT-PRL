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
#define N_GHOST_COLS            2           // Number of ghost columns

using namespace std;

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <file.txt>\n", argv[0]);
        exit(1);
    }

    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    vector<int> current_line_in_ints;

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
            cout << "0: Reading line: " << line << endl;
            lines.push_back(line);
        }
        file.close();

        // Send the length of the lines
        int line_length = 0;
        if (lines.size() > 1)
        {
            const string &line = lines[0];
            line_length = line.size() - 1; // Exclude the newline character

            // Save the first line to the current_line_in_ints
            for (char c : line) 
            {
                current_line_in_ints.push_back(c - '0');
            }
            // Remove the last element (newline character)
            current_line_in_ints.pop_back();
            
            // Broadcast the length of the line to all processes
            MPI_Bcast(&line_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
        }

        // Send each line to the corresponding process, leave the 0'th row for 0'th process
        for (int i = 1; i < lines.size(); i++)
        {
            // The lines has to be same length
            if ((lines[i].size() - 1) == line_length || ((i == lines.size() - 1) && (lines[i].size()) == line_length)) {
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
        }
    }
    else
    {
        // Receive the length of the line from process 0
        int line_length;
        MPI_Bcast(&line_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Allocate memory to receive the line
        vector<int> received_line(line_length, 0);

        // Receive the line from process 0
        MPI_Recv(received_line.data(), line_length, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Save the received line to the current_line_in_ints
        current_line_in_ints.assign(received_line.begin(), received_line.end());
    }


    // Create a 2D space for each process with init value of it's row
    vector<vector<int> > currGrid(N_ROWS_LOCAL_W_GHOST, current_line_in_ints);
    vector<vector<int> > nextGrid(N_ROWS_LOCAL_W_GHOST, current_line_in_ints);

    
    MPI_Finalize();
    return 0;
}
