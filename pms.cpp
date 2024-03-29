/***************************************************************
 * File: pms.cpp
 * Author: Filip Jahn
 * Login: xjahnf00
 * Email: xjahnf00@stud.fit.vutbr.cz
 * Date Created: 2024-03-29
 * Description: Implementation file for the Pipeline Merge Sort (PMS) system.
 *              This program implements a pipeline merge sort algorithm
 *              capable of sorting 2^i numbers efficiently.
 ***************************************************************/

/***************************************************************
 * Compile with: mpic++ -std=c++17 pms.cpp -o pms
 * 
 * Run with:        mpirun -np {number of processes} ./pms
 * 
 * Example:         mpirun -np 4 ./pms
 * 
 * Capabilities:    This program can sort 2^i numbers ascending, where i is the 
 *                  number of processes.
 *                  The program reads the numbers from a file called "numbers"
 *                  and outputs the sorted numbers to the console.
 * 
 * Tested on:       This program was tested on the Merlin cluster.
 *                  Program was tested on 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 processes.
 *                  If there are same numbers, that is no problem for this program.
 ***************************************************************/

#include <iostream>
#include <fstream>
#include <mpi.h>
#include <deque>
#include <vector>
#include <algorithm>
#include <cmath>

// Constants for message tags
constexpr int MSG_TAG = 0;
constexpr int MSG_FINAL = 1;

constexpr int MIN_ITEMS_IN_BOTTOM_QUEUE = 1;

// Enum to distinguish between top and bottom queues
enum QueuePosition
{
    Q_TOP,
    Q_BOTTOM
};

// Function prototypes
void processFirst(int procID);
void processOthers(int procID, int noProc);


int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int procID, noProc;
    MPI_Comm_rank(MPI_COMM_WORLD, &procID);
    MPI_Comm_size(MPI_COMM_WORLD, &noProc);

    if (noProc < 2)
    {
        std::cerr << "This program requires at least 2 MPI processes." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (procID == 0)
    {
        processFirst(procID);
    }
    else
    {
        processOthers(procID, noProc);
    }

    MPI_Finalize();
    return 0;
}


/**
 * void processFirst(int procID)
 * 
 * @brief Process the first process in the pipeline.
 * 
 * @param procID The process ID.
 * 
 * @return void
 * 
 * This function reads the numbers from the file and sends them to the next process in the pipeline.
 * The function also sends an EOF value to indicate the end of the file.
 * 
 * The function also prints the numbers to the console.
 * 
*/
void processFirst(int procID)
{
    std::ifstream inputFile("numbers", std::ios::binary);
    if (!inputFile)
    {
        std::cerr << "Failed to open file." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    uint8_t num;
    while (inputFile.read(reinterpret_cast<char *>(&num), sizeof(num)))
    {
        std::cout << static_cast<unsigned int>(num) << " ";
        MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, 1, MSG_TAG, MPI_COMM_WORLD);
    }
    std::cout << std::endl;

    // Send EOF value to indicate the end of file
    MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, 1, MSG_FINAL, MPI_COMM_WORLD);

    inputFile.close();
}


