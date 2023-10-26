#include "tftp-client.h"

// Define the initial block ID
uint16_t blockID = 0;

bool options_used = false;
bool option_blksize_used = false;
bool option_timeout_used = false;
bool option_tsize_used = false;

// TFTP Error Codes
const uint16_t ERROR_UNDEFINED = 0;
const uint16_t ERROR_FILE_NOT_FOUND = 1;
const uint16_t ERROR_ACCESS_VIOLATION = 2;
const uint16_t ERROR_DISK_FULL = 3;
const uint16_t ERROR_ILLEGAL_OPERATION = 4;
const uint16_t ERROR_UNKNOWN_TRANSFER_ID = 5;
const uint16_t ERROR_FILE_ALREADY_EXISTS = 6;

bool isAscii(const std::string &fileName)
{
    // Zjistěte příponu souboru
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos == std::string::npos)
    {
        // std::cerr << "Invalid file name: " << fileName << std::endl;
        return false; // Neplatný název souboru
    }

    std::string fileExtension = fileName.substr(dotPos + 1);

    // Porovnejte příponu s podporovanými režimy TFTP
    if (fileExtension == "txt" || fileExtension == "html" || fileExtension == "xml")
    {
        // std::cerr << "file is ascii " << fileName << std::endl;

        return true; // Textový režim ('netascii')
    }
    else
    {
        // std::cerr << "file is octet " << fileName << std::endl;

        return false; // Binární režim ('octet')
    }
}

std::string determineMode(const std::string &filePath)
{

    if (isAscii(filePath))
    {
        return "netascii"; // Pokud jsou to ASCII data, použijte "netascii" režim
    }
    else
    {
        return "octet"; // Pokud jsou to binární data, použijte "octet" režim
    }
}

// Function to receive ACK (Acknowledgment) packet and capture the server's port

std::string hexStringToCharString(const std::string &hexStr)
{
    std::string charStr;
    for (size_t i = 0; i < hexStr.length(); i += 2)
    {
        // Extract two characters from the hex string
        std::string hexPair = hexStr.substr(i, 2);

        // Convert the hex pair to an integer
        int hexValue = std::stoi(hexPair, nullptr, 16);

        // Convert the integer to a char and append it to the result
        charStr += static_cast<char>(hexValue);
    }
    return charStr;
}

// Function to set socket timeout
bool setSocketTimeout(int sock, int timeout)
{
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        std::cerr << "Error: Failed to set socket timeout." << std::endl;
        return false;
    }
    return true;
}

bool isBinaryFormat(const std::string &filename)
{
    // Získej koncovku souboru z názvu
    size_t dotPosition = filename.find_last_of('.');
    if (dotPosition == std::string::npos)
    {
        // Pokud nemáme koncovku, nelze určit formát, takže použijeme octet mode
        return true;
    }

    std::string fileExtension = filename.substr(dotPosition + 1);

    // Převeď koncovku na malá písmena pro jednodušší porovnání
    std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::tolower);

    // Definuj seznam koncovek, které budeme považovat za binární formáty
    std::vector<std::string> binaryExtensions = {"bin", "jpg", "png", "exe"};

    // Porovnej koncovku s seznamem binárních koncovek
    return std::find(binaryExtensions.begin(), binaryExtensions.end(), fileExtension) != binaryExtensions.end();
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

