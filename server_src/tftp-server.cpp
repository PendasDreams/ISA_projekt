#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

const int MAX_BUFFER_SIZE = 516; // Maximum size of TFTP packets

// Structure for storing the client's address
struct ClientAddress
{
    sockaddr_in addr;
    socklen_t len;
};

// Function to send an ACK packet
void sendACK(int sockfd, const ClientAddress &clientAddr, int blockNum)
{
    char ackPacket[4];
    ackPacket[0] = 0; // Opcode: ACK
    ackPacket[1] = 4;
    ackPacket[2] = (blockNum >> 8) & 0xFF;
    ackPacket[3] = blockNum & 0xFF;

    sendto(sockfd, ackPacket, 4, 0, (struct sockaddr *)&clientAddr.addr, clientAddr.len);
}

// Function to send an error packet
void sendError(int sockfd, const ClientAddress &clientAddr, int errorCode, const string &errorMessage)
{
    char errorPacket[MAX_BUFFER_SIZE];
    errorPacket[0] = 0; // Opcode: ERROR
    errorPacket[1] = 5;
    errorPacket[2] = (errorCode >> 8) & 0xFF;
    errorPacket[3] = errorCode & 0xFF;
    strncpy(errorPacket + 4, errorMessage.c_str(), MAX_BUFFER_SIZE - 5);

    sendto(sockfd, errorPacket, errorMessage.length() + 5, 0, (struct sockaddr *)&clientAddr.addr, clientAddr.len);
}

// Function to process a Read Request (RRQ)
void handleRRQ(int sockfd, const ClientAddress &clientAddr, const string &filename)
{
    // Open the requested file
    ifstream file(filename, ios::binary);
    if (!file.is_open())
    {
        // File not found, send an error packet
        sendError(sockfd, clientAddr, 1, "File not found");
        return;
    }

    int blockNum = 1;
    char buffer[MAX_BUFFER_SIZE];

    while (true)
    {
        // Read data from the file
        file.read(buffer, MAX_BUFFER_SIZE - 4); // Leave space for opcode and block number
        streamsize bytesRead = file.gcount();

        if (bytesRead == 0)
        {
            // End of file reached
            break;
        }

        // Create and send a DATA packet
        char dataPacket[MAX_BUFFER_SIZE];
        dataPacket[0] = 0; // Opcode: DATA
        dataPacket[1] = 3;
        dataPacket[2] = (blockNum >> 8) & 0xFF;
        dataPacket[3] = blockNum & 0xFF;

        // Copy the data into the packet
        memcpy(dataPacket + 4, buffer, bytesRead);

        sendto(sockfd, dataPacket, bytesRead + 4, 0, (struct sockaddr *)&clientAddr.addr, clientAddr.len);

        // Wait for an ACK
        // ...
    }

    // Close the file
    file.close();
}

// Function to handle a Write Request (WRQ)
void handleWRQ(int sockfd, const ClientAddress &clientAddr, const string &filename)
{
    // Implement WRQ handling here
    // ...
}

int main()
{
    // Initialize the server socket and bind it to a port
    // ...

    while (true)
    {
        // Wait for a client request
        // ...

        // Determine if it's a RRQ or WRQ
        // if (isRRQ) {
        //     handleRRQ(sockfd, clientAddress, filename);
        // } else if (isWRQ) {
        //     handleWRQ(sockfd, clientAddress, filename);
        // }
    }

    return 0;
}
