/**
 * @file tftp_server.h
 * @brief TFTP client part of ISA project
 * @author xnovos14 - Denis Novosád
 */

#include "tftp-client.h"

bool isAscii(const std::string &fileName)
{
    // Find the file extension
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos == std::string::npos)
    {
        return false; // Invalid file name
    }

    std::string fileExtension = fileName.substr(dotPos + 1);

    // Compare the file extension with supported TFTP modes
    if (fileExtension == "txt" || fileExtension == "html" || fileExtension == "xml")
    {
        return true; // Text mode ('netascii')
    }
    else
    {
        return false; // Binary mode ('octet')
    }
}

std::string determineMode(const std::string &filePath)
{
    if (isAscii(filePath))
    {
        return "netascii"; // If it's ASCII data, use "netascii" mode
    }
    else
    {
        return "octet"; // If it's binary data, use "octet" mode
    }
}

bool setSocketTimeout(int sock, int timeout)
{
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        std::cout << "Error: Failed to set socket timeout." << std::endl;
        return false;
    }
    return true;
}

bool isBinaryFormat(const std::string &filename)
{
    // Get the file extension from the filename
    size_t dotPosition = filename.find_last_of('.');
    if (dotPosition == std::string::npos)
    {
        // If there is no file extension, we cannot determine the format, so use octet mode
        return true;
    }

    std::string fileExtension = filename.substr(dotPosition + 1);

    // Convert the file extension to lowercase for easier comparison
    std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::tolower);

    // Define a list of extensions to consider as binary formats
    std::vector<std::string> binaryExtensions = {"bin", "jpg", "png", "exe"};

    // Compare the file extension with the list of binary extensions
    return std::find(binaryExtensions.begin(), binaryExtensions.end(), fileExtension) != binaryExtensions.end();
}

void handleError(int sock, const std::string &hostname, int srcPort, int serverPort, uint16_t errorCode, const std::string &errorMsg)
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

    // Create sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    // Convert the hostname to an IP address and set it in serverAddr
    if (inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr)) <= 0)
    {
        std::cout << "Error: Failed to convert hostname to IP address." << std::endl;
    }

    // Send ERROR packet
    ssize_t sentBytes = sendto(sock, errorBuffer, sizeof(errorBuffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cout << "Error: Failed to send ERROR." << std::endl;
    }

    std::cerr << "ERROR " << hostname << ":" << srcPort << ":" << serverPort << " " << errorCode << " \"" << errorMsg << "\"" << std::endl;
}

bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort, TFTPOparams &params, std::map<std::string, std::string> &receivedOptions)
{
    // Create a buffer to receive the ACK or OACK packet
    uint8_t packetBuffer[516]; // Adjust the buffer size as needed for maximum possible packet size

    // Create sockaddr_in structure to store the sender's address
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    std::map<std::string, std::string> received_options;

    // Receive the ACK or OACK packet and capture the sender's address
    ssize_t receivedBytes = recvfrom(sock, packetBuffer, sizeof(packetBuffer), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
    if (receivedBytes == -1)
    {
        std::cout << "Error: Failed to receive packet." << std::endl;
        return false;
    }

    // Check if the received packet is an ACK or OACK packet
    if (receivedBytes < 4)
    {
        std::cout << "Error: Received packet is too short to be an ACK or OACK." << std::endl;

        return false;
    }

    uint16_t opcode = (packetBuffer[0] << 8) | packetBuffer[1];
    if (opcode != 4 && opcode != 6)
    {
        std::cout << "Error: Received packet is not an ACK or OACK." << std::endl;
        return false;
    }

    // Parse the received block ID from the ACK packet
    receivedBlockID = (packetBuffer[2] << 8) | packetBuffer[3];

    // Capture the server's port from the sender's address
    serverPort = ntohs(senderAddr.sin_port);

    if (opcode == 6)
    {
        receivedBlockID = 0;
        // This is an OACK packet, parse options
        size_t pos = 2;

        while (pos < receivedBytes)
        {
            std::string option;
            std::string value;

            // Read option until null terminator
            while (pos < receivedBytes && packetBuffer[pos] != 0)
            {
                option += (packetBuffer[pos]);
                pos++;
            }

            // Skip null terminator
            pos++;

            // Read value until null terminator
            while (pos < receivedBytes && packetBuffer[pos] != 0)
            {
                value += packetBuffer[pos];
                pos++;
            }

            // Skip null terminator
            pos++;

            // Store option and value in the receivedOptions map
            received_options[option] = value;
        }

        for (const auto &pair : received_options)
        {

            if (pair.first == "blksize")
            {
                int intblksize = std::stoi(pair.second);

                if (intblksize > params.blksize)
                {

                    std::cout << "Error: Received blksize option does not match the requested value." << std::endl;
                    std::cout << "Error: Requested value from server: " << pair.second << std::endl;
                    std::cout << "Error: Requested value from client: " << std::to_string(params.blksize) << std::endl;
                    return false;
                }
                else
                {
                    params.blksize = intblksize;
                }
            }
            else if (pair.first == "timeout" && (pair.second != std::to_string(params.timeout_max)))
            {
                std::cout << "Error: Received timeout option does not match the requested value." << std::endl;
                std::cout << "Error: Requested value from server: " << pair.second << std::endl;
                std::cout << "Error: Requested value from client: " << std::to_string(params.timeout_max) << std::endl;
                return false;
            }
            else if (pair.first == "tsize")
            {
                receivedOptions["tsize"] = received_options["tsize"];

                std::string tsize_str = received_options["tsize"];

                int tsize = std::stoi(tsize_str);

                params.transfersize = tsize;

                struct statvfs stat;
                if (statvfs("/", &stat) == 0)
                {
                    unsigned long long freeSpace = stat.f_frsize * stat.f_bfree;
                    if (freeSpace < params.transfersize)
                        std::cout << "Free space is: " << freeSpace / (1024 * 1024) << " MB "
                                  << "You need" << receivedOptions["tsize"] << std::endl;
                }
                else
                {
                    std::cout << "Error getting disk space information." << std::endl;
                    // TODO ERROR SEND
                }
            }
        }
    }

    std::cerr << (opcode == 4 ? "ACK" : "OACK") << " " << inet_ntoa(senderAddr.sin_addr) << ":" << ntohs(senderAddr.sin_port);

    if (opcode == 6)
    {
        for (const auto &pair : received_options)
        {
            std::cerr << " " << pair.first << "=" << pair.second;
        }
        std::cerr << std::endl;
    }
    else
    {
        std::cerr << std::endl;
    }

    return true;
}

bool sendData(int sock, const std::string &hostname, int port, const std::string &data)
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
        std::cout << "Error: Failed to send DATA." << std::endl;
        return false;
    }
    std::cout << std::dec;

    // std::cout << "Sent DATA packet with size: " << dataBuffer.size() << " bytes, block ID: " << blockID << std::endl; // Print the size and block ID

    return true;
}

int SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params)
{
    // Initialize variables and open the file for reading
    std::istream *inputStream;
    std::ifstream file;
    std::string userInput;

    // Read user input and handle file open errors
    std::cout << "Enter a line of text: ";
    std::getline(std::cin, userInput);
    std::cout << "You entered: " << userInput << std::endl;

    file.open(userInput, std::ios::binary);

    if (!file)
    {
        std::cout << "Error: Failed to open file for reading." << std::endl;
        handleError(sock, hostname, port, 0, ERROR_ACCESS_VIOLATION, "ERROR_ACCESS_VIOLATION");
        close(sock); // Close the socket on error
        return 1;
    }

    inputStream = &file;

    // Determine the transmission mode based on the file content
    mode = determineMode(remoteFilePath);

    const size_t maxDataSize = params.blksize; // Maximum data size in one DATA packet
    char buffer[maxDataSize];                  // Buffer for reading data from the file or stdin

    int serverPort = 0; // Variable to capture the server's port

    int writeRequestRetries = 0;
    bool wrqAckReceived = false;
    setSocketTimeout(sock, params.timeout_max);

    std::map<std::string, std::string> receivedOptions;

    // Send WRQ packet with optional parameters (OACK) and retry up to 4 times if no ACK/OACK is received
    while (!wrqAckReceived && writeRequestRetries < 4)
    {
        sendTFTPRequest(WRITE_REQUEST, sock, hostname, port, localFilePath, mode, params);

        wrqAckReceived = receiveAck(sock, blockID, serverPort, params, receivedOptions);

        if (!wrqAckReceived)
        {
            std::cout << "Warning: ACK not received after WRQ, retrying..." << std::endl;
            writeRequestRetries++;
        }
    }

    if (writeRequestRetries == 4)
    {
        std::cout << "Error: Failed to receive ACK or OACK after multiple WRQ attempts. Exiting..." << std::endl;
        handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
        close(sock);
        return 1;
    }

    bool isOACK = (blockID == 0);

    bool transferComplete = false;
    int max_retries = 4;

    long long totalSize;
    long long dataReceivedSoFar;
    double percentageReceived;

    bool lastnullpacket = false;
    int lastbytesread;

    if (option_tsize_used)
    {
        totalSize = params.transfersize; // Convert string to long long
    }

    while (!transferComplete)
    {
        inputStream->read(buffer, maxDataSize);
        std::streamsize bytesRead = inputStream->gcount();

        if (bytesRead > 0)
        {
            lastbytesread = bytesRead;

            if (!sendData(sock, hostname, serverPort, std::string(buffer, bytesRead)))
            {
                return 1;
            }; // Send data to the server

            // If the client doesn't receive an ACK in the required time, try resending the data packet
            int numRetries = 0;
            bool ackReceived = false;

            while (!ackReceived && numRetries < max_retries)
            {
                ackReceived = receiveAck(sock, blockID, serverPort, params, receivedOptions);

                if (!ackReceived)
                {
                    // If ACK wasn't received in time, retry sending the data packet
                    std::cout << "Warning: ACK not received for block " << blockID << ", retrying..." << std::endl;

                    if (!sendData(sock, hostname, serverPort, std::string(buffer, bytesRead)))
                    {
                        return 1;
                    }

                    numRetries++;
                }
            }

            if (!ackReceived)
            {
                // Data packet wasn't acknowledged by ACK even after multiple retries, exit the program
                std::cout << "Error: Data packet not acknowledged after multiple retries, exiting..." << std::endl;
                handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
                close(sock);
                return 1;
            }

            if (option_tsize_used)
            {
                // Extract the total size (tsize) from the receivedOptions map
                dataReceivedSoFar = blockID * params.blksize;

                // Calculate the percentage of data received
                percentageReceived = ((double)dataReceivedSoFar / totalSize) * 100;

                if (percentageReceived > 100)
                {
                    percentageReceived = 100;
                }

                // Print the percentage
                std::cout << "Send: " << percentageReceived << "% of total data." << std::endl;
            }

            if (bytesRead < maxDataSize) // If we read less than the max data size, we're done.
            {
                transferComplete = true;
            }
        }
        else
        {
            if (bytesRead == 0 && lastbytesread == params.blksize)
            {
                lastnullpacket = true; // Set to true if the last data is of size 0
            }
            // No more data to send
            transferComplete = true;
        }
    }

    if (lastnullpacket)
    {
        if (!sendData(sock, hostname, serverPort, ""))
        {
            return 1;
        }

        // Wait for acknowledgment for the last null DATA packet
        int numRetries = 0;
        bool ackReceived = false;

        while (!ackReceived && numRetries < max_retries)
        {
            ackReceived = receiveAck(sock, blockID, serverPort, params, receivedOptions);

            if (!ackReceived)
            {
                // If ACK wasn't received in time, retry sending the last null DATA packet
                std::cout << "Warning: ACK not received for the last null DATA packet, retrying..." << std::endl;

                if (!sendData(sock, hostname, serverPort, ""))
                {
                    return 1;
                }

                numRetries++;
            }
        }

        if (!ackReceived)
        {
            // Last null DATA packet wasn't acknowledged by ACK even after multiple retries, exit the program
            std::cout << "Error: Last null DATA packet not acknowledged after multiple retries, exiting..." << std::endl;
            handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
            close(sock);
            return 1;
        }
    }

    // Close the file handle if opened
    if (file.is_open())
    {
        file.close();
    }

    std::cout << "Upload file complete" << std::endl;

    // Close the socket
    close(sock);
}