// Function to send WRQ (Write Request) packet with optional parameters (OACK)
bool sendWriteRequest(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, const std::string &options, const TFTPOparams &params)
{
    // Create a buffer for the write request packet
    std::vector<uint8_t> requestBuffer;
    requestBuffer.push_back(0); // High byte of opcode (0 for WRQ)
    requestBuffer.push_back(2); // Low byte of opcode (2 for WRQ)
    requestBuffer.insert(requestBuffer.end(), filepath.begin(), filepath.end());
    requestBuffer.push_back(0); // Null-terminate the filepath
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0); // Null-terminate the mode

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
        // Add "blksize" followed by a null terminator
        std::string timeoutOption = "timeout";
        requestBuffer.insert(requestBuffer.end(), timeoutOption.begin(), timeoutOption.end());
        requestBuffer.push_back(0);

        // Add the value as a string followed by a null terminator
        std::string timeoutValue = std::to_string(params.timeout_max);
        requestBuffer.insert(requestBuffer.end(), timeoutValue.begin(), timeoutValue.end());
        requestBuffer.push_back(0);
    }

    if (option_tsize_used == true)
    {
        // Add "blksize" followed by a null terminator
        std::string transferSizeOption = "tsize";
        requestBuffer.insert(requestBuffer.end(), transferSizeOption.begin(), transferSizeOption.end());
        requestBuffer.push_back(0); // Null-terminate "blksize"

        // Add the value as a string followed by a null terminator

        std::string transersizeValue = std::to_string(params.transfersize);
        requestBuffer.insert(requestBuffer.end(), transersizeValue.begin(), transersizeValue.end());
        requestBuffer.push_back(0); // Null-terminate the value
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
        return false;
    }

    std::cerr << std::endl;

    std::cerr << "WRQ " << hostname << ":" << port << " \"" << filepath << "\" " << mode;
    if (option_timeout_used == true || option_blksize_used == true || option_tsize_used == true)
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
    return true;
}

bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort, const TFTPOparams &params, std::map<std::string, std::string> &receivedOptions)
{
    // Create a buffer to receive the ACK or OACK packet
    uint8_t packetBuffer[516]; // Adjust the buffer size as needed for maximum possible packet size

    // Create sockaddr_in structure to store the sender's address
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    std::map<std::string, std::string> recieved_options;

    // Receive the ACK or OACK packet and capture the sender's address
    ssize_t receivedBytes = recvfrom(sock, packetBuffer, sizeof(packetBuffer), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
    if (receivedBytes == -1)
    {
        std::cerr << "Error: Failed to receive packet." << std::endl;
        return false;
    }

    // Check if the received packet is an ACK or OACK packet
    if (receivedBytes < 4)
    {
        std::cerr << "Error: Received packet is too short to be an ACK or OACK." << std::endl;
        return false;
    }

    uint16_t opcode = (packetBuffer[0] << 8) | packetBuffer[1];
    if (opcode != 4 && opcode != 6)
    {
        std::cerr << "Error: Received packet is not an ACK or OACK." << std::endl;
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
            recieved_options[option] = value;
        }

        for (const auto &pair : recieved_options)
        {

            if (pair.first == "blksize" && (pair.second != std::to_string(params.blksize)))
            {
                std::cerr << "Error: Received blksize option does not match the requested value." << std::endl;
                std::cerr << "Error: Requested value from server: " << pair.second << std::endl;
                std::cerr << "Error: Requested value from client: " << std::to_string(params.blksize) << std::endl;
                return false;
            }
            else if (pair.first == "timeout" && (pair.second != std::to_string(params.timeout_max)))
            {
                std::cerr << "Error: Received timeout option does not match the requested value." << std::endl;
                std::cerr << "Error: Requested value from server: " << pair.second << std::endl;
                std::cerr << "Error: Requested value from client: " << std::to_string(params.timeout_max) << std::endl;
                return false;
            }
            else if (pair.first == "tsize")
            {
                receivedOptions["tsize"] = recieved_options["tsize"];
            }
        }
    }

    std::cerr << (opcode == 4 ? "ACK" : "OACK") << " " << inet_ntoa(senderAddr.sin_addr) << ":" << ntohs(senderAddr.sin_port);

    if (opcode == 6)
    {
        for (const auto &pair : recieved_options)
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

// Function to send DATA packet
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
        std::cerr << "Error: Failed to send DATA." << std::endl;
        return false;
    }
    std::cout << std::dec;

    // std::cerr << "Sent DATA packet with size: " << dataBuffer.size() << " bytes, block ID: " << blockID << std::endl; // Print the size and block ID

    return true;
}

