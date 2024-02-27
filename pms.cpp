#include <iostream>
#include <fstream>
#include <mpi.h>
#include <queue>
#include <cmath>

#define MSG_TAG 0
#define MSG_TAG_EOF 1

enum QueuePosition {
    Q_TOP,
    Q_BOTTOM
};

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x
#else
#define DEBUG_PRINT(x) do {} while (0)
#endif

#define MIN_ITEMS_IN_BOTTOM_QUEUE 1

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int procID, noProc;
    MPI_Comm_rank(MPI_COMM_WORLD, &procID);
    MPI_Comm_size(MPI_COMM_WORLD, &noProc);

    std::queue<uint8_t> qTop, qBottom;

    if (noProc < 2) {
        std::cerr << "This program requires at least 2 MPI processes." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // First process
    if (procID == 0) {
        std::ifstream inputFile("numbers", std::ios::binary);
        if (!inputFile) {
            std::cerr << "Failed to open file." << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        uint8_t num;
        while (inputFile.read(reinterpret_cast<char*>(&num), sizeof(num))) {
            MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, 1, MSG_TAG, MPI_COMM_WORLD);
            DEBUG_PRINT("Process " << procID << " Input numbers: " << static_cast<unsigned int>(num) << std::endl);
        }

        // Send EOF value to indicate end of file
        MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, 1, MSG_TAG_EOF, MPI_COMM_WORLD);
        DEBUG_PRINT("Process " << procID << " Final: " << static_cast<unsigned int>(num) << std::endl);
        
        inputFile.close();
    } 

    // Any other process
    else if (procID < noProc - 1)
    {
        bool initCond = false;
        bool newBatch = true;

        // Receive numbers from the previous process and forward them to the next process
        uint8_t num;
        uint8_t sentItemsB = 0;
        uint8_t sentItemsT = 0;

        MPI_Status status;

        // In process i need to have minimum of 2^(i-1) items in the TOP queue
        int condNeededItemsInTQ = pow (2, procID - 1); 
        QueuePosition recvQueue = Q_TOP;
        uint8_t cntRecv = 0;


        while (true) {
            // Receive data from the previous process
            MPI_Recv(&num, 1, MPI_UNSIGNED_CHAR, procID - 1, MSG_TAG, MPI_COMM_WORLD, &status);
            cntRecv++;
            
            if (status.MPI_TAG == MSG_TAG_EOF) {
                // If not the last process, forward the EOF to the next process
                MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_TAG_EOF, MPI_COMM_WORLD);
                DEBUG_PRINT("Process " << procID << " Final: " << static_cast<unsigned int>(num) << std::endl);
                break; // Exit the loop
            }

            // Save received data to particular queue
            if (recvQueue == Q_TOP) 
            {
                qTop.push(num);
            } 
            else 
            {
                qBottom.push(num);
            }

            if (cntRecv == procID)
            {
                cntRecv = 0;
                recvQueue = (recvQueue == Q_TOP) ? Q_BOTTOM : Q_TOP;
            }
            DEBUG_PRINT("P" << procID << ": Recv: " << static_cast<unsigned int>(num) << " To Q: " << recvQueue << std::endl);

            // ----------------------------------------------------------------------------
            
            // Init condition:
            //      Condition of minimum data in top queue and bottom queue
            //      Top queue needs to have minumum of it's index number and one item in the bottom queue
            if (newBatch)
            {
                initCond = (qTop.size() >= condNeededItemsInTQ && qBottom.size() >= MIN_ITEMS_IN_BOTTOM_QUEUE);
            }
            
            if (initCond)
            {
                newBatch = false;
                if (!qTop.empty() && !qBottom.empty())
                {
                    // Compare the first item of the top and bottom queue and send the bigger one
                    if (qTop.front() > qBottom.front())
                    {
                        num = qTop.front();
                        qTop.pop();
                        sentItemsT++;
                    }
                    else
                    {
                        num = qBottom.front();
                        qBottom.pop();
                        sentItemsB++;
                    }
                }
                // From bottom queue has been sent all items in batch, continue on sending from top queue
                else if (sentItemsB == procID && qTop.size() > 0)
                {
                    num = qTop.front();
                    qTop.pop();
                    sentItemsT++;
                }
                // From top queue has been sent all items in batch, continue on sending from bottom queue
                else if (sentItemsT == procID && qBottom.size() > 0)
                {
                    num = qBottom.front();
                    qBottom.pop();
                    sentItemsB++;
                }
                else if (sentItemsT == procID && sentItemsB == procID)
                {
                    sentItemsT = 0;
                    sentItemsB = 0;
                    initCond = false;
                    newBatch = true;
                }

                DEBUG_PRINT("P" << procID << ": Send: " << static_cast<unsigned int>(num) << " to proc ID: " << procID+1 <<  std::endl);
                MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_TAG, MPI_COMM_WORLD);
            }
        }
    }

    // Last process
    else 
    {
        uint8_t num;
        MPI_Status status;
        while (true) {
            MPI_Recv(&num, 1, MPI_UNSIGNED_CHAR, procID - 1, MSG_TAG, MPI_COMM_WORLD, &status);
            DEBUG_PRINT("P" << procID << ": Recv: " << static_cast<unsigned int>(num) << " To Q: " << recvQueue << std::endl);
            if (status.MPI_TAG == MSG_TAG_EOF) {
                break; // End
            }

            // The last process prints the final number
        }
    }

    MPI_Finalize();
    return 0;
}