bool sendTFTPRequest(TFTPRequestType requestType, int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, TFTPOparams &params)
{
    std::vector<uint8_t> requestBuffer;
    if (requestType == READ_REQUEST)
    {
        requestBuffer.push_back(0);
        requestBuffer.push_back(1); // Opcode for RRQ
    }
    else
    {
        requestBuffer.push_back(0);
        requestBuffer.push_back(2); // Opcode for WRQ
    }

    requestBuffer.insert(requestBuffer.end(), filepath.begin(), filepath.end());
    requestBuffer.push_back(0);
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0);

    // Append the blocksize option from params if blksize > 0
    if (option_blksize_used == true)
    {
        // Add "blksize" followed by a null terminator
        std::string blocksizeOption = "blksize";
        requestBuffer.insert(requestBuffer.end(), blocksizeOption.begin(), blocksizeOption.end());
        requestBuffer.push_back(0); // Null-terminate "blksize"

        // Add the value as a string followed by a null terminator
        std::string blockSizeValue = std::to_string(params.blksize);
        requestBuffer.insert(requestBuffer.end(), blockSizeValue.begin(), blockSizeValue.end());
        requestBuffer.push_back(0); // Null-terminate the value
    }

    if (option_timeout_used == true)
    {
        // Add "timeout" followed by a null terminator
        std::string timeoutOption = "timeout";
        requestBuffer.insert(requestBuffer.end(), timeoutOption.begin(), timeoutOption.end());
        requestBuffer.push_back(0); // Null-terminate "timeout"

        // Add the value as a string followed by a null terminator
        std::string timeoutValue = std::to_string(params.timeout_max);
        requestBuffer.insert(requestBuffer.end(), timeoutValue.begin(), timeoutValue.end());
        requestBuffer.push_back(0); // Null-terminate the value
    }

    if (option_tsize_used == true)
    {
        // Add "timeout" followed by a null terminator
        std::string transfersizeOption = "tsize";
        requestBuffer.insert(requestBuffer.end(), transfersizeOption.begin(), transfersizeOption.end());
        requestBuffer.push_back(0); // Null-terminate "timeout"

        // Add the value as a string followed by a null terminator
        std::string TransfersizeValue = std::to_string(params.transfersize);
        requestBuffer.insert(requestBuffer.end(), TransfersizeValue.begin(), TransfersizeValue.end());
        requestBuffer.push_back(0); // Null-terminate the value
    }

    // Create sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    // Send the RRQ packet
    ssize_t sentBytes = sendto(sock, requestBuffer.data(), requestBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cout << "Error: Failed to send " << (requestType == READ_REQUEST ? "RRQ" : "WRQ") << " packet." << std::endl;
        return false;
    }

    std::cerr << (requestType == READ_REQUEST ? "RRQ " : "WRQ ") << hostname << ":" << port << " \"" << filepath << "\" " << mode;
    if (option_timeout_used == true || option_blksize_used == true)
    {

        if (option_timeout_used == true)
        {
            std::cerr << " timeout=" << params.timeout_max;
        }
        if (option_blksize_used == true)
        {
            std::cerr << " blksize=" << params.blksize;
        }
        if (option_tsize_used == true)
        {
            std::cerr << " tsize=" << params.transfersize;
        }
    }
    std::cerr << std::endl;
}

