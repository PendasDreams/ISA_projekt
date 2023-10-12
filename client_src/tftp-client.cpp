#include "tftp-client.h"

// Define the initial block ID
uint16_t blockID = 0;

struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout_min;
    uint16_t timeout_max;
    uint16_t transfersize;
};

// Function to send WRQ (Write Request) packet with optional parameters (OACK)
void sendWriteRequestWithOACK(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, const std::string &options, const TFTPOparams &params)
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
    if (params.blksize > 0)
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

    // Convert the requestBuffer to a string for printing
    std::string requestString(requestBuffer.begin(), requestBuffer.end());

    // Print the request packet contents
    std::cerr << "Sent Write Request packet with content:" << std::endl;
    for (size_t i = 0; i < requestString.length(); i++)
    {
        std::cerr << "0x" << std::hex << static_cast<int>(requestString[i]) << " ";
    }
    std::cerr << std::endl;
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

#include <iomanip> // for std::hex

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
        size_t pos = 1;

        while (pos < receivedBytes)
        {
            std::string option;
            std::string value;

            std::cerr << "option read:" << std::endl;

            // Read option until null terminator
            while (pos < receivedBytes && packetBuffer[pos] != 0)
            {
                std::cerr << static_cast<char>(packetBuffer[pos]);
                option += (packetBuffer[pos]);
                pos++;
            }

            // Skip null terminator
            pos++;

            std::cerr << " " << std::endl;

            std::cerr << "value read:" << std::endl;

            // Read value until null terminator
            while (pos < receivedBytes && packetBuffer[pos] != 0)
            {
                std::cerr << static_cast<char>(packetBuffer[pos]);
                value += packetBuffer[pos];
                pos++;
            }
            std::cerr << " " << std::endl;

            // Skip null terminator
            pos++;

            option.erase(0, 1); // Erase the first character
            option.push_back('\0');

            std::cerr << "option: " << option << std::endl;
            std::cerr << "value: " << value << std::endl;

            // Store option and value in the receivedOptions map
            recieved_options[option] = value;
        }

        for (const auto &pair : recieved_options)
        {
            if (pair.first == "blksize" && pair.second != std::to_string(params.blksize))
            {
                std::cerr << "Error: Received blksize option does not match the requested value" << pair.second << " and " << std::to_string(params.blksize) << std::endl;
                return false;
            }
        }

        // You can similarly check other options as needed
    }

    std::cerr << "Received " << (opcode == 4 ? "ACK" : "OACK") << " with block ID: " << receivedBlockID << " from server port: " << serverPort << std::endl;

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
    // std::cerr << "DATA Content: " << std::string(dataBuffer.begin() + 4, dataBuffer.end()) << std::endl;
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

// Function to send a file to the server or upload from stdin
void SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options, const TFTPOparams &params)
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
        return;
    }
    inputStream = &file;

    const size_t maxDataSize = params.blksize; // Max data size in one DATA packet
    char buffer[maxDataSize];                  // Buffer for reading data from the file or stdin

    int serverPort = 0; // Variable to capture the server's port

    // Send WRQ packet with optional parameters (OACK)
    sendWriteRequestWithOACK(sock, hostname, port, remoteFilePath, mode, options, params);

    std::map<std::string, std::string> receivedOptions;

    // Wait for an ACK or OACK response after WRQ and capture the server's port
    if (!receiveAck(sock, blockID, serverPort, params, receivedOptions))
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

        // std::cerr << "reading data *** " << std::endl; // Print the server's port

        inputStream->read(buffer, maxDataSize);

        std::streamsize bytesRead = inputStream->gcount();

        if (bytesRead > 0)
        {
            // std::cerr << "Sending DATA packet with block ID: " << blockID + 1 << std::endl;
            sendData(sock, hostname, serverPort, std::string(buffer, bytesRead)); // Send data to the server's port
                                                                                  // Wait for the ACK after sending DATA

            if (!receiveAck(sock, blockID, serverPort, params, receivedOptions) && !isOACK)
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

    std::cout << "Waiting for 1 second..." << std::endl;

    // Sleep for 1 second (1000 milliseconds)
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Done!" << std::endl;

    // Close the socket
    close(sock);
}

