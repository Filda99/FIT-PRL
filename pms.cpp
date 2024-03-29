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

// Minimum number of items in the bottom queue
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

void processOthers(int procID, int noProc)
{
    std::deque<uint8_t> qTop, qBottom, finalNumbers;
    bool shouldIReceive = true, initCond = false, newBatch = true;
    uint8_t num, sendNum, sentItemsB = 0, sentItemsT = 0;
    QueuePosition recvQueue = Q_TOP;
    int condNeededItemsInTQ = pow(2, procID - 1), cntRecv = 0;
    MPI_Status status;

    auto removeElement = [&](std::deque<uint8_t> &deque, uint8_t &sentItemsCounter)
    {
        auto it = std::find(deque.begin(), deque.end(), sendNum);
        if (it != deque.end())
        {
            deque.erase(it); // Erase by iterator
            sentItemsCounter++;
        }
    };

    do
    {
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

                if (cntRecv == condNeededItemsInTQ)
                {
                    cntRecv = 0;
                    recvQueue = (recvQueue == Q_TOP) ? Q_BOTTOM : Q_TOP;
                }

                recvQueue == Q_TOP ? qTop.push_back(num) : qBottom.push_back(num);
                cntRecv++;
            }
        }

        if (newBatch)
        {
            initCond = (qTop.size() >= condNeededItemsInTQ && qBottom.size() >= MIN_ITEMS_IN_BOTTOM_QUEUE);
            initCond |= status.MPI_TAG;
        }

        if (initCond)
        {
            newBatch = false;

            if (!qTop.empty() || !qBottom.empty())
            {
                std::vector<std::pair<uint8_t, char>> elements; // Pair: value and origin ('T' or 'B')
               

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

                if (maxElement != elements.end())
                {
                    sendNum = maxElement->first;

                    // Remove the element from the correct deque
                    maxElement->second == 'T' ? removeElement(qTop, sentItemsT) : removeElement(qBottom, sentItemsB);
                }
            }

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

            if (sentItemsT == condNeededItemsInTQ && sentItemsB == condNeededItemsInTQ)
            {
                sentItemsT = 0;
                sentItemsB = 0;
                initCond = false;
                newBatch = true;
            }
        }
    } while (qTop.size() > 0 || qBottom.size() > 0);

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