/**
 * void processOthers(int procID, int noProc)
 * 
 * @brief Process the other processes in the pipeline.
 * 
 * @param procID The process ID.
 * @param noProc The number of processes.
 * 
 * @return void
 * 
 * This function receives the numbers from the previous process in the pipeline.
 * The function sorts the numbers and sends them to the next process in the pipeline.
 * This is done within batches of numbers.
 * The function also sends an EOF value to indicate the end of the file.
 * 
*/
void processOthers(int procID, int noProc)
{
    // Deques for the top and bottom queues (and final numbers to be printed)
    std::deque<uint8_t> qTop, qBottom, finalNumbers;
    bool shouldIReceive = true, initCond = false, newBatch = true;
    uint8_t num, sendNum, sentItemsB = 0, sentItemsT = 0;
    // Initial queue position is top
    QueuePosition recvQueue = Q_TOP;
    // Number of items needed in the top queue
    int condNeededItemsInTQ = pow(2, procID - 1), cntRecv = 0;
    // Status for receiving messages
    MPI_Status status;

    // Lambda function to remove an element from a deque
    auto removeElement = [&](std::deque<uint8_t> &deque, uint8_t &sentItemsCounter)
    {
        auto it = std::find(deque.begin(), deque.end(), sendNum);
        if (it != deque.end())
        {
            deque.erase(it); // Erase by iterator
            sentItemsCounter++;
        }
    };

    // Main loop
    do
    {
        // Receive a number from the previous process
        if (shouldIReceive)
        {
            uint8_t recNum;
            MPI_Recv(&recNum, 1, MPI_UNSIGNED_CHAR, (procID - 1), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == MSG_FINAL)
            {
                shouldIReceive = false;
            }
            else
            {
                num = recNum;

                // Check if the number belongs to the top or bottom queue
                if (cntRecv == condNeededItemsInTQ)
                {
                    cntRecv = 0;
                    recvQueue = (recvQueue == Q_TOP) ? Q_BOTTOM : Q_TOP;
                }

                recvQueue == Q_TOP ? qTop.push_back(num) : qBottom.push_back(num);
                cntRecv++;
            }
        }

        // Check if we should initialize a new batch    
        if (newBatch)
        {
            initCond = (qTop.size() >= condNeededItemsInTQ && qBottom.size() >= MIN_ITEMS_IN_BOTTOM_QUEUE);
            initCond |= status.MPI_TAG;
        }

        // Process the numbers
        if (initCond)
        {
            newBatch = false;

            // Sort the queues (in batch)
            if (!qTop.empty() || !qBottom.empty())
            {
                std::vector<std::pair<uint8_t, char>> elements; // Pair: value and origin ('T' or 'B')
               
                // Get numbers from the queues (only the numbers in current batch) and store them in a vector with their origin
                int numElementsTop = qTop.size() >= condNeededItemsInTQ - sentItemsT ? condNeededItemsInTQ - sentItemsT : qTop.size();  
                for (int i = 0; i < numElementsTop; i++)
                {
                    elements.emplace_back(qTop[i], 'T');
                }

                int numElementsBottom = qBottom.size() >= condNeededItemsInTQ - sentItemsB ? condNeededItemsInTQ - sentItemsB : qBottom.size();  
                for (int i = 0; i < numElementsBottom; i++)
                {
                    elements.emplace_back(qBottom[i], 'B');
                }

                // Find the largest element and its origin
                auto maxElement = std::max_element(elements.begin(), elements.end(),
                                                   [](const std::pair<uint8_t, char> &a, const std::pair<uint8_t, char> &b)
                                                   {
                                                       return a.first < b.first;
                                                   });

                // Send the largest element to the next process
                if (maxElement != elements.end())
                {
                    sendNum = maxElement->first;

                    // Remove the element from the correct deque
                    maxElement->second == 'T' ? removeElement(qTop, sentItemsT) : removeElement(qBottom, sentItemsB);
                }
            }

            // Send the number to the next process
            if (procID < noProc - 1)
            {
                if (qTop.empty() && qBottom.empty())
                {
                    MPI_Send(&sendNum, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_TAG, MPI_COMM_WORLD);
                    MPI_Send(&sendNum, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_FINAL, MPI_COMM_WORLD);
                    break;
                }
                else
                {
                    MPI_Send(&sendNum, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_TAG, MPI_COMM_WORLD);
                }

            }
            // If this is the last process, store the number in the final numbers vector
            else
            {
                if (!status.MPI_TAG || !qTop.empty() || !qBottom.empty())
                {
                    finalNumbers.push_back(sendNum);
                }
                else
                {
                    finalNumbers.push_back(sendNum);
                    break;
                }
            }

            // Check if we should initialize a new batch
            if (sentItemsT == condNeededItemsInTQ && sentItemsB == condNeededItemsInTQ)
            {
                sentItemsT = 0;
                sentItemsB = 0;
                initCond = false;
                newBatch = true;
            }
        }
    } while (qTop.size() > 0 || qBottom.size() > 0);

    // Print the final numbers
    if (procID == noProc - 1)
    {
        while (!finalNumbers.empty())
        {
            std::cout << static_cast<unsigned int>(finalNumbers.back()) << "\n";
            finalNumbers.pop_back();
        }
        std::cout << std::endl;
    }
}