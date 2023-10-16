#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr);

// TFTP opcodes
const uint16_t RRQ = 1;
const uint16_t WRQ = 2;
const uint16_t DATA = 3;
const uint16_t ACK = 4;
const uint16_t ERROR = 5;

// Maximum data packet size
const size_t MAX_DATA_SIZE = 514;

// TFTP Error Codes
const uint16_t ERROR_UNDEFINED = 0;
const uint16_t ERROR_FILE_NOT_FOUND = 1;
const uint16_t ERROR_ACCESS_VIOLATION = 2;
// ...

// Structure to represent a TFTP packet
struct TFTPPacket
{
    uint16_t opcode;
    char data[MAX_DATA_SIZE];
};

bool lastblockfromoutside = false;
// Function to send an error packet
void sendError(int sockfd, uint16_t errorCode, const std::string &errorMsg, sockaddr_in &clientAddr)
{
    TFTPPacket errorPacket;
    errorPacket.opcode = htons(ERROR);
    memcpy(errorPacket.data, &errorCode, sizeof(uint16_t));
    strcpy(errorPacket.data + sizeof(uint16_t), errorMsg.c_str());

    sendto(sockfd, &errorPacket, sizeof(uint16_t) + errorMsg.size() + 1, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
}

// Function to send a data packet
bool sendDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum, const char *data, size_t dataSize)
{
    TFTPPacket dataPacket;
    dataPacket.opcode = htons(DATA);
    uint16_t blockNumNetwork = htons(blockNum);
    memcpy(dataPacket.data, &blockNumNetwork, sizeof(uint16_t));
    memcpy(dataPacket.data + sizeof(uint16_t), data, dataSize);

    ssize_t sentBytes = sendto(sockfd, &dataPacket, sizeof(uint16_t) * 2 + dataSize, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    if (sentBytes == -1)
    {
        std::cerr << "Error sending DATA packet for block " << blockNum << std::endl;
        return false;
    }

    return true;
}

// Function to send a file in DATA packets
bool sendFileData(int sockfd, sockaddr_in &clientAddr, const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        sendError(sockfd, ERROR_FILE_NOT_FOUND, "File not found", clientAddr);
        return false;
    }

    std::cerr << "Sending file" << std::endl;

    char dataBuffer[MAX_DATA_SIZE];
    uint16_t blockNum = 1;

    while (true)
    {
        // Read data into the buffer
        file.read(dataBuffer, MAX_DATA_SIZE);
        std::streamsize bytesRead = file.gcount();

        std::cerr << "Bytes read: " << bytesRead << std::endl;

        if (bytesRead > 0)
        {
            std::cerr << "Data loaded into buffer" << std::endl;

            if (!sendDataPacket(sockfd, clientAddr, blockNum, dataBuffer, bytesRead))
            {
                file.close();
                return false;
            }

            // Wait for ACK for the sent block
            // You need to implement receiveAck function to handle ACKs
            if (!receiveAck(sockfd, blockNum, clientAddr))
            {
                file.close();
                return false;
            }

            if (bytesRead < MAX_DATA_SIZE)
            {
                // Reached the end of the file
                break;
            }

            blockNum++;
        }
        else
        {
            // No more data to send
            break;
        }
    }

    file.close();
    return true;
}