// Function to send RRQ (Read Request) packet
void sendReadRequest(int sock, const std::string &hostname, int port, const std::string &remoteFilePath, const std::string &mode, const std::string &options)
{
    // Create an RRQ packet (opcode 1)
    std::vector<uint8_t> requestBuffer;
    requestBuffer.push_back(0); // High byte of opcode (0 for RRQ)
    requestBuffer.push_back(1); // Low byte of opcode (1 for RRQ)
    requestBuffer.insert(requestBuffer.end(), remoteFilePath.begin(), remoteFilePath.end());
    requestBuffer.push_back(0); // Null-terminate the filepath
    requestBuffer.insert(requestBuffer.end(), mode.begin(), mode.end());
    requestBuffer.push_back(0); // Null-terminate the mode

    // Append optional parameters if provided
    if (!options.empty())
    {
        requestBuffer.insert(requestBuffer.end(), options.begin(), options.end());
        requestBuffer.push_back(0); // Null-terminate the options
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

    std::cerr << "Read Request " << hostname << ":" << port << " \"" << remoteFilePath << "\" " << mode;
    if (!options.empty())
    {
        std::cerr << " with options: " << options;
    }
    std::cerr << std::endl;
}

bool receiveData(int sock, uint16_t &receivedBlockID, std::string &data)
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

    std::cerr << "Received DATA with block ID: " << receivedBlockID << " from server port: " << ntohs(serverAddr.sin_port) << std::endl;

    return true;
}

// Modify the transfer_file function to receive a file from the server
void receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options)
{
    std::ofstream outputFile(localFilePath, std::ios::binary | std::ios::out); // Open a local file to write the received data

    if (!outputFile.is_open())
    {
        std::cerr << "Error: Failed to open file for writing." << std::endl;
        close(sock); // Close the socket on error
        return;
    }

    std::cerr << "File opened" << std::endl;

    // Send an RRQ packet to request the file from the server
    sendReadRequest(sock, hostname, port, remoteFilePath, mode, options);

    bool transferComplete = false;

    uint16_t blockID = 0; // Initialize the block ID

    while (!transferComplete)
    {
        uint16_t receivedBlockID;
        std::string data;

        // Receive a DATA packet and store the data in 'data'
        if (!receiveData(sock, receivedBlockID, data))
        {
            std::cerr << "Error: Failed to receive DATA." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return;
        }

        // Check if the received block ID is the expected one
        if (receivedBlockID != blockID + 1)
        {
            std::cerr << "Error: Received out-of-order block ID." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return;
        }

        // Write the received data to the output file
        if (!outputFile.write(data.data(), data.size()))
        {
            std::cerr << "Error: Failed to write data to the file." << std::endl;
            close(sock);                   // Close the socket on error
            outputFile.close();            // Close the output file
            remove(localFilePath.c_str()); // Delete the partially downloaded file
            return;
        }

        // Increment the block ID for the next ACK
        blockID = receivedBlockID;

        // If the received data block is less than the maximum size, it indicates the end of the transfer
        if (data.size() < 1024)
        {
            transferComplete = true;
        }
    }

    // Close the output file
    outputFile.close();

    std::cout << "Waiting for 1 second..." << std::endl;

    // Sleep for 1 second (1000 milliseconds)
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Done!" << std::endl;

    // Close the socket
    close(sock);

    std::cerr << "File download complete: " << localFilePath << std::endl;
}

