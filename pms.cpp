#include <iostream>
#include <fstream>
#include <mpi.h>
#include <queue>
#include <cmath>

#define MSG_TAG 0
#define MSG_FINAL 255

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
        num = MSG_FINAL;
        MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, 1, MSG_TAG, MPI_COMM_WORLD);
        DEBUG_PRINT("Process " << procID << " Final: " << static_cast<unsigned int>(num) << std::endl);
        
        inputFile.close();
    } 

    // Any other process
    else
    {
        bool initCond = false;
        bool newBatch = true;
        bool finish   = false;

        // Receive numbers from the previous process and forward them to the next process
        uint8_t num;
        uint8_t sentItemsB = 0;
        uint8_t sentItemsT = 0;

        MPI_Status status;

        // In process i need to have minimum of 2^(i-1) items in the TOP queue
        int condNeededItemsInTQ = pow (2, procID - 1); 
        QueuePosition recvQueue = Q_BOTTOM;
        uint8_t cntRecv = 0;

        int i = -1;


        while (true) {
            i++;
            DEBUG_PRINT("P" << procID << " ------------------ " << std::endl);
            // Receive data from the previous process
            if (!finish)
            {
                MPI_Recv(&num, 1, MPI_UNSIGNED_CHAR, procID - 1, MSG_TAG, MPI_COMM_WORLD, &status);
                if (++cntRecv == procID)
                {
                    cntRecv = 0;
                    recvQueue = (recvQueue == Q_TOP) ? Q_BOTTOM : Q_TOP;
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
                DEBUG_PRINT("P" << procID << " Id:" << i << " Recv: " << static_cast<unsigned int>(num) << " To Q: " << recvQueue  << " Status: " << qTop.size() << "|" << qBottom.size() <<  std::endl);
            }
            
            if (finish) {
                if (procID < noProc - 1)
                {
                    // If not the last process, forward the EOF to the next process
                    num = 255;
                    MPI_Send(&num, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_TAG, MPI_COMM_WORLD);
                    DEBUG_PRINT("Process " << procID << " Final: " << static_cast<unsigned int>(num) << " Status: " << status.MPI_TAG << std::endl);
                }
                else
                {
                    DEBUG_PRINT("P" << procID << ": Recv Final: " << static_cast<unsigned int>(num) << " To Q: " << recvQueue << std::endl);
                }
                break; // Exit the loop
            }

            // ----------------------------------------------------------------------------
            
            // Init condition:
            //      Condition of minimum data in top queue and bottom queue
            //      Top queue needs to have minumum of it's index number and one item in the bottom queue
            if (newBatch)
            {
                initCond = (qTop.size() >= condNeededItemsInTQ && qBottom.size() >= MIN_ITEMS_IN_BOTTOM_QUEUE);
            }
            
            if (initCond && !finish)
            {
                uint8_t sendNum;
                newBatch = false;
                
                // From bottom queue has been sent all items in batch, continue on sending from top queue
                if (sentItemsB == procID && qTop.size() > 0)
                {
                    if (qTop.front() == MSG_FINAL && qTop.size() == 1 && qBottom.size() == 0)
                    {
                        finish = true;
                         DEBUG_PRINT("Finish. sentT " << static_cast<unsigned int>(sentItemsT) << " sentB " << static_cast<unsigned int>(sentItemsB) << " Tsize " <<
                 qTop.size() << " Bsize " << qBottom.size() << std::endl);
                        continue;
                    }
                    else
                    {
                        sendNum = qTop.front();
                        qTop.pop();
                        sentItemsT++;
                    }
                }
                // From top queue has been sent all items in batch, continue on sending from bottom queue
                else if (sentItemsT == procID && qBottom.size() > 0)
                {
                    if (qBottom.front() == MSG_FINAL && qBottom.size() == 1 && qTop.size() == 0)
                    {
                        finish = true;
                         DEBUG_PRINT("Finish. sentT " << static_cast<unsigned int>(sentItemsT) << " sentB " << static_cast<unsigned int>(sentItemsB) << " Tsize " <<
                 qTop.size() << " Bsize " << qBottom.size() << std::endl);
                        continue;
                    }
                    else
                    {
                        sendNum = qBottom.front();
                        qBottom.pop();
                        sentItemsB++;
                    }
                }
                else if (!qTop.empty() && !qBottom.empty())
                {
                    // Compare the first item of the top and bottom queue and send the bigger one
                    if (qTop.front() > qBottom.front())
                    {
                        sendNum = qTop.front();
                        qTop.pop();
                        sentItemsT++;
                    }
                    else
                    {
                        sendNum = qBottom.front();
                        qBottom.pop();
                        sentItemsB++;
                    }
                }

               
                
                DEBUG_PRINT("OOF. sentT " << static_cast<unsigned int>(sentItemsT) << " sentB " << static_cast<unsigned int>(sentItemsB) << " Tsize " <<
                 qTop.size() << " Bsize " << qBottom.size() << std::endl);
                

                if (procID < noProc - 1)
                {
                    DEBUG_PRINT("P" << procID << ": Send: " << static_cast<unsigned int>(sendNum) << " to proc ID: " << procID+1 <<  std::endl);
                    MPI_Send(&sendNum, 1, MPI_UNSIGNED_CHAR, procID + 1, MSG_TAG, MPI_COMM_WORLD);
                }
                else
                {
                    DEBUG_PRINT("P" << procID << ": Final print: " << static_cast<unsigned int>(sendNum) << std::endl);
                }

                if (sentItemsT == procID && sentItemsB == procID)
                {
                    sentItemsT = 0;
                    sentItemsB = 0;
                    initCond = false;
                    newBatch = true;
                }
            }
        }
    }

   

    MPI_Finalize();
    return 0;
}
