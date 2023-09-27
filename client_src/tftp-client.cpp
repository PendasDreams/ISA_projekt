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

// Funkce pro odeslání RRQ (Read Request) nebo WRQ (Write Request)
void sendRequestPacket(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, uint16_t opcode)
{
    // Create a buffer for the request packet
    std::vector<uint8_t> requestBuffer;
    requestBuffer.push_back((opcode >> 8) & 0xFF); // High byte of opcode
    requestBuffer.push_back(opcode & 0xFF);        // Low byte of opcode
    requestBuffer.insert(requestBuffer.end(), filepath.begin(), filepath.end());
    requestBuffer.push_back(0); // Null-terminate the filepath
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0); // Null-terminate the mode

    // Create a sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send the request packet
    ssize_t sentBytes = sendto(sock, requestBuffer.data(), requestBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send request packet." << std::endl;
        return;
    }

    std::cerr << "Request " << hostname << ":" << port << " \"" << filepath << "\" " << mode << std::endl;
}

bool waitForAckWithTimeout(int sock, int seconds)
{
    fd_set fds;
    struct timeval timeout;
    int ret_val;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);

    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    ret_val = select(sock + 1, &fds, NULL, NULL, &timeout);

    return ret_val > 0;
}

// Funkce pro odeslání ACK (Acknowledgment)
void sendAck(int sock, const std::string &hostname, int port, uint16_t blockID)
{
    // Vytvoření ACK zprávy
    uint8_t ackBuffer[4];
    ackBuffer[0] = 0;                     // High byte of opcode (0 for ACK)
    ackBuffer[1] = 4;                     // Low byte of opcode (4 for ACK)
    ackBuffer[2] = (blockID >> 8) & 0xFF; // High byte of block ID
    ackBuffer[3] = blockID & 0xFF;        // Low byte of block ID

    // Vytvoření sockaddr_in struktury pro vzdálený server (serverAddr)
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Odeslání ACK zprávy
    ssize_t sentBytes = sendto(sock, ackBuffer, sizeof(ackBuffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send ACK." << std::endl;
        return;
    }

    // Výpis odeslané ACK zprávy do stderr
    std::cerr << "ACK " << hostname << ":" << port << " " << blockID << std::endl;
}

bool receiveAck(int sock, uint16_t expectedBlockID)
{

    if (!waitForAckWithTimeout(sock, 3)) // Wait for 3 seconds
    {
        std::cerr << "Timeout waiting for ACK." << std::endl;
        return false;
    }

    uint8_t ackBuffer[4];
    sockaddr_in serverAddr;
    socklen_t serverAddrLen = sizeof(serverAddr);

    ssize_t receivedBytes = recvfrom(sock, ackBuffer, sizeof(ackBuffer), 0, (struct sockaddr *)&serverAddr, &serverAddrLen);
    if (receivedBytes == -1)
    {
        std::cerr << "Error: Failed to receive ACK." << std::endl;
        return false;
    }

    if (ackBuffer[0] == 0 && ackBuffer[1] == 4)
    {
        uint16_t receivedBlockID = (ackBuffer[2] << 8) | ackBuffer[3];
        if (receivedBlockID == expectedBlockID)
        {
            std::cerr << "Received ACK with right block ID: " << receivedBlockID << std::endl;
            // No need to increment blockID here, as it should be incremented before sending the next DATA packet.
            return true;
        }
        else
        {
            // Print the received block ID when it's unexpected.
            std::cerr << "Error: Received ACK with unexpected block ID: received " << receivedBlockID << "  expected :" << expectedBlockID << std::endl;
        }
    }
    else
    {
        std::cerr << "Error: Received unexpected packet (not ACK)." << std::endl;
    }

    return false;
}

// Function to send DATA packet
void sendData(int sock, const std::string &hostname, int port, const std::string &data)
{
    // Increment the block ID before sending each DATA packet
    blockID++;

    // Create DATA packet
    uint8_t dataBuffer[4 + data.length()];
    dataBuffer[0] = 0;                     // High byte of opcode (0 for DATA)
    dataBuffer[1] = 3;                     // Low byte of opcode (3 for DATA)
    dataBuffer[2] = (blockID >> 8) & 0xFF; // High byte of block ID
    dataBuffer[3] = blockID & 0xFF;        // Low byte of block ID

    // Copy data into the buffer
    std::memcpy(dataBuffer + 4, data.c_str(), data.length());

    // Create server address structure
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send DATA packet
    ssize_t sentBytes = sendto(sock, dataBuffer, sizeof(dataBuffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send DATA." << std::endl;
        return;
    }

    std::cerr << "DATA " << hostname << ":" << port << " " << blockID << std::endl;

    // Increment the block ID for the next data block
}

// Funkce pro zpracování chybové zprávy
void handleError(int sock, const std::string &hostname, int srcPort, int dstPort, uint16_t errorCode, const std::string &errorMsg)
{
    // Vytvoření ERROR zprávy
    uint8_t errorBuffer[4 + errorMsg.length() + 1]; // +1 pro nulový byte ukončení řetězce
    errorBuffer[0] = 0;                             // High byte of opcode (0 for ERROR)
    errorBuffer[1] = 5;                             // Low byte of opcode (5 for ERROR)
    errorBuffer[2] = (errorCode >> 8) & 0xFF;       // High byte of error code
    errorBuffer[3] = errorCode & 0xFF;              // Low byte of error code

    // Přidání chybového textu do bufferu
    std::memcpy(errorBuffer + 4, errorMsg.c_str(), errorMsg.length());
    errorBuffer[4 + errorMsg.length()] = '\0'; // Nulový byte ukončení řetězce

    // Vytvoření sockaddr_in struktury pro vzdálený server (serverAddr)
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(dstPort);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Odeslání ERROR zprávy
    ssize_t sentBytes = sendto(sock, errorBuffer, sizeof(errorBuffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send ERROR." << std::endl;
        return;
    }

    // Výpis odeslané ERROR zprávy do stderr
    std::cerr << "ERROR " << hostname << ":" << srcPort << ":" << dstPort << " " << errorCode << " \"" << errorMsg << "\"" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        std::cerr << "Usage: tftp-client -h hostname [-p port] [-f filepath] -t dest_filepath" << std::endl;
        return 1;
    }

    std::string hostname;
    int port = 69; // Default port for TFTP
    std::string filepath;
    std::string destFilePath;
    std::string mode = "netascii"; // Default mode is "netascii"

    // Zpracování příkazového řádku
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
            filepath = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc)
        {
            destFilePath = argv[++i];
        }
        else if (arg == "-m" && i + 1 < argc)
        {
            mode = argv[++i]; // Nastavit mód na základě poskytnutého argumentu
        }
    }

    // Otevření souboru pro čtení
    std::ifstream file;

    if (filepath == "-")
    {
        // Use standard input (stdin)
        file.copyfmt(std::cin);
        file.clear();
        file.basic_ios<char>::rdbuf(std::cin.rdbuf());
    }
    else
    {
        file.open(filepath, std::ios::binary);
        if (!file)
        {
            std::cerr << "Error: Failed to open file for reading." << std::endl;
            return 1;
        }
    }

    // Otevření socketu pro komunikaci s vzdáleným serverem
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        std::cerr << "Error: Unable to create socket." << std::endl;
        return 1;
    }

    // Implement the logic to send WRQ (Write Request)
    sendRequestPacket(sock, hostname, port, destFilePath, mode, 2); // 2 means WRQ

    // Wait for the server's acknowledgment after sending the WRQ
    if (!receiveAck(sock, 0))
    {
        std::cerr << "Error: Did not receive initial acknowledgment from server after sending WRQ." << std::endl;
        return 1;
    }

    // Sending data from the file
    const size_t maxDataSize = 512; // Max data size in one DATA packet

    char buffer[maxDataSize]; // Buffer for reading data from the file

    bool transfer_complete = false;

    while (!transfer_complete)
    {
        file.read(buffer, maxDataSize);
        std::streamsize bytesRead = file.gcount();

        if (bytesRead > 0 && bytesRead <= maxDataSize)
        {
            std::cerr << "Sending DATA packet with block ID: " << blockID + 1 << std::endl;

            sendData(sock, hostname, port, std::string(buffer, bytesRead));

            if (!receiveAck(sock, blockID))
            {
                std::cerr << "Error: Failed to receive ACK for block " << blockID << std::endl;
                break;
            }

            if (bytesRead < maxDataSize) // If we read less than the max data size, we're done.
            {
                transfer_complete = true;
            }
        }
        else
        {
            if (bytesRead >= maxDataSize)
            {
                std::cerr << "Error: Data vetsi nez " << blockID << std::endl;
            }
            // No more data to send
            transfer_complete = true;
        }
    }

    // Close the socket
    close(sock);

    std::cout << "File transfer completed." << std::endl;

    return 0;
}