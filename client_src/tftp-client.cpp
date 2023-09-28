#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>

// Define the initial block ID
uint16_t blockID = 0;

// Function to send WRQ (Write Request) packet with optional parameters (OACK)
void sendWriteRequestWithOACK(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, const std::string &options)
{
    // Create a buffer for the write request packet
    std::vector<uint8_t> requestBuffer;
    requestBuffer.push_back(0); // High byte of opcode (0 for WRQ)
    requestBuffer.push_back(2); // Low byte of opcode (2 for WRQ)
    requestBuffer.insert(requestBuffer.end(), filepath.begin(), filepath.end());
    requestBuffer.push_back(0); // Null-terminate the filepath
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0); // Null-terminate the mode

    // Append optional parameters (OACK) if provided
    if (!options.empty())
    {
        requestBuffer.insert(requestBuffer.end(), options.begin(), options.end());
        requestBuffer.push_back(0); // Null-terminate the options
    }

    // Create a sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send the write request packet
    ssize_t sentBytes = sendto(sock, requestBuffer.data(), requestBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send WRQ packet." << std::endl;
        return;
    }

    std::cerr << "Write Request " << hostname << ":" << port << " \"" << filepath << "\" " << mode;
    if (!options.empty())
    {
        std::cerr << " with options: " << options;
    }
    std::cerr << std::endl;
}

// Function to send RRQ (Read Request) packet with optional parameters (OACK)
void sendReadRequestWithOACK(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, const std::string &options)
{
    // Create a buffer for the read request packet
    std::vector<uint8_t> requestBuffer;
    requestBuffer.push_back(0); // High byte of opcode (1 for RRQ)
    requestBuffer.push_back(1); // Low byte of opcode (1 for RRQ)
    requestBuffer.insert(requestBuffer.end(), filepath.begin(), filepath.end());
    requestBuffer.push_back(0); // Null-terminate the filepath
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0); // Null-terminate the mode

    // Append optional parameters (OACK) if provided
    if (!options.empty())
    {
        requestBuffer.insert(requestBuffer.end(), options.begin(), options.end());
        requestBuffer.push_back(0); // Null-terminate the options
    }

    // Create a sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send the read request packet
    ssize_t sentBytes = sendto(sock, requestBuffer.data(), requestBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send RRQ packet." << std::endl;
        return;
    }

    std::cerr << "Read Request " << hostname << ":" << port << " \"" << filepath << "\" " << mode;
    if (!options.empty())
    {
        std::cerr << " with options: " << options;
    }
    std::cerr << std::endl;
}