// Function to receive an ACK packet
bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr)
{
    TFTPPacket ackPacket;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (true)
    {
        ssize_t bytesReceived = recvfrom(sockfd, &ackPacket, sizeof(ackPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

        if (bytesReceived < 0)
        {
            std::cerr << "Error receiving ACK packet" << std::endl;
            return false;
        }

        if (bytesReceived < sizeof(uint16_t) * 2)
        {
            // Invalid ACK packet
            std::cerr << "Received an invalid ACK packet" << std::endl;
            return false;
        }

        uint16_t opcode = ntohs(ackPacket.opcode);
        if (opcode == ACK)
        {
            uint16_t blockNum = ntohs(*(uint16_t *)ackPacket.data);

            if (blockNum == expectedBlockNum)
            {
                // Received ACK for the expected block
                return true;
            }
            else if (blockNum < expectedBlockNum)
            {
                // Received a duplicate ACK (ignore)
                continue;
            }
            else
            {
                // Received an out-of-order ACK (unexpected)
                std::cerr << "Received an unexpected ACK for block " << blockNum << std::endl;
                return false;
            }
        }
        else
        {
            // Received an unexpected packet (not an ACK)
            std::cerr << "Received an unexpected packet with opcode " << opcode << std::endl;
            return false;
        }
    }
}

bool receiveDataPacket(int sockfd, TFTPPacket &dataPacket, sockaddr_in &clientAddr, uint16_t expectedBlockNum, std::ofstream &file)
{
    socklen_t clientAddrLen = sizeof(clientAddr);

    ssize_t bytesReceived = recvfrom(sockfd, &dataPacket, sizeof(dataPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

    if (bytesReceived < 0)
    {
        std::cerr << "Error receiving DATA packet" << std::endl;
        return false;
    }

    if (bytesReceived < sizeof(uint16_t) * 2)
    {
        // Invalid DATA packet
        std::cerr << "Received an invalid DATA packet" << std::endl;
        return false;
    }

    uint16_t opcode = ntohs(dataPacket.opcode);

    if (opcode == DATA)
    {
        uint16_t blockNum = ntohs(*(uint16_t *)dataPacket.data);

        if (blockNum == expectedBlockNum)
        {
            // Calculate the size of the data portion in this packet
            uint16_t dataSize = bytesReceived - sizeof(uint16_t) * 2;

            if (dataSize <= 0)
            {
                std::cerr << "Received an empty DATA packet for block " << blockNum << std::endl;
                return false;
            }

            // Write the data to the file
            file.write(dataPacket.data + sizeof(uint16_t), dataSize);

            if (dataSize < MAX_DATA_SIZE - 2)
            {
                lastblockfromoutside = true;
            }

            std::cout << "Received DATA packet for block " << blockNum << ", Data Size: " << dataSize << " bytes." << std::endl;

            return true;
        }
        else if (blockNum < expectedBlockNum)
        {
            // Received a duplicate DATA packet (ignore)
            std::cerr << "Received a duplicate DATA packet for block " << blockNum << std::endl;
            return false;
        }
        else
        {
            // Received an out-of-order DATA packet (unexpected)
            std::cerr << "Received an unexpected DATA packet for block " << blockNum << std::endl;
            return false;
        }
    }
    else
    {
        // Received an unexpected packet (not a DATA packet)
        std::cerr << "Received an unexpected packet with opcode " << opcode << std::endl;
        return false;
    }
}

bool sendAck(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum)
{
    TFTPPacket ackPacket;
    ackPacket.opcode = htons(ACK);
    uint16_t blockNumNetwork = htons(blockNum);
    memcpy(ackPacket.data, &blockNumNetwork, sizeof(uint16_t));

    ssize_t sentBytes = sendto(sockfd, &ackPacket, sizeof(uint16_t) * 2, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    if (sentBytes == -1)
    {
        std::cerr << "Error sending ACK packet for block " << blockNum << std::endl;
        return false;
    }

    return true;
}

// Main TFTP server function
void runTFTPServer(int port)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    struct sockaddr_in serverAddr, clientAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Error binding socket" << std::endl;
        close(sockfd);
        return;
    }

    while (true)
    {
        TFTPPacket requestPacket;
        socklen_t clientAddrLen = sizeof(clientAddr);

        ssize_t bytesReceived = recvfrom(sockfd, &requestPacket, sizeof(requestPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);
        std::cerr << "received shit" << std::endl;

        if (bytesReceived < 0)
        {
            std::cerr << "Error receiving packet" << std::endl;
            continue;
        }

        uint16_t opcode = ntohs(requestPacket.opcode);
        std::string filename(requestPacket.data);
        std::string mode(requestPacket.data + filename.size() + 1);

        if (opcode == RRQ)
        {
            // Print a message when RRQ is received
            std::cout << "Received RRQ for file: " << filename << " from client." << std::endl;

            std::ifstream file(filename, std::ios::binary);

            if (!file)
            {
                sendError(sockfd, ERROR_FILE_NOT_FOUND, "File not found", clientAddr);
            }
            else
            {
                if (!sendFileData(sockfd, clientAddr, filename))
                {
                    std::cerr << "Error sending file data" << std::endl;
                }
            }
        }
        else if (opcode == WRQ)
        {
            std::cout << "Received WRQ for file: " << filename << " from client." << std::endl;

            std::ofstream file(filename, std::ios::binary);

            if (!file)
            {
                sendError(sockfd, ERROR_ACCESS_VIOLATION, "Access violation", clientAddr);
            }
            else
            {
                // Send initial ACK (block number 0) to start the transfer
                if (!sendAck(sockfd, clientAddr, 0))
                {
                    std::cerr << "Error sending initial ACK" << std::endl;
                    file.close();
                    continue;
                }

                // Start receiving file data in DATA packets
                uint16_t blockNum = 1; // Initialize block number to 1
                bool lastBlockReceived = false;
                lastblockfromoutside = false;

                while (!lastBlockReceived)
                {
                    // Wait for DATA packet
                    TFTPPacket dataPacket;
                    if (receiveDataPacket(sockfd, dataPacket, clientAddr, blockNum, file))
                    {

                        // Send ACK for the received block
                        if (!sendAck(sockfd, clientAddr, blockNum))
                        {
                            std::cerr << "Error sending ACK for block " << blockNum << std::endl;
                            break;
                        }

                        // Check if the received block was the last block
                        if (dataPacket.opcode != htons(DATA) || lastblockfromoutside == true)
                        {
                            lastBlockReceived = true;
                        }
                        blockNum++;
                    }
                    else
                    {
                        // Error receiving data packet, break the loop
                        std::cerr << "Error receiving DATA packet for block " << blockNum << std::endl;
                        break;
                    }
                }

                file.close(); // Close the file when the transfer is complete or encounters an error
            }
        }

        else
        {
            sendError(sockfd, ERROR_UNDEFINED, "Undefined request", clientAddr);
        }
    }

    close(sockfd);
}

int main()
{
    int port = 1070; // Default TFTP port
    runTFTPServer(port);
    return 0;
}