// Function to send a file to the server or upload from stdin
int SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, const TFTPOparams &params)
{
    std::istream *inputStream;
    std::ifstream file;

    std::string userInput;
    std::cout << "Zadejte řádek textu: ";
    std::getline(std::cin, userInput);

    std::cout << "Zadali jste: " << userInput << std::endl;

    file.open(userInput, std::ios::binary);

    if (!file)
    {
        std::cerr << "Error: Failed to open file for reading sendfile." << std::endl;
        close(sock); // Close the socket on error
        return 1;
    }
    inputStream = &file;

    // Determine the mode based on the file content
    mode = determineMode(remoteFilePath);

    const size_t maxDataSize = params.blksize; // Max data size in one DATA packet
    char buffer[maxDataSize];                  // Buffer for reading data from the file or stdin

    int serverPort = 0; // Variable to capture the server's port

    int writeRequestRetries = 0;
    bool wrqAckReceived = false;
    setSocketTimeout(sock, params.timeout_max);

    std::map<std::string, std::string> receivedOptions;

    // Send WRQ packet with optional parameters (OACK) and retry up to 4 times if no ACK/OACK is received
    while (!wrqAckReceived && writeRequestRetries < 4)
    {
        if (!sendWriteRequest(sock, hostname, port, localFilePath, mode, options, params))
        {
            return 1;
        }

        wrqAckReceived = receiveAck(sock, blockID, serverPort, params, receivedOptions);

        if (!wrqAckReceived)
        {
            std::cerr << "Warning: ACK not received after WRQ, retrying..." << std::endl;
            writeRequestRetries++;
        }
    }

    if (writeRequestRetries == 4)
    {
        std::cerr << "Error: Failed to receive ACK or OACK after multiple WRQ attempts. Exiting..." << std::endl;
        close(sock);
        return 1;
    }

    bool isOACK = (blockID == 0);

    bool transferComplete = false;
    int max_retries = 4;

    long long totalSize;
    long long dataReceivedSoFar;
    double percentageReceived;

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

            if (!sendData(sock, hostname, serverPort, std::string(buffer, bytesRead)))
            {
                return 1;
            }; // Odeslat data na server

            // Pokud klient nepřijme ACK v požadovaném čase, pokusí se znovu odeslat datový paket
            int numRetries = 0;
            bool ackReceived = false;
            while (!ackReceived && numRetries < max_retries)
            {

                ackReceived = receiveAck(sock, blockID, serverPort, params, receivedOptions);
                if (!ackReceived)
                {
                    // Pokud ACK nebyl přijat včas, pokusit se znovu odeslat datový paket
                    std::cerr << "Warning: ACK not received for block " << blockID << ", retrying..." << std::endl;

                    if (!sendData(sock, hostname, serverPort, std::string(buffer, bytesRead)))
                    {
                        return 1;
                    };
                    numRetries++;
                }
            }

            if (!ackReceived)
            {
                // Datový paket nebyl potvrzen ACK ani po opakovaných pokusech, ukončit program
                std::cerr << "Error: Data packet not acknowledged after multiple retries, exiting..." << std::endl;
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

            // No more data to send
            transferComplete = true;
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

//****************************************************************************************************************************
//
//
// Recieving file
//
//
//****************************************************************************************************************************

// Function to send RRQ (Read Request) packet
void sendReadRequest(int sock, const std::string &hostname, int port, const std::string &remoteFilePath, const std::string &mode, const std::string &options, const TFTPOparams &params)
{
    // Create an RRQ packet (opcode 1)
    std::vector<uint8_t> requestBuffer;
    requestBuffer.push_back(0); // High byte of opcode (0 for RRQ)
    requestBuffer.push_back(1); // Low byte of opcode (1 for RRQ)
    requestBuffer.insert(requestBuffer.end(), remoteFilePath.begin(), remoteFilePath.end());
    requestBuffer.push_back(0); // Null-terminate the filepath
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0); // Null-terminate the mode

    // // Append optional parameters if provided
    // if (!options.empty())
    // {
    //     requestBuffer.insert(requestBuffer.end(), options.begin(), options.end());
    //     requestBuffer.push_back(0); // Null-terminate the options
    // }

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
        std::cerr << "Error: Failed to send RRQ packet." << std::endl;
    }

    std::cerr << "RRQ " << hostname << ":" << port << " \"" << remoteFilePath << "\" " << mode;
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

bool receiveData(int sock, uint16_t &receivedBlockID, std::string &data, const TFTPOparams &params, const std::string &hostname)
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

    // std::cerr << "Sent DATA packet with size: " << dataBuffer.size() << " bytes, block ID: " << blockID << std::endl; // Print the size and block ID

    blockID = receivedBlockID + 1;

    return true;
}

