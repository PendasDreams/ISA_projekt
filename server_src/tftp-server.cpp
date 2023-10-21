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

// Structure to represent a TFTP packet
struct TFTPPacket
{
    uint16_t opcode;
    char data[MAX_DATA_SIZE];
};

bool lastblockfromoutside = false;
bool blocksizeOptionUsed = false;

struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout;
    uint16_t transfersize;
};

// Function to send an error packet
void sendError(int sockfd, uint16_t errorCode, const std::string &errorMsg, sockaddr_in &clientAddr)
{
    TFTPPacket errorPacket;
    errorPacket.opcode = htons(ERROR);
    memcpy(errorPacket.data, &errorCode, sizeof(uint16_t));
    strcpy(errorPacket.data + sizeof(uint16_t), errorMsg.c_str());

    sendto(sockfd, &errorPacket, sizeof(uint16_t) + errorMsg.size() + 1, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
}

bool sendDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum, const char *data, size_t dataSize, uint16_t blockSize)
{
    std::cerr << "Send data packet " << std::endl;

    uint16_t opcode = htons(DATA);
    uint16_t blockNumNetwork = htons(blockNum);
    uint16_t blockSizeOption = htons(0x0004); // Blocksize Option

    size_t packetSize = sizeof(uint16_t) * 2 + dataSize;

    std::vector<uint8_t> dataPacket(packetSize);

    // Copy the opcode, block number, and data into the packet
    memcpy(dataPacket.data(), &opcode, sizeof(uint16_t));
    memcpy(dataPacket.data() + sizeof(uint16_t), &blockNumNetwork, sizeof(uint16_t));
    memcpy(dataPacket.data() + sizeof(uint16_t) * 2, data, dataSize);

    // Copy the block size option into the packet
    memcpy(dataPacket.data() + sizeof(uint16_t) * 2 + dataSize, &blockSizeOption, sizeof(uint16_t));

    ssize_t sentBytes = sendto(sockfd, dataPacket.data(), packetSize, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    if (sentBytes == -1)
    {
        std::cerr << "Error sending DATA packet for block " << blockNum << std::endl;
        return false;
    }

    return true;
}

// Function to send an OACK packet
bool sendOACK(int sockfd, sockaddr_in &clientAddr, std::map<std::string, int> &options_map, TFTPOparams &params)
{
    std::cerr << "Sending OACK packet" << std::endl;

    std::vector<uint8_t> oackBuffer;

    // Přidej opcode do vektoru
    uint16_t opcode = htons(OACK);
    oackBuffer.insert(oackBuffer.end(), reinterpret_cast<uint8_t *>(&opcode), reinterpret_cast<uint8_t *>(&opcode) + sizeof(uint16_t));

    if (blocksizeOptionUsed)
    {
        const char *optionName = "blksize";
        oackBuffer.insert(oackBuffer.end(), optionName, optionName + strlen(optionName) + 1); // Včetně nulového znaku

        // Přidej hodnotu volby (jako text) do vektoru
        std::string blockSizeStr = std::to_string(params.blksize);
        oackBuffer.insert(oackBuffer.end(), blockSizeStr.begin(), blockSizeStr.end());
        oackBuffer.push_back('\0'); // Přidej nulový znak za hodnotou
    }
    else
    {
        std::cout << "\"blksize\" not found in the map." << std::endl;
    }

    // Po dokončení vytvoření vektoru můžete poslat OACK packet
    ssize_t sentBytes = sendto(sockfd, oackBuffer.data(), oackBuffer.size(), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    // Check for errors
    if (sentBytes == -1)
    {
        std::cerr << "Error sending OACK packet" << std::endl;
        return false;
    }

    return true;
}

// Function to send a file in DATA packets
bool sendFileData(int sockfd, sockaddr_in &clientAddr, const std::string &filename, std::map<std::string, int> &options_map, TFTPOparams &params)
{
    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        sendError(sockfd, ERROR_FILE_NOT_FOUND, "File not found", clientAddr);
        return false;
    }

    std::cerr << "Sending file" << std::endl;

    auto it = options_map.find("blksize");

    // If "blksize" option is found in the map, use the specified block size
    if (blocksizeOptionUsed)
    {
        sendOACK(sockfd, clientAddr, options_map, params);
        if (!receiveAck(sockfd, 0, clientAddr))
        {
            file.close();
            return false;
        }
    }

    char dataBuffer[params.blksize]; // Use the outer dataBuffer with maximum size

    uint16_t blockNum = 1;

    while (true)
    {
        // Read data into the buffer
        file.read(dataBuffer, params.blksize); // Use the specified blockSize
        std::streamsize bytesRead = file.gcount();

        if (bytesRead > 0)
        {
            if (!sendDataPacket(sockfd, clientAddr, blockNum, dataBuffer, bytesRead, bytesRead))
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

            if (bytesRead < params.blksize)
            {
                std::cerr << "No more data to send" << std::endl;
                break;
            }

            blockNum++;
        }
        else
        {
            std::cerr << "No more data to send" << std::endl;
            break;
        }
    }

    options_map.clear();
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
                std::cerr << "Received an ACK packet" << std::endl;

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

bool receiveDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t expectedBlockNum, std::ofstream &file, TFTPOparams &params)
{
    std::vector<uint8_t> dataPacket;
    socklen_t clientAddrLen = sizeof(clientAddr);
    dataPacket.resize(params.blksize + 4, 0); // Inicializace vektoru nulami

    ssize_t bytesReceived = recvfrom(sockfd, dataPacket.data(), dataPacket.size(), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

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

    uint16_t opcode = ntohs(*reinterpret_cast<uint16_t *>(dataPacket.data()));

    if (opcode == DATA)
    {
        uint16_t blockNum = ntohs(*reinterpret_cast<uint16_t *>(dataPacket.data() + sizeof(uint16_t)));

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
            file.write(reinterpret_cast<const char *>(dataPacket.data() + sizeof(uint16_t) * 2), dataSize);

            if (dataSize < params.blksize - 2)
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

bool hasOptions(TFTPPacket &requestPacket, std::string &filename, std::string &mode, std::map<std::string, int> &options_map, TFTPOparams &params)
{

    int optionValue;
    std::string optionName;

    // Extract the filename
    char *packetData = requestPacket.data;

    // Find the null terminator after the filename
    size_t filenameLength = strlen(packetData);

    // Check if the null terminator is found and the filename is not empty
    if (filenameLength == 0 || filenameLength >= MAX_DATA_SIZE)
    {
        std::cerr << "Invalid filename in the request packet." << std::endl;
        return false;
    }

    filename = packetData;
    packetData += filenameLength + 1; // Move to the next byte after the filename

    // Extract the mode
    size_t modeLength = strlen(packetData);

    // Check if the mode is "octet" or "netascii"
    if (modeLength == 0 || modeLength >= MAX_DATA_SIZE)
    {
        std::cerr << "Unsupported transfer mode in the request packet." << std::endl;
        return false;
    }

    mode = packetData;

    // Move to the next byte after the mode
    packetData += modeLength + 1;

    // Check if there are options present
    while (packetData < requestPacket.data + sizeof(requestPacket.data))
    {
        // Extract the option name
        size_t optionNameLength = strnlen(packetData, MAX_DATA_SIZE);

        if (optionNameLength == 0)
        {
            break; // No more options
        }

        std::string optionName = packetData;
        packetData += optionNameLength + 1;

        // Extract the option value
        size_t optionValueLength = strnlen(packetData, MAX_DATA_SIZE);

        if (optionValueLength == 0)
        {
            std::cerr << "Malformed option in the request packet." << std::endl;
            return false;
        }

        std::string optionValueStr = packetData;
        packetData += optionValueLength + 1;

        try
        {
            int optionValue = std::stoi(optionValueStr);
            options_map[optionName] = optionValue;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid option value: " << optionValueStr << std::endl;
            return false;
        }
    }

    auto it = options_map.find("blksize");

    // If "blksize" option is found in the map, use the specified block size
    if (it != options_map.end())
    {
        params.blksize = it->second; // Read the value from the map
        std::cout << "Value for \"blksize\" is " << params.blksize << std::endl;
        blocksizeOptionUsed = true;
    }
    else
    {
        std::cout << "\"blksize\" not found in the map." << std::endl;
        return false;
    }

    // Options processed successfully
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
        TFTPOparams params;
        params.blksize = 512;
        params.timeout = 5;
        blocksizeOptionUsed = false;

        memset(&requestPacket, 0, sizeof(requestPacket));

        socklen_t clientAddrLen = sizeof(clientAddr);

        std::map<std::string, int> options_map;

        ssize_t bytesReceived = recvfrom(sockfd, &requestPacket, sizeof(requestPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);
        std::cerr << "received shit" << std::endl;

        if (bytesReceived < 0)
        {
            std::cerr << "Error receiving packet" << std::endl;
            continue;
        }

        uint16_t opcode = ntohs(requestPacket.opcode);
        std::string filename;
        std::string mode;

        if (opcode == RRQ)
        {
            // Print a message when RRQ is received

            // todo přidání víc options

            if (hasOptions(requestPacket, filename, mode, options_map, params))
            {
                std::cout << "Option included." << std::endl;
            }
            else
            {
                std::cout << "No options included in the RRQ packet." << std::endl;
            }

            std::ifstream file(filename, std::ios::binary);

            if (!file)
            {
                sendError(sockfd, ERROR_FILE_NOT_FOUND, "File not found", clientAddr);
            }
            else
            {
                if (!sendFileData(sockfd, clientAddr, filename, options_map, params))
                {
                    std::cerr << "Error sending file data" << std::endl;
                }
            }
        }
        else if (opcode == WRQ)
        {

            if (hasOptions(requestPacket, filename, mode, options_map, params))
            {
                std::cout << "Option included." << std::endl;
            }
            else
            {
                std::cout << "No options included in the RRQ packet." << std::endl;
            }

            std::cout << "Received WRQ for file: " << filename << " from client." << std::endl;

            std::ofstream file(filename, std::ios::binary);

            if (!file)
            {
                sendError(sockfd, ERROR_ACCESS_VIOLATION, "Access violation", clientAddr);
            }
            else
            {

                if (!options_map.empty())
                {

                    sendOACK(sockfd, clientAddr, options_map, params);
                }
                else
                {
                    if (!sendAck(sockfd, clientAddr, 0))
                    {
                        std::cerr << "Error sending initial ACK" << std::endl;
                        file.close();
                        continue;
                    }
                }

                // Start receiving file data in DATA packets
                uint16_t blockNum = 1; // Initialize block number to 1
                bool lastBlockReceived = false;
                lastblockfromoutside = false;

                while (!lastBlockReceived)
                {
                    // Wait for DATA packet

                    if (receiveDataPacket(sockfd, clientAddr, blockNum, file, params))
                    {

                        // Send ACK for the received block
                        if (!sendAck(sockfd, clientAddr, blockNum))
                        {
                            std::cerr << "Error sending ACK for block " << blockNum << std::endl;
                            break;
                        }

                        // Check if the received block was the last block
                        if (lastblockfromoutside == true)
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