bool receiveData(int sock, uint16_t &receivedBlockID, int &serverPort, std::string &data, TFTPOparams &params, const std::string &hostname)
{
    // Create a buffer to receive the DATA packet
    std::vector<uint8_t> dataBuffer(params.blksize + 4); // Max size of a DATA packet with room for header

    // Create sockaddr_in structure to store the sender's address
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    // Receive the DATA packet and capture the sender's address
    ssize_t receivedBytes = recvfrom(sock, dataBuffer.data(), dataBuffer.size(), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
    if (receivedBytes == -1)
    {
        std::cout << "Error: Failed to receive DATA." << std::endl;
        return false;
    }

    // Check if the received packet is a DATA packet
    if (receivedBytes < 4 || dataBuffer[0] != 0 || dataBuffer[1] != 3)
    {
        std::cout << "Error: Received packet is not a DATA packet." << std::endl;
        return false;
    }

    // Parse the received block ID from the DATA packet
    receivedBlockID = (dataBuffer[2] << 8) | dataBuffer[3];

    serverPort = ntohs(senderAddr.sin_port);

    // Extract the data from the packet (skip the first 4 bytes which are the header)
    data.assign(dataBuffer.begin() + 4, dataBuffer.begin() + receivedBytes);

    std::string srcIP = inet_ntoa(senderAddr.sin_addr);
    uint16_t srcPort = ntohs(senderAddr.sin_port);

    sockaddr_in localAddress;
    socklen_t addressLength = sizeof(localAddress);
    getsockname(sock, (struct sockaddr *)&localAddress, &addressLength);
    uint16_t dstPort = ntohs(localAddress.sin_port);

    // Print the desired format
    std::cerr << "DATA " << srcIP << ":" << srcPort << ":" << dstPort << " " << receivedBlockID << std::endl;

    // std::cout << "Sent DATA packet with size: " << dataBuffer.size() << " bytes, block ID: " << blockID << std::endl; // Print the size and block ID

    blockID = receivedBlockID + 1;

    return true;
}

bool sendAck(int sock, uint16_t blockID, const std::string &hostname, int serverPort, TFTPOparams &params)
{
    // Create an ACK packet
    std::vector<uint8_t> ackBuffer(4);
    ackBuffer[0] = 0;                     // High byte of opcode (0 for ACK)
    ackBuffer[1] = 4;                     // Low byte of opcode (4 for ACK)
    ackBuffer[2] = (blockID >> 8) & 0xFF; // High byte of block ID
    ackBuffer[3] = blockID & 0xFF;        // Low byte of block ID

    // Create sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    // Convert the hostname to an IP address and set it in serverAddr
    if (inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr)) <= 0)
    {
        std::cout << "Error: Failed to convert hostname to IP address." << std::endl;
        return false;
    }

    // Send the ACK packet to the server
    ssize_t sentBytes = sendto(sock, ackBuffer.data(), ackBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cout << "Error: Failed to send ACK." << std::endl;
        return false;
    }

    std::cout << "Sent ACK with block ID: " << blockID << " to server port: " << serverPort << std::endl;

    return true;
}

int receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params)
{
    mode = determineMode(remoteFilePath);

    std::ofstream outputFile(localFilePath, std::ios::binary | std::ios::out); // Open a local file to write the received data

    if (!outputFile.is_open())
    {
        std::cout << "Error: Failed to open file for writing." << std::endl;
        handleError(sock, hostname, port, 0, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
        close(sock); // Close the socket on error
        return 1;
    }

    std::cout << "File opened" << std::endl;

    // Send an RRQ packet to request the file from the server with options

    bool transferComplete = false;

    // Create sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;

    // Initialize serverPort and firstOACK outside the loop
    int serverPort = 0;
    bool firstOACK = false;

    long long totalSize;
    long long dataReceivedSoFar;
    double percentageReceived;

    int RequestRetries = 0;
    bool rrqAckReceived = false;
    bool firstDataRecv = false;

    setSocketTimeout(sock, params.timeout_max);

    uint16_t blockID = 0; // Initialize the block ID

    sendTFTPRequest(READ_REQUEST, sock, hostname, port, remoteFilePath, mode, params);

    while (!transferComplete)
    {
        uint16_t receivedBlockID;
        std::string data;
        std::map<std::string, std::string> receivedOptions;

        if (options_used == true && firstOACK == false)
        {

            firstOACK = true;

            while (!rrqAckReceived && RequestRetries < 1)
            {

                rrqAckReceived = receiveAck(sock, blockID, serverPort, params, receivedOptions);

                if (!rrqAckReceived)
                {
                    std::cout << "RequestRetries:" << RequestRetries << std::endl;

                    sendTFTPRequest(READ_REQUEST, sock, hostname, port, remoteFilePath, mode, params);
                    std::cout << "Warning: ACK not received after RRQ, retrying..." << std::endl;
                    RequestRetries++;
                }
            }

            if (RequestRetries == 1)
            {
                std::cout << "Error: Failed to receive ACK or OACK after multiple WRQ attempts. Exiting..." << std::endl;
                handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
                close(sock);
                return 1;
            }

            // Set the server's port and IP address in serverAddr
            serverAddr.sin_port = htons(serverPort);
            inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));
            if (option_tsize_used)
            {
                totalSize = std::stoll(receivedOptions["tsize"]); // Convert string to long long
            }
        }
        //
        if (options_used)
        {

            // Send an ACK packet before receiving DATA
            if (!sendAck(sock, blockID, hostname, serverPort, params))
            {
                std::cout << "Error: Failed to send ACK before receiving DATA." << std::endl;
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
                return 1;
            }

            int numRetriesRecvData = 0;
            bool dataReceived = false;

            while (!dataReceived && numRetriesRecvData < 4)
            {

                dataReceived = receiveData(sock, receivedBlockID, serverPort, data, params, hostname);
                if (!dataReceived)
                {
                    // Pokud ACK nebyl přijat včas, pokusit se znovu odeslat datový paket
                    std::cout << "Warning: DATA not received for block " << blockID << ", retrying..." << std::endl;

                    if (!sendAck(sock, blockID, hostname, serverPort, params))
                    {
                        std::cout << "Error: Failed to send ACK before receiving DATA." << std::endl;
                        close(sock);                   // Close the socket on error
                        outputFile.close();            // Close the output file
                        remove(localFilePath.c_str()); // Delete the partially downloaded file
                        return 1;
                    };

                    numRetriesRecvData++;
                }
            }

            if (!dataReceived)
            {
                // Datový paket nebyl potvrzen ACK ani po opakovaných pokusech, ukončit program
                handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
                return 1;
                std::cout << "Error: Data packet not acknowledged after multiple retries, exiting..." << std::endl;

                return 1;
            }

            if (option_tsize_used)
            {
                // Extract the total size (tsize) from the receivedOptions map

                dataReceivedSoFar = receivedBlockID * params.blksize;

                // Calculate the percentage of data received
                percentageReceived = ((double)dataReceivedSoFar / totalSize) * 100;

                if (percentageReceived >= 100)
                {
                    percentageReceived = 100;
                }

                // Print the percentage
                std::cout << "Received: " << percentageReceived << "% of total data." << std::endl;
            }
        }
        else
        {
            int numRetriesRecvData = 0;
            bool dataReceived = false;

            // Set the server's port and IP address in serverAddr

            while (!dataReceived && numRetriesRecvData < 4)
            {

                dataReceived = receiveData(sock, receivedBlockID, serverPort, data, params, hostname);
                if (!dataReceived && firstDataRecv == true)
                {

                    // Pokud ACK nebyl přijat včas, pokusit se znovu odeslat datový paket
                    std::cout << "Warning: DATA not received for block " << blockID << ", retrying..." << std::endl;

                    if (!sendAck(sock, blockID + 1, hostname, serverPort, params))
                    {
                        std::cout << "Error: Failed to send ACK before receiving DATA." << std::endl;
                        handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
                        close(sock);                   // Close the socket on error
                        outputFile.close();            // Close the output file
                        remove(localFilePath.c_str()); // Delete the partially downloaded file
                        return 1;
                    };
                }
                if (!dataReceived && firstDataRecv == false)
                {
                    sendTFTPRequest(READ_REQUEST, sock, hostname, port, remoteFilePath, mode, params);
                }
                if (numRetriesRecvData == 4)
                {
                    std::cout << "Error: Failed to receive ACK or OACK after multiple WRQ attempts. Exiting..." << std::endl;
                    handleError(sock, hostname, port, serverPort, ERROR_UNDEFINED, "Failed to receive ACK or OACK after WRQ");
                    close(sock);
                }
                numRetriesRecvData++;
            }
            firstDataRecv = true;

            serverAddr.sin_port = htons(serverPort);
            inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

            if (!dataReceived)
            {
                // Datový paket nebyl potvrzen ACK ani po opakovaných pokusech, ukončit program
                std::cout << "Error: Failed to receive DATA." << std::endl;
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
                std::cout << "Error: Data packet not acknowledged after multiple retries, exiting..." << std::endl;
                return 1;
            }

            // Send an ACK packet before receiving DATA
            if (!sendAck(sock, blockID + 1, hostname, serverPort, params))
            {
                std::cout << "Error: Failed to send ACK before receiving DATA." << std::endl;
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
                return 1;
            }
        }

        // Check if the received block ID is the expected one
        if (receivedBlockID != blockID + 1)
        {
            std::cout << "Error: Received out-of-order block ID." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return 1;
        }

        // Write the received data to the output file
        if (!outputFile.write(data.data(), data.size()))
        {
            std::cout << "Error: Failed to write data to the file." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return 1;
        }

        // Increment the block ID for the next ACK
        blockID = receivedBlockID;

        if (data.size() < params.blksize)
        {

            if (options_used == true)
            {
                if (!sendAck(sock, blockID, hostname, serverPort, params))
                {
                    std::cout << "Error: Failed to send ACK before receiving DATA." << std::endl;
                    close(sock);                   // Close the socket on error
                    outputFile.close();            // Close the output file
                    remove(localFilePath.c_str()); // Delete the partially downloaded file
                    return 1;
                }
            }
            transferComplete = true;
        }
    }

    // Close the output file
    outputFile.close();

    // Close the socket
    close(sock);

    std::cout << "File download complete: " << localFilePath << std::endl;
}