bool isAscii(const std::string &filePath)
{
    std::ifstream file(filePath, std::ios::binary);

    if (!file)
    {
        std::cerr << "Failed to open file for reading asii." << std::endl;
        return false;
    }

    char buffer;
    while (file.get(buffer))
    {
        if (buffer == '\0')
        {
            // První nula by mohla indikovat binární data
            file.close();
            return false; // Pravděpodobně binární data
        }
    }

    file.close();
    return true; // Pravděpodobně ASCII data
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

bool parseTFTPParameters(const std::string &Oparamstring, TFTPOparams &Oparams)
{
    // Inicializace parametrů na výchozí hodnoty
    Oparams.blksize = 512;      // Výchozí hodnota blksize
    Oparams.timeout_min = 1;    // Výchozí hodnota timeout_min
    Oparams.timeout_max = 1000; // Výchozí hodnota timeout_max
    Oparams.transfersize = 0;   // Výchozí hodnota transfersize

    // Rozdělení řetězce parametrů na jednotlivé páry název=hodnota
    std::istringstream Oparamstream(Oparamstring);
    std::string paramPair;
    while (std::getline(Oparamstream, paramPair, ','))
    {
        size_t equalPos = paramPair.find('=');
        if (equalPos != std::string::npos)
        {
            std::string paramName = paramPair.substr(0, equalPos);
            std::string paramValue = paramPair.substr(equalPos + 1);

            if (paramName == "blksize")
            {
                int blksize = std::stoi(paramValue);
                if (blksize >= 8 && blksize <= 65464) // Kontrola platného rozsahu blksize
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
                size_t commaPos = paramValue.find(',');
                if (commaPos != std::string::npos)
                {
                    int minTimeout = std::stoi(paramValue.substr(0, commaPos));
                    int maxTimeout = std::stoi(paramValue.substr(commaPos + 1));

                    if (minTimeout >= 1 && maxTimeout >= minTimeout)
                    {
                        Oparams.timeout_min = minTimeout;
                        Oparams.timeout_max = maxTimeout;
                    }
                    else
                    {
                        std::cerr << "Chybné hodnoty parametru timeout: " << minTimeout << ", " << maxTimeout << std::endl;
                        return false;
                    }
                }
                else
                {
                    std::cerr << "Chybějící čárka mezi hodnotami parametru timeout." << std::endl;
                    return false;
                }
            }
            else if (paramName == "transfersize")
            {
                int transfersize = std::stoi(paramValue);
                if (transfersize >= 0)
                {
                    Oparams.transfersize = transfersize;
                }
                else
                {
                    std::cerr << "Chybná hodnota parametru transfersize: " << transfersize << std::endl;
                    return false;
                }
            }
            else
            {
                std::cerr << "Neznámý parametr: " << paramName << std::endl;
                return false;
            }
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
    std::string hostname;
    int port = 69; // Default port for TFTP
    std::string localFilePath;
    std::string remoteFilePath;
    std::string options; // Optional parameters for OACK

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
        else if (arg == "-o" && i + 1 < argc)
        {
            options = argv[++i];
        }
    }

    if (hostname.empty() || remoteFilePath.empty())
    {
        std::cerr << "Usage: tftp-client -h hostname -f [filepath] -t dest_filepath [-p port]" << std::endl;
        return 1;
    }

    // Determine the mode based on the file content
    std::string mode = determineMode(localFilePath);

    // Create a UDP socket for communication
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        std::cerr << "Error: Failed to create socket." << std::endl;
        return 1;
    }

    TFTPOparams Oparams;

    if (parseTFTPParameters(options, Oparams))
    {
        std::cout << "blksize: " << Oparams.blksize << std::endl;
        std::cout << "timeout_min: " << Oparams.timeout_min << std::endl;
        std::cout << "timeout_max: " << Oparams.timeout_max << std::endl;
        std::cout << "transfersize: " << Oparams.transfersize << std::endl;
    }
    else
    {
        std::cerr << "Chyba při parsování parametrů." << std::endl;
    }

    // Set up the sockaddr_in structure for the server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, hostname.c_str(), &(serverAddr.sin_addr));

    if (localFilePath.empty())
    {
        SendFile(sock, hostname, port, localFilePath, remoteFilePath, mode, options, Oparams);
    }
    else if (!localFilePath.empty() && !remoteFilePath.empty())
    {
        // Receive a file from the server
        receive_file(sock, hostname, port, localFilePath, remoteFilePath, mode, options);
    }
    else
    {
        std::cerr << "Error: You must specify either -f or -t option to send or receive a file, respectively." << std::endl;
        close(sock);
        return 1;
    }

    // Send the file to the server or upload from stdin

    return 0;
}