bool receiveData_without_options(int sock, uint16_t &receivedBlockID, std::string &data, const TFTPOparams &params, const std::string &hostname)
{
    // Create a buffer to receive the DATA packet
    std::vector<uint8_t> dataBuffer(1024); // Max size of a DATA packet

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

    // Extract the data from the packet (skip the first 4 bytes which are the header)
    data.assign(dataBuffer.begin() + 4, dataBuffer.begin() + receivedBytes);

    // Create sockaddr_in structure for the remote server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = senderAddr.sin_port;
    serverAddr.sin_addr = senderAddr.sin_addr;

    // Send an ACK packet back to the server
    std::vector<uint8_t> ackBuffer(4);
    ackBuffer[0] = 0;                             // High byte of opcode (0 for ACK)
    ackBuffer[1] = 4;                             // Low byte of opcode (4 for ACK)
    ackBuffer[2] = (receivedBlockID >> 8) & 0xFF; // High byte of block ID
    ackBuffer[3] = receivedBlockID & 0xFF;        // Low byte of block ID

    ssize_t sentBytes = sendto(sock, ackBuffer.data(), ackBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send ACK." << std::endl;
        return false;
    }

    sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(sock, (struct sockaddr *)&localAddr, &addrLen) == -1)
    {
        std::cerr << "Error: Failed to get local port." << std::endl;
        return false;
    }

    std::cerr << "DATA " << hostname << ":" << ntohs(localAddr.sin_port) << ":" << ntohs(serverAddr.sin_port) << " " << receivedBlockID << std::endl;

    return true;
}
bool sendAck(int sock, uint16_t blockID, const std::string &hostname, int serverPort, const TFTPOparams &params)
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
        std::cerr << "Error: Failed to convert hostname to IP address." << std::endl;
        return false;
    }

    // Send the ACK packet to the server
    ssize_t sentBytes = sendto(sock, ackBuffer.data(), ackBuffer.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (sentBytes == -1)
    {
        std::cerr << "Error: Failed to send ACK." << std::endl;
        return false;
    }

    // std::cerr << "Sent ACK with block ID: " << blockID << " to server port: " << serverPort << std::endl;

    return true;
}