// Function to receive ACK (Acknowledgment) packet and capture the server's port
bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort)
{
    // Create a buffer to receive the ACK packet
    uint8_t ackBuffer[4];

    // Create sockaddr_in structure to store the sender's address
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    // Receive the ACK packet and capture the sender's address
    ssize_t receivedBytes = recvfrom(sock, ackBuffer, sizeof(ackBuffer), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
    if (receivedBytes == -1)
    {
        std::cerr << "Error: Failed to receive ACK." << std::endl;
        return false;
    }

    // Check if the received packet is an ACK packet
    if (receivedBytes != 4 || ackBuffer[0] != 0 || ackBuffer[1] != 4)
    {
        std::cerr << "Error: Received packet is not an ACK." << std::endl;
        return false;
    }

    // Parse the received block ID from the ACK packet
    receivedBlockID = (ackBuffer[2] << 8) | ackBuffer[3];

    // Capture the server's port from the sender's address
    serverPort = ntohs(senderAddr.sin_port);

    std::cerr << "Received ACK with block ID: " << receivedBlockID << " from server port: " << serverPort << std::endl;

    return true;
}

// Function to send DATA packet
void sendData(int sock, const std::string &hostname, int port, const std::string &data)
{
    // Increment the block ID for the next data block
    blockID++;

    // Create a buffer for the DATA packet
    std::vector<uint8_t> dataBuffer;
    dataBuffer.push_back(0);                     // High byte of opcode (0 for DATA)
    dataBuffer.push_back(3);                     // Low byte of opcode (3 for DATA)
    dataBuffer.push_back((blockID >> 8) & 0xFF); // High byte of block ID
    dataBuffer.push_back(blockID & 0xFF);        // Low byte of block ID
    dataBuffer.insert(dataBuffer.end(), data.begin(), data.end());

    // Create sockaddr_in structure for the remote server (serverAddr)
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send DATA packet
    ssize_t sentBytes = sendto(sock, dataBuffer.data(), dataBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send DATA." << std::endl;
    }

    std::cerr << "Sent DATA packet with block ID: " << blockID << std::endl; // Print the opcode

    // If you want to print the actual data being sent, you can do so here
    std::cerr << "DATA Content: " << std::string(dataBuffer.begin() + 4, dataBuffer.end()) << std::endl;
}

// Function to receive DATA packet and send ACK packet
bool receiveData(int sock, uint16_t &receivedBlockID, std::string &data, int &serverPort)
{
    // Create a buffer to receive the DATA packet
    std::vector<uint8_t> dataBuffer(516); // Maximum size of a DATA packet is 516 bytes

    // Create sockaddr_in structure to store the sender's address
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    // Receive the DATA packet and capture the sender's address
    ssize_t receivedBytes = recvfrom(sock, dataBuffer.data(), dataBuffer.size(), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
    if (receivedBytes == -1)
    {
        std::cerr << "Error: Failed to receive DATA." << std::endl;
        return false;
    }

    // Check if the received packet is a DATA packet
    if (receivedBytes < 4 || dataBuffer[0] != 0 || dataBuffer[1] != 3)
    {
        std::cerr << "Error: Received packet is not a DATA packet." << std::endl;
        return false;
    }

    // Parse the received block ID from the DATA packet
    receivedBlockID = (dataBuffer[2] << 8) | dataBuffer[3];

    // Capture the server's port from the sender's address
    serverPort = ntohs(senderAddr.sin_port);

    // Extract the data from the DATA packet
    data.assign(dataBuffer.begin() + 4, dataBuffer.begin() + receivedBytes);

    std::cerr << "Received DATA with block ID: " << receivedBlockID << " from server port: " << serverPort << std::endl;

    // Send ACK for the received block
    std::vector<uint8_t> ackBuffer(4);
    ackBuffer[0] = 0;                             // High byte of opcode (0 for ACK)
    ackBuffer[1] = 4;                             // Low byte of opcode (4 for ACK)
    ackBuffer[2] = (receivedBlockID >> 8) & 0xFF; // High byte of block ID
    ackBuffer[3] = receivedBlockID & 0xFF;        // Low byte of block ID

    // Send ACK packet
    ssize_t sentBytes = sendto(sock, ackBuffer.data(), ackBuffer.size(), 0, (struct sockaddr *)&senderAddr, sizeof(senderAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send ACK." << std::endl;
        return false;
    }

    std::cerr << "Sent ACK for block ID: " << receivedBlockID << std::endl;

    return true;
}

// Function to handle errors
void handleError(int sock, const std::string &hostname, int srcPort, int dstPort, uint16_t errorCode, const std::string &errorMsg)
{
    // Create an ERROR packet
    uint8_t errorBuffer[4 + errorMsg.length() + 1]; // +1 for null-terminated string
    errorBuffer[0] = 0;                             // High byte of opcode (0 for ERROR)
    errorBuffer[1] = 5;                             // Low byte of opcode (5 for ERROR)
    errorBuffer[2] = (errorCode >> 8) & 0xFF;       // High byte of error code
    errorBuffer[3] = errorCode & 0xFF;              // Low byte of error code

    // Add the error message to the buffer
    std::memcpy(errorBuffer + 4, errorMsg.c_str(), errorMsg.length());
    errorBuffer[4 + errorMsg.length()] = '\0'; // Null-terminated string

    // Create sockaddr_in structure for the remote server (serverAddr)
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(dstPort);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send ERROR packet
    ssize_t sentBytes = sendto(sock, errorBuffer, sizeof(errorBuffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send ERROR." << std::endl;
    }

    std::cerr << "ERROR " << hostname << ":" << srcPort << ":" << dstPort << " " << errorCode << " \"" << errorMsg << "\"" << std::endl;
}

// Function to send a file to the server
void sendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options)
{
    std::ifstream file(localFilePath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Failed to open file for reading." << std::endl;
        close(sock); // Close the socket on error
        return;
    }

    const size_t maxDataSize = 512; // Max data size in one DATA packet
    char buffer[maxDataSize];       // Buffer for reading data from the file

    int serverPort = 0; // Variable to capture the server's port

    // Send WRQ packet with optional parameters (OACK)
    sendWriteRequestWithOACK(sock, hostname, port, remoteFilePath, mode, options);

    // Wait for an ACK or OACK response after WRQ and capture the server's port
    if (!receiveAck(sock, blockID, serverPort))
    {
        std::cerr << "Error: Failed to receive ACK or OACK after WRQ." << std::endl;
        handleError(sock, hostname, port, 0, 0, "Failed to receive ACK or OACK after WRQ");
        close(sock);
        return;
    }

    bool isOACK = (blockID == 0);

    std::cerr << "Received " << (isOACK ? "OACK" : "ACK") << " after WRQ. Starting data transfer." << std::endl;
    std::cerr << "Server provided port for data transfer: " << serverPort << std::endl; // Print the server's port

    bool transferComplete = false;

    while (!transferComplete)
    {
        file.read(buffer, maxDataSize);
        std::streamsize bytesRead = file.gcount();

        if (bytesRead > 0)
        {
            std::cerr << "Sending DATA packet with block ID: " << blockID + 1 << std::endl;
            sendData(sock, hostname, serverPort, std::string(buffer, bytesRead)); // Send data to the server's port
        }
        else
        {
            // No more data to send
            transferComplete = true;
        }

        if (bytesRead > 0)
        {
            // Wait for the ACK after sending DATA
            if (!isOACK && !receiveAck(sock, blockID, serverPort))
            {
                std::cerr << "Error: Failed to receive ACK for block " << blockID << std::endl;
                handleError(sock, hostname, serverPort, 0, 0, "Failed to receive ACK");
                break;
            }

            if (bytesRead < maxDataSize) // If we read less than the max data size, we're done.
            {
                transferComplete = true;
            }
        }
    }

    // Close the file handle
    file.close();

    // Close the socket
    close(sock);
}

// Function to receive a file from the server
void receiveFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options)
{
    std::ofstream file(localFilePath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Failed to open file for writing." << std::endl;
        close(sock); // Close the socket on error
        return;
    }

    int serverPort = 0; // Variable to capture the server's port

    // Send RRQ packet with optional parameters (OACK)
    sendReadRequestWithOACK(sock, hostname, port, remoteFilePath, mode, options);

    std::string receivedData; // Buffer to store received data
    uint16_t receivedBlockID = 0;
    bool transferComplete = false;

    while (!transferComplete)
    {
        // Receive DATA packet and send ACK
        if (!receiveData(sock, receivedBlockID, receivedData, serverPort))
        {
            std::cerr << "Error: Failed to receive DATA or send ACK for block " << receivedBlockID << std::endl;
            handleError(sock, hostname, port, serverPort, 0, "Failed to receive DATA or send ACK");
            break;
        }

        // Write received data to the file
        file.write(receivedData.c_str(), receivedData.size());

        // Check if this is the last block
        if (receivedData.size() < 512)
        {
            transferComplete = true;
        }
    }

    // Close the file handle
    file.close();

    // Close the socket
    close(sock);
}

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        std::cerr << "Usage: tftp-client -h hostname [-p port] -f filepath -t dest_filepath [-o options]" << std::endl;
        return 1;
    }

    std::string hostname;
    int port = 69; // Default port for TFTP
    std::string localFilePath;
    std::string remoteFilePath;
    std::string mode = "netascii"; // Default mode is "netascii"
    std::string options;           // Optional parameters for OACK

    // Process command-line arguments, including optional parameters
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc)
        {
            hostname = argv[++i];
        }
        else if (arg == "-p" && i + 1 < argc)
        {
            port = std::atoi(argv[++i]);
        }
        else if (arg == "-f" && i + 1 < argc)
        {
            localFilePath = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc)
        {
            remoteFilePath = argv[++i];
        }
        else if (arg == "-m" && i + 1 < argc)
        {
            mode = argv[++i]; // Set mode based on the provided argument
        }
        else if (arg == "-o" && i + 1 < argc)
        {
            options = argv[++i]; // Set optional parameters based on the provided argument
        }
    }

    // Create a UDP socket for communication
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        std::cerr << "Error: Failed to create socket." << std::endl;
        return 1;
    }

    // Set up the sockaddr_in structure for the server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send the file to the server
    sendFile(sock, hostname, port, localFilePath, remoteFilePath, mode, options);

    return 0;
}
