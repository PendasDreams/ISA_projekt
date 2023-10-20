#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <map>
#include <iomanip> // Pro std::setw a std::setfill

bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr);

// TFTP opcodes
const uint16_t RRQ = 1;
const uint16_t WRQ = 2;
const uint16_t DATA = 3;
const uint16_t ACK = 4;
const uint16_t ERROR = 5;
const uint16_t OACK = 6;

// Maximum data packet size
const size_t MAX_DATA_SIZE = 514;

// TFTP Error Codes
const uint16_t ERROR_UNDEFINED = 0;
const uint16_t ERROR_FILE_NOT_FOUND = 1;
const uint16_t ERROR_ACCESS_VIOLATION = 2;
// ...

// Structure to represent a TFTP packet with Blocksize Option
struct TFTPPacket
{
    uint16_t opcode;
    char data[MAX_DATA_SIZE]; // Use a pointer for flexible data size
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
bool sendDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum, const char *data, size_t dataSize, uint16_t blockSize)
{
    // Calculate the total size needed for data, including opcode, options, and data
    size_t sizeOfDataPacket = sizeof(uint16_t) * 2 + dataSize; // For opcode, blockNum, and data

    // Create a TFTPPacket and set the opcode
    TFTPPacket dataPacket;
    dataPacket.opcode = htons(DATA);
    dataPacket.data = new char[sizeOfDataPacket];

    // Copy the opcode and blockNum into the dataPacket.data
    uint16_t opcodeNetworkOrder = htons(DATA);
    uint16_t blockNumNetwork = htons(blockNum);
    memcpy(dataPacket.data, &opcodeNetworkOrder, sizeof(uint16_t));
    memcpy(dataPacket.data + sizeof(uint16_t), &blockNumNetwork, sizeof(uint16_t));

    // Copy the data into dataPacket.data
    memcpy(dataPacket.data + sizeof(uint16_t) * 2, data, dataSize);

    // Send the entire packet
    ssize_t sentBytes = sendto(sockfd, dataPacket.data, sizeOfDataPacket, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    if (sentBytes == -1)
    {
        std::cerr << "Error sending DATA packet for block " << blockNum << std::endl;
        releaseTFTPPacketData(dataPacket);
        return false;
    }

    releaseTFTPPacketData(dataPacket); // Release the allocated memory for data

    return true;
}

// Function to send an OACK packet
bool sendOACK(int sockfd, sockaddr_in &clientAddr, const std::map<std::string, int> &options_map)
{
    int sizeOfDataPacket;
    std::string blocksizeOption = "blksize";

    // Calculate the total size needed for data, including opcode and options
    sizeOfDataPacket = sizeof(uint16_t); // For opcode
    auto it = options_map.find("blksize");
    if (it != options_map.end())
    {
        int blockSize = it->second;
        std::string blocksizeValue = std::to_string(blockSize);
        sizeOfDataPacket += blocksizeOption.size() + 1 + blocksizeValue.size() + 1; // +1 for null terminators
    }

    // Create a TFTPPacket and set the opcode
    TFTPPacket oackPacket;
    oackPacket.opcode = htons(OACK);

    // Allocate memory for the data and copy the opcode and options into it
    oackPacket.data = new char[sizeOfDataPacket];

    // Copy the opcode
    uint16_t opcodeNetworkOrder = htons(OACK);
    memcpy(oackPacket.data, &opcodeNetworkOrder, sizeof(uint16_t));

    // Prepare the options and copy them after the opcode
    if (it != options_map.end())
    {
        int blockSize = it->second;
        std::string blocksizeValue = std::to_string(blockSize);
        std::string optionsString = blocksizeOption + '\0' + blocksizeValue + '\0';
        memcpy(oackPacket.data + sizeof(uint16_t), optionsString.c_str(), optionsString.size());
    }

    // Send the entire packet
    ssize_t sentBytes = sendto(sockfd, oackPacket.data, sizeOfDataPacket, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    if (sentBytes == -1)
    {
        std::cerr << "Error sending OACK packet" << std::endl;
        releaseTFTPPacketData(oackPacket);
        return false;
    }

    releaseTFTPPacketData(oackPacket); // Release the allocated memory for data

    return true;
}

// Function to send a file in DATA packets with Blocksize Option
bool sendFileData(int sockfd, sockaddr_in &clientAddr, const std::string &filename, const std::map<std::string, int> &options_map)
{
    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        sendError(sockfd, ERROR_FILE_NOT_FOUND, "File not found", clientAddr);
        return false;
    }

    std::cerr << "Sending file" << std::endl;

    char dataBuffer[MAX_DATA_SIZE];
    int blockSize = 512; // Deklarace proměnné blockSize
    uint16_t blockNum = 1;

    // Zkontrolujte, zda se řetězec nachází v mapě
    auto it = options_map.find("blksize");

    // Pokud byl řetězec nalezen
    if (it != options_map.end())
    {
        blockSize = it->second; // Načtěte hodnotu do proměnné
        char dataBuffer[blockSize + 4];

        std::cout << "Hodnota pro "
                  << "blksize"
                  << " je " << blockSize << std::endl;
    }
    else
    {
        std::cout << "blksize"
                  << " nebylo nalezeno v mapě." << std::endl;
    }
    while (true)
    {
        // Read data into the buffer

        std::cout << "before read " << std::endl;

        file.read(dataBuffer, blockSize); // Read data according to Blocksize Option

        std::cout << "after read " << std::endl;

        std::streamsize bytesRead = file.gcount();

        std::cerr << "Bytes read: " << bytesRead << std::endl;

        if (bytesRead > 0)
        {
            std::cerr << "Data loaded into buffer" << std::endl;

            // Wait for ACK for the sent block
            // You need to implement receiveAck function to handle ACKs
            if (!receiveAck(sockfd, blockNum, clientAddr))
            {

                file.close();
                return false;
            }

            std::cerr << "Ack received" << std::endl;

            if (!sendDataPacket(sockfd, clientAddr, blockNum, dataBuffer, bytesRead, blockSize))
            {
                file.close();
                return false;
            }

            if (bytesRead < blockSize)
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
        requestPacket.data = new char[514];

        char requesPacket[514];

        socklen_t clientAddrLen = sizeof(clientAddr);

        ssize_t bytesReceived = recvfrom(sockfd, &requesPacket, sizeof(requesPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

        memcpy(&requestPacket.opcode, requesPacket, sizeof(uint16_t));

        std::cerr << "requestPacket.opcode: " << requestPacket.opcode << std::endl;

        memmove(requestPacket.data, requesPacket + 2, bytesReceived - 2);

        std::cerr << "werehere" << std::endl;

        if (bytesReceived < 0)
        {
            std::cerr << "Error receiving packet" << std::endl;
            releaseTFTPPacketData(requestPacket);
            continue;
        }

        // Vytisknout obsah celého přijatého packetu (včetně všech bytů a znaků)
        std::cout << "Received Packet (Hexadecimal): ";
        for (int i = 0; i < bytesReceived; i++)
        {
            printf("%02X ", static_cast<unsigned char>(requestPacket.data[i]));
        }
        std::cout << std::endl;

        std::cout << "Received Packet (Character): ";
        for (int i = 0; i < bytesReceived; i++)
        {
            std::cout << requestPacket.data[i];
        }
        std::cout << std::endl;

        uint16_t opcode = ntohs(requestPacket.opcode);
        std::string filename(requestPacket.data);
        std::string mode(requestPacket.data + filename.size() + 1);

        std::map<std::string, int> options_map;

        std::cout << "Filename: " << filename << std::endl;
        std::cout << "Mode: " << mode << std::endl;

        std::cout << "opcode: " << opcode << std::endl;

        if (opcode == RRQ)
        {
            // todo přidání víc options
            int blockSize = 512; // Deklarace proměnné blockSize

            std::cout << "wearehere: " << std::endl;

            std::string options(requestPacket.data + filename.size() + mode.size() + 2);

            std::cout << "options: " << options << std::endl;

            std::string options_val(requestPacket.data + filename.size() + mode.size() + options.size() + 3);

            int options_val_int = std::stoi(options_val);

            std::cout << "options value: " << options_val_int << std::endl;

            options_map[options] = options_val_int;

            // Zkontrolujte, zda se řetězec nachází v mapě
            auto it = options_map.find("blksize");

            // Pokud byl řetězec nalezen
            if (it != options_map.end())
            {
                blockSize = it->second; // Načtěte hodnotu do proměnné
                char dataBuffer[blockSize + 4];

                std::cout << "Hodnota pro "
                          << "blksize"
                          << " je " << blockSize << std::endl;

                if (!sendOACK(sockfd, clientAddr, options_map))
                {
                    std::cerr << "Error sending OACK packet" << std::endl;
                }
            }
            else
            {
                std::cout << "blksize"
                          << " nebylo nalezeno v mapě." << std::endl;
            }

            std::ifstream file(filename, std::ios::binary);

            if (!file)
            {
                sendError(sockfd, ERROR_FILE_NOT_FOUND, "File not found", clientAddr);
                releaseTFTPPacketData(requestPacket);
            }
            else
            {

                if (!sendFileData(sockfd, clientAddr, filename, options_map))
                {
                    std::cerr << "Error sending file data" << std::endl;
                    releaseTFTPPacketData(requestPacket);
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
                releaseTFTPPacketData(requestPacket);
            }
            else
            {
                // Send initial ACK (block number 0) to start the transfer
                if (!sendAck(sockfd, clientAddr, 0))
                {
                    std::cerr << "Error sending initial ACK" << std::endl;
                    file.close();
                    releaseTFTPPacketData(requestPacket);
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
                            releaseTFTPPacketData(requestPacket);

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
        releaseTFTPPacketData(requestPacket);
    }

    close(sockfd);
}

int main()
{
    int port = 1070; // Default TFTP port
    runTFTPServer(port);
    return 0;
}