// Modify the transfer_file function to receive a file from the server
int receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, const TFTPOparams &params)
{
    mode = determineMode(remoteFilePath);

    std::ofstream outputFile(localFilePath, std::ios::binary | std::ios::out); // Open a local file to write the received data

    if (!outputFile.is_open())
    {
        std::cerr << "Error: Failed to open file for writing." << std::endl;
        close(sock); // Close the socket on error
        return 1;
    }

    std::cout << "File opened" << std::endl;

    // Send an RRQ packet to request the file from the server with options
    sendReadRequest(sock, hostname, port, remoteFilePath, mode, options, params);

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

    uint16_t blockID = 0; // Initialize the block ID

    while (!transferComplete)
    {
        uint16_t receivedBlockID;
        std::string data;
        std::map<std::string, std::string> receivedOptions;

        if (options_used == true && firstOACK == false)
        {
            firstOACK = true;
            // Wait for an ACK or OACK response after WRQ and capture the server's port
            if (!receiveAck(sock, blockID, serverPort, params, receivedOptions))
            {
                std::cerr << "Error: Failed to receive ACK or OACK after WRQ." << std::endl;
                handleError(sock, hostname, port, 0, 0, "Failed to receive ACK or OACK after WRQ");
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
        if (options_used == true)
        {

            // Send an ACK packet before receiving DATA
            if (!sendAck(sock, blockID, hostname, serverPort, params))
            {
                std::cerr << "Error: Failed to send ACK before receiving DATA." << std::endl;
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
                return 1;
            }

            // Receive a DATA packet and store the data in 'data' with block size option
            if (!receiveData(sock, receivedBlockID, data, params, hostname))
            {
                std::cerr << "Error: Failed to receive DATA." << std::endl;
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
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

            // Receive a DATA packet and store the data in 'data' with block size option

            if (!receiveData_without_options(sock, receivedBlockID, data, params, hostname))
            {
                std::cerr << "Error: Failed to receive DATA." << std::endl;
                close(sock);                   // Close the socket on error
                outputFile.close();            // Close the output file
                remove(localFilePath.c_str()); // Delete the partially downloaded file
                return 1;
            }
        }

        // Check if the received block ID is the expected one
        if (receivedBlockID != blockID + 1)
        {
            std::cerr << "Error: Received out-of-order block ID." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return 1;
        }

        // Write the received data to the output file
        if (!outputFile.write(data.data(), data.size()))
        {
            std::cerr << "Error: Failed to write data to the file." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return 1;
        }

        // Increment the block ID for the next ACK
        blockID = receivedBlockID;

        // If the received data block is less than the block size, it indicates the end of the transfer
        std::cerr << "Size of data (" << data.size() << ") is less than params.blksize (" << params.blksize << ")." << std::endl;

        if (data.size() < params.blksize)
        {

            if (options_used == true)
            {
                if (!sendAck(sock, blockID, hostname, serverPort, params))
                {
                    std::cerr << "Error: Failed to send ACK before receiving DATA." << std::endl;
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
        if (paramName == "blksize")
        {
            option_blksize_used = true;

            int blksize = std::stoi(paramValue);
            if (blksize >= 8 && blksize <= 65464) // Check valid range for blksize
            {
                Oparams.blksize = blksize;
            }
            else
            {
                std::cerr << "Chybná hodnota parametru blksize: " << blksize << std::endl;
                return false;
            }
        }
        else if (paramName == "timeout")
        {
            option_timeout_used = true;

            int maxTimeout = std::stoi(paramValue);
            if (maxTimeout >= 0)
            {
                Oparams.timeout_max = maxTimeout;
            }
            else
            {
                std::cerr << "Chybná hodnota parametru timeout: " << maxTimeout << std::endl;
                return false;
            }
        }
        else if (paramName == "tsize")
        {
            option_tsize_used = true;

            int transfersize = std::stoi(paramValue);
            if (transfersize >= 0)
            {
                Oparams.transfersize = transfersize;
            }
            else
            {
                std::cerr << "Chybná hodnota parametru tsize: " << transfersize << std::endl;
                return false;
            }
        }
        else
        {
            std::cerr << "Neznámý parametr: " << paramName << std::endl;
            return false;
        }
    }
    else
    {
        std::cerr << "Nesprávný formát parametrů." << std::endl;
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
                std::cerr << "Chyba při parsování parametrů." << std::endl;
                return 1;
            }
        }
    }

    if (hostname.empty() || localFilePath.empty())
    {
        std::cerr << "Usage: tftp-client -h hostname -f [filepath] -t dest_filepath [-p port]" << std::endl;
        return 1;
    }

    // Determine the mode based on the file content
    std::string mode = "octet";

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
        std::cerr << "Error: You must specify either -f or -t option to send or receive a file, respectively." << std::endl;
        close(sock);
        return 1;
    }

    return 0;
}