bool parseTFTPParameters(const std::string &Oparamstring, TFTPOparams &Oparams)
{

    // Split the string parameters into individual name-value pairs
    std::istringstream Oparamstream(Oparamstring);
    std::string paramName;
    std::string paramValue;

    // Split based on space
    Oparamstream >> paramName >> paramValue;

    if (!paramName.empty() && !paramValue.empty())
    {
        if (paramName == "blksize" || paramName == "BLKSIZE")
        {
            option_blksize_used = true;

            int blksize = std::stoi(paramValue);
            if (blksize >= 8 && blksize <= 65464) // Check valid range for blksize
            {
                Oparams.blksize = blksize;
            }
            else
            {
                std::cout << "Chybná hodnota parametru blksize: " << blksize << std::endl;
                return false;
            }
        }
        else if (paramName == "timeout" || paramName == "TIMEOUT")
        {
            option_timeout_used = true;

            int maxTimeout = std::stoi(paramValue);
            if (maxTimeout >= 0)
            {
                Oparams.timeout_max = maxTimeout;
            }
            else
            {
                std::cout << "Chybná hodnota parametru timeout: " << maxTimeout << std::endl;
                return false;
            }
        }
        else if (paramName == "tsize" || paramName == "TSIZE")
        {
            option_tsize_used = true;

            int transfersize = std::stoi(paramValue);
            if (transfersize >= 0)
            {
                Oparams.transfersize = transfersize;
            }
            else
            {
                std::cout << "Chybná hodnota parametru tsize: " << transfersize << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "Neznámý parametr: " << paramName << std::endl;
            return false;
        }
    }
    else
    {
        std::cout << "Nesprávný formát parametrů." << std::endl;
        return false;
    }

    options_used = true;
    return true;
}

int main(int argc, char *argv[])
{
    std::string hostname;
    int port = 69; // Default port for TFTP
    std::string localFilePath;
    std::string remoteFilePath;
    std::string options; // Optional parameters for OACK

    TFTPOparams Oparams;

    // Inicializace parametrů na výchozí hodnoty
    Oparams.blksize = 512;    // Výchozí hodnota blksize
    Oparams.timeout_max = 5;  // Výchozí hodnota timeout_max
    Oparams.transfersize = 0; // Výchozí hodnota transfersize

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
            remoteFilePath = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc)
        {
            localFilePath = argv[++i];
        }
        else if (arg == "--option" && i + 1 < argc)
        {
            if (!parseTFTPParameters(argv[++i], Oparams))
            {
                std::cout << "Chyba při parsování parametrů." << std::endl;
                return 1;
            }
        }
    }

    if (hostname.empty() || localFilePath.empty())
    {
        std::cout << "Usage: tftp-client -h hostname [-p port] [-f filepath] -t dest_filepath [--option]" << std::endl;
        return 1;
    }

    // Determine the mode based on the file content
    std::string mode = "octet";

    // Create a UDP socket for communication
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        std::cout << "Error: Failed to create socket." << std::endl;
        return 1;
    }

    // Set up the sockaddr_in structure for the server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    if (remoteFilePath.empty())
    {
        if (SendFile(sock, hostname, port, localFilePath, remoteFilePath, mode, options, Oparams) == 1)
        {
            return 1;
        };
    }
    else if (!localFilePath.empty() && !remoteFilePath.empty())
    {
        // Receive a file from the server
        if (receive_file(sock, hostname, port, localFilePath, remoteFilePath, mode, options, Oparams) == 1)
        {
            return 1;
        };
    }
    else
    {
        std::cout << "Error: You must specify either -f or -t option to send or receive a file, respectively." << std::endl;
        close(sock);
        return 1;
    }

    return 0;
}