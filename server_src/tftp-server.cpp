/**
 * @file tftp_server.h
 * @brief TFTP server part of ISA project
 * @author xnovos14 - Denis Novosád
 */

#include "tftp-server.h"

// TODO
//      code review + komnety
//      ověřit na referenčním stroji
//      dokumentace

bool fileExists(const std::string &filepath)
{
    std::ifstream file(filepath.c_str());
    return file.good();
}

int handleIncomingPacket(int sockfd, sockaddr_in &clientAddr, int opcode, sockaddr_in &serverAddr)
{
    switch (opcode)
    {
    case OP_RRQ:
        return 0;
    case OP_WRQ:
        return 0;

    default:
        sendError(sockfd, ERROR_ILLEGAL_OPERATION, "Illegal operation", clientAddr, serverAddr);
        std::cout << "Illegal operation detected!" << std::endl;
        return 1;
    }
}

uint16_t checkDiskSpace(int size_of_file, const std::string &path)
{
    struct statvfs stat;
    if (statvfs("/", &stat) == 0)
    {
        unsigned long long freeSpace = stat.f_frsize * stat.f_bfree;

        if (freeSpace < size_of_file)
        {
            std::cout << "Free space is: " << freeSpace / (1024 * 1024) << " MB "
                      << "You need" << size_of_file << std::endl;
            return ERROR_DISK_FULL;
        }
    }
    else
    {
        std::cout << "Error getting disk space information." << std::endl;
        return ERROR_UNDEFINED;
    }

    return 0;
}

void sendError(int sockfd, uint16_t errorCode, const std::string &errorMsg, sockaddr_in &clientAddr, sockaddr_in &serverAddr)
{
    // Vytvoření chybového packetu
    TFTPPacket errorPacket;
    errorPacket.opcode = htons(ERROR);
    memcpy(errorPacket.data, &errorCode, sizeof(uint16_t));
    strcpy(errorPacket.data + sizeof(uint16_t), errorMsg.c_str());

    // Odeslání chybového packetu klientovi
    sendto(sockfd, &errorPacket, sizeof(uint16_t) + errorMsg.size() + 1, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    // Výpis chybové zprávy na standardní chybový výstup
    std::cerr << "ERROR "
              << inet_ntoa(clientAddr.sin_addr) << ":"
              << ntohs(clientAddr.sin_port) << ":"
              << ntohs(serverAddr.sin_port) << " "
              << errorCode << " \"" << errorMsg << "\"" << std::endl;
}

bool sendDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum, const char *data, size_t dataSize, uint16_t blockSize)
{
    uint16_t opcode = htons(DATA);
    uint16_t blockNumNetwork = htons(blockNum);

    size_t packetSize = sizeof(uint16_t) * 2 + dataSize;

    std::vector<uint8_t> dataPacket(packetSize);

    // Kopírování operačního kódu, čísla bloku a dat do packetu
    memcpy(dataPacket.data(), &opcode, sizeof(uint16_t));
    memcpy(dataPacket.data() + sizeof(uint16_t), &blockNumNetwork, sizeof(uint16_t));
    memcpy(dataPacket.data() + sizeof(uint16_t) * 2, data, dataSize);

    ssize_t sentBytes = sendto(sockfd, dataPacket.data(), packetSize, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    if (sentBytes == -1)
    {
        std::cout << "Chyba při odesílání datového packetu pro blok " << blockNum << std::endl;
        return false;
    }

    return true;
}

bool sendOACK(int sockfd, sockaddr_in &clientAddr, std::map<std::string, int> &options_map, TFTPOparams &params, std::streampos filesize)
{

    std::vector<uint8_t> oackBuffer;

    // Přidej opcode do vektoru
    uint16_t opcode = htons(OACK);
    oackBuffer.insert(oackBuffer.end(), reinterpret_cast<uint8_t *>(&opcode), reinterpret_cast<uint8_t *>(&opcode) + sizeof(uint16_t));

    // Přidej "blksize" option, pokud je k dispozici v options_map
    auto blksizeIt = options_map.find("blksize");
    if (blksizeIt != options_map.end())
    {
        const char *optionName = "blksize";
        oackBuffer.insert(oackBuffer.end(), optionName, optionName + strlen(optionName) + 1); // Včetně nulového znaku

        // Přidej hodnotu volby (jako text) do vektoru
        std::string blockSizeStr = std::to_string(params.blksize);
        oackBuffer.insert(oackBuffer.end(), blockSizeStr.begin(), blockSizeStr.end());
        oackBuffer.push_back('\0'); // Přidej nulový znak za hodnotou
    }

    // Přidej "timeout" option, pokud je k dispozici v options_map
    auto timeoutIt = options_map.find("timeout");
    if (timeoutIt != options_map.end())
    {
        const char *optionName = "timeout";
        oackBuffer.insert(oackBuffer.end(), optionName, optionName + strlen(optionName) + 1); // Včetně nulového znaku

        // Přidej hodnotu volby (jako text) do vektoru
        std::string timeoutStr = std::to_string(params.timeout);
        oackBuffer.insert(oackBuffer.end(), timeoutStr.begin(), timeoutStr.end());
        oackBuffer.push_back('\0'); // Přidej nulový znak za hodnotou
    }

    auto tsizeIt = options_map.find("tsize");
    if (tsizeIt != options_map.end())
    {

        params.transfersize = filesize;
        const char *optionName = "tsize";
        oackBuffer.insert(oackBuffer.end(), optionName, optionName + strlen(optionName) + 1); // Včetně nulového znaku

        // Přidej hodnotu volby (jako text) do vektoru
        std::string tsizeStr = std::to_string(params.transfersize);
        oackBuffer.insert(oackBuffer.end(), tsizeStr.begin(), tsizeStr.end());
        oackBuffer.push_back('\0'); // Přidej nulový znak za hodnotou
    }

    // Po dokončení vytvoření vektoru můžete poslat OACK packet
    ssize_t sentBytes = sendto(sockfd, oackBuffer.data(), oackBuffer.size(), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

    // Check for errors
    if (sentBytes == -1)
    {
        std::cout << "Error sending OACK packet" << std::endl;
        return false;
    }

    return true;
}

bool sendFileData(int sockfd, sockaddr_in &clientAddr, sockaddr_in &serverAddr, const std::string &filename, std::map<std::string, int> &options_map, TFTPOparams &params)
{
    std::ifstream file;

    file.open(filename, std::ios::binary);

    if (!file)
    {
        sendError(sockfd, ERROR_FILE_NOT_FOUND, "Illegal operation", clientAddr, serverAddr);
        return false;
    }

    // Získání velikosti souboru
    file.seekg(0, std::ios::end);
    std::streampos filesize = file.tellg();

    std::cout << "Size of the file: " << filesize << " bytes" << std::endl;
    // návrat zpět na zčátek
    file.seekg(0, std::ios::beg);

    // Pokud byly nalezeny volitelné parametry, pokusit se je nastavit
    if (blocksizeOptionUsed || timeoutOptionUsed || transfersizeOptionUsed)
    {
        int retries = 0;
        const int maxRetries = 4; // podle rfc specifikace
        bool ackReceived = false;

        // Odeslání OACK s nastavenými volitelnými parametry
        while (retries < maxRetries)
        {
            if (!sendOACK(sockfd, clientAddr, options_map, params, filesize))
            {
                continue;
            }

            // Přijetí ACK pro potvrzení OACK
            if (receiveAck(sockfd, 0, clientAddr, serverAddr, params.timeout))
            {
                ackReceived = true;
                break;
            }
            else
            {
                retries++;
            }
        }

        if (!ackReceived)
        {
            std::cout << "Failed to receive ACK after multiple retries" << std::endl;
            file.close();
            return false;
        }
    }

    // buffer pro data
    std::vector<char> dataBuffer(params.blksize);
    // počítadlo čísla bloku
    uint16_t blockNum = 1;

    bool lastnullpacket = false;
    int lastbytesread;

    while (true)
    {
        // Načtení dat do bufferu
        file.read(dataBuffer.data(), params.blksize);

        std::streamsize bytesRead = file.gcount();

        if (bytesRead > 0)
        {
            lastbytesread = bytesRead;
            int retries = 0;
            const int maxRetries = 4;
            bool ackReceived = false;

            while (retries < maxRetries)
            {

                // Odeslání dat
                if (!sendDataPacket(sockfd, clientAddr, blockNum, dataBuffer.data(), bytesRead, bytesRead))
                {
                    file.close();
                    return false;
                }

                // Čekání na ACK pro odeslaný blok
                if (receiveAck(sockfd, blockNum, clientAddr, serverAddr, params.timeout))
                {
                    ackReceived = true;
                    break;
                }
                else
                {
                    retries++;
                }
            }

            if (!ackReceived)
            {
                std::cout << "Failed to receive ACK for block " << blockNum << " after multiple retries" << std::endl;
                file.close();
                return false;
            }

            if (bytesRead < params.blksize)
            {
                std::cout << "No more data to send" << std::endl;
            }

            blockNum++;
        }
        else
        {
            if (bytesRead == 0 && lastbytesread == params.blksize)
            {
                lastnullpacket = true; // Nastavit na true, pokud byla poslední data o velikosti 0
            }
            break;
        }

        dataBuffer.clear();
    }

    if (lastnullpacket)
    {
        // Odešlete poslední prázdný DATA packet
        if (!sendDataPacket(sockfd, clientAddr, blockNum, nullptr, 0, 0))
        {
            file.close();
            return false;
        }

        // Čekat na potvrzení pro poslední prázdný DATA packet
        if (!receiveAck(sockfd, blockNum, clientAddr, serverAddr, params.timeout))
        {
            std::cout << "Failed to receive ACK for the last null DATA packet" << std::endl;
            file.close();
            return false;
        }
    }

    options_map.clear();
    file.close();
    return true;
}
bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr, sockaddr_in &serverAddr, int timeout)
{
    TFTPPacket ackPacket;
    memset(&ackPacket, 0, sizeof(TFTPPacket));

    socklen_t clientAddrLen = sizeof(clientAddr);

    // Nastavení timeoutu
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        std::cout << "Failed to set socket timeout" << std::endl;
        return false;
    }

    while (true)
    {
        ssize_t bytesReceived = recvfrom(sockfd, &ackPacket, sizeof(ackPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

        if (bytesReceived < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cout << "Timeout waiting for ACK packet" << std::endl;
                return false;
            }
            std::cout << "Error receiving ACK packet" << std::endl;
            sendError(sockfd, ERROR_UNDEFINED, "Illegal operation", clientAddr, serverAddr);

            return false;
        }

        if (bytesReceived < sizeof(uint16_t) * 2)
        {
            // Invalid ACK packet
            std::cout << "Received an invalid ACK packet" << std::endl;
            sendError(sockfd, ERROR_UNDEFINED, "Illegal operation", clientAddr, serverAddr);

            return false;
        }

        uint16_t opcode = ntohs(ackPacket.opcode);
        if (opcode == ACK)
        {
            uint16_t blockNum = ntohs(*(uint16_t *)ackPacket.data);

            if (blockNum == expectedBlockNum)
            {
                std::cerr << "ACK "
                          << inet_ntoa(clientAddr.sin_addr) << ":"
                          << ntohs(clientAddr.sin_port) << " "
                          << blockNum
                          << std::endl;
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
                std::cout << "Received an unexpected ACK for block " << blockNum << std::endl;
                sendError(sockfd, ERROR_UNKNOWN_TRANSFER_ID, "Illegal operation", clientAddr, serverAddr);
                return false;
            }
        }
        else
        {
            // Received an unexpected packet (not an ACK)
            std::cout << "Received an unexpected packet with opcode " << opcode << std::endl;
            sendError(sockfd, ERROR_ILLEGAL_OPERATION, "Illegal operation", clientAddr, serverAddr);
            return false;
        }
    }
}

bool receiveDataPacket(int sockfd, sockaddr_in &clientAddr, sockaddr_in &serverAddr, uint16_t expectedBlockNum, std::ofstream &file, TFTPOparams &params)
{
    std::vector<uint8_t> dataPacket;
    dataPacket.clear();
    socklen_t clientAddrLen = sizeof(clientAddr);
    dataPacket.resize(params.blksize + 4, 0); // Inicializace vektoru nulami

    // Set the timeout for recvfrom
    struct timeval tv;
    tv.tv_sec = params.timeout;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        std::cout << "Failed to set socket timeout" << std::endl;
        return false;
    }

    ssize_t bytesReceived = recvfrom(sockfd, dataPacket.data(), dataPacket.size(), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

    if (bytesReceived < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            std::cout << "Timeout waiting for DATA packet" << std::endl;
            return false;
        }
        std::cout << "Error receiving DATA packet" << std::endl;
        return false;
    }

    if (bytesReceived < sizeof(uint16_t) * 2)
    {
        // Invalid DATA packet
        std::cout << "Received an invalid DATA packet" << std::endl;
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

            // Write the data to the file
            file.write(reinterpret_cast<const char *>(dataPacket.data() + sizeof(uint16_t) * 2), dataSize);

            if (dataSize < params.blksize - 2)
            {
                lastblockfromoutside = true;
            }

            std::cerr << "DATA "
                      << inet_ntoa(clientAddr.sin_addr) << ":"
                      << ntohs(clientAddr.sin_port) << ":"
                      << ntohs(serverAddr.sin_port) << " "
                      << blockNum
                      << std::endl;

            return true;
        }
        else if (blockNum < expectedBlockNum)
        {
            // Received a duplicate DATA packet (ignore)
            std::cout << "Received a duplicate DATA packet for block " << blockNum << std::endl;
            sendError(sockfd, ERROR_UNKNOWN_TRANSFER_ID, "Illegal operation", clientAddr, serverAddr);
            return false;
        }
        else
        {
            // Received an out-of-order DATA packet (unexpected)
            std::cout << "Received an unexpected DATA packet for block " << blockNum << std::endl;
            sendError(sockfd, ERROR_UNKNOWN_TRANSFER_ID, "Illegal operation", clientAddr, serverAddr);
            return false;
        }
    }
    else
    {
        // Received an unexpected packet (not a DATA packet)
        std::cout << "Received an unexpected packet with opcode " << opcode << std::endl;
        sendError(sockfd, ERROR_ILLEGAL_OPERATION, "Illegal operation", clientAddr, serverAddr);
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
        std::cout << "Error sending ACK packet for block " << blockNum << std::endl;
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
        std::cout << "Invalid filename in the request packet." << std::endl;
        return false;
    }

    filename = packetData;
    packetData += filenameLength + 1; // Move to the next byte after the filename

    // Extract the mode
    size_t modeLength = strlen(packetData);

    // Check if the mode is "octet" or "netascii"
    if (modeLength == 0 || modeLength >= MAX_DATA_SIZE)
    {
        std::cout << "Unsupported transfer mode in the request packet." << std::endl;
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
            std::cout << "Malformed option in the request packet." << std::endl;
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
            std::cout << "Invalid option value: " << optionValueStr << std::endl;
            return false;
        }
    }

    auto blksizeIt = options_map.find("blksize");
    auto timeoutIt = options_map.find("timeout");
    auto tsizeIt = options_map.find("tsize");

    // If "blksize" option is found in the map, use the specified block size
    if (blksizeIt != options_map.end())
    {
        params.blksize = blksizeIt->second; // Read the value from the map
        blocksizeOptionUsed = true;
    }

    // If "timeout" option is found in the map, use the specified timeout
    if (timeoutIt != options_map.end())
    {
        params.timeout = timeoutIt->second; // Read the value from the map
        timeoutOptionUsed = true;
    }

    // If "tsize" option is found in the map, use the specified timeout
    if (tsizeIt != options_map.end())
    {
        params.transfersize = tsizeIt->second; // Read the value from the map
        transfersizeOptionUsed = true;
    }

    // Options processed successfully
    return true;
}

void runTFTPServer(int port, const std::string &root_dirpath)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cout << "Error creating socket" << std::endl;
        return;
    }

    if (chdir(root_dirpath.c_str()) != 0)
    {
        std::cout << "Error: Failed to change to root directory: " << root_dirpath << std::endl;
        return;
    }

    struct sockaddr_in serverAddr, clientAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cout << "Error binding socket" << std::endl;
        close(sockfd);
        return;
    }

    // Get the current socket timeout settings for restoration
    struct timeval original_tv;
    socklen_t len = sizeof(original_tv);
    getsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &original_tv, &len);

    while (true)
    {
        TFTPPacket requestPacket;
        TFTPOparams params;
        params.blksize = 512;
        params.timeout = 5;
        blocksizeOptionUsed = false;
        timeoutOptionUsed = false;
        transfersizeOptionUsed = false;

        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &original_tv, sizeof(original_tv));

        memset(&requestPacket, 0, sizeof(requestPacket));

        socklen_t clientAddrLen = sizeof(clientAddr);

        std::map<std::string, int> options_map;

        ssize_t bytesReceived = recvfrom(sockfd, &requestPacket, sizeof(requestPacket), 0, (struct sockaddr *)&clientAddr, &clientAddrLen);

        if (bytesReceived < 0)
        {
            std::cout << "Error receiving packet" << std::endl;
            continue;
        }

        uint16_t opcode = ntohs(requestPacket.opcode);
        std::string filename;
        std::string mode;

        hasOptions(requestPacket, filename, mode, options_map, params);

        std::string optionsString = "";
        for (const auto &pair : options_map)
        {
            optionsString += pair.first + "=" + std::to_string(pair.second) + " ";
        }

        if (handleIncomingPacket(sockfd, clientAddr, opcode, serverAddr) == 1)
        {
            continue;
        }

        if (opcode == RRQ)
        {

            std::cerr << "RRQ "
                      << inet_ntoa(clientAddr.sin_addr) << ":"
                      << ntohs(clientAddr.sin_port) << " \""
                      << filename << "\" "
                      << mode << " "
                      << optionsString
                      << std::endl;

            if (!sendFileData(sockfd, clientAddr, serverAddr, filename, options_map, params))
            {
                std::cout << "Error sending file data" << std::endl;
            }
        }
        else if (opcode == WRQ)
        {

            std::cerr << "WRQ "
                      << inet_ntoa(clientAddr.sin_addr) << ":"
                      << ntohs(clientAddr.sin_port) << " \""
                      << filename << "\" "
                      << mode << " "
                      << optionsString
                      << std::endl;

            // if (fileExists(filename))
            // {
            //     std::cout << "The file " << filename << " exists." << std::endl;
            //     sendError(sockfd, ERROR_FILE_ALREADY_EXISTS, "File exists", clientAddr, serverAddr);
            // }

            if (transfersizeOptionUsed)
            {

                uint16_t diskspace = checkDiskSpace(params.transfersize, filename);

                if (diskspace == ERROR_DISK_FULL)
                {
                    std::cout << "ERROR_DISK_FULL!" << std::endl;
                    sendError(sockfd, ERROR_DISK_FULL, "Illegal operation", clientAddr, serverAddr);
                    continue;
                }
            }

            std::ofstream file(filename, std::ios::binary);

            if (!file)
            {
                sendError(sockfd, ERROR_ACCESS_VIOLATION, "Illegal operation", clientAddr, serverAddr);
                continue;
            }
            else
            {
                if (!options_map.empty())
                {
                    if (!sendOACK(sockfd, clientAddr, options_map, params, params.transfersize)) // 0 zatím
                    {
                        std::cout << "Error sending OACK" << std::endl;
                        continue;
                    }
                }
                else
                {
                    if (!sendAck(sockfd, clientAddr, 0))
                    {
                        std::cout << "Error sending initial ACK" << std::endl;
                        continue;
                    }
                }

                // Start receiving file data in DATA packets
                uint16_t blockNum = 1; // Initialize block number to 1
                bool lastBlockReceived = false;
                lastblockfromoutside = false;

                while (!lastBlockReceived)
                {
                    int retries = 0;
                    const int maxRetries = 4; // As per RFC specification
                    bool dataPacketReceived = false;

                    while (retries < maxRetries)
                    {
                        if (receiveDataPacket(sockfd, clientAddr, serverAddr, blockNum, file, params))
                        {
                            dataPacketReceived = true;
                            break;
                        }
                        else
                        {
                            retries++;
                            if (!options_map.empty() && blockNum == 1)
                            {
                                if (!sendOACK(sockfd, clientAddr, options_map, params, params.transfersize)) // 0 zatím
                                {
                                    continue;
                                }
                            }
                            else
                            {
                                if (!sendAck(sockfd, clientAddr, blockNum - 1))
                                {
                                    std::cout << "Error sending initial ACK" << std::endl;
                                    file.close();
                                    continue;
                                }
                            }
                        }
                    }

                    if (!dataPacketReceived)
                    {
                        std::cout << "Failed to receive DATA packet for block " << blockNum << " after multiple retries" << std::endl;
                        break;
                    }

                    // Send ACK for the received block
                    if (!sendAck(sockfd, clientAddr, blockNum))
                    {
                        std::cout << "Error sending ACK for block " << blockNum << std::endl;
                        break;
                    }

                    // Check if the received block was the last block
                    if (lastblockfromoutside == true)
                    {
                        lastBlockReceived = true;
                    }
                    blockNum++;
                }

                file.close(); // Close the file when the transfer is complete or encounters an error
            }
        }

        else
        {
            sendError(sockfd, ERROR_UNDEFINED, "Illegal operation", clientAddr, serverAddr);
        }
        memset(&requestPacket, 0, sizeof(requestPacket));
    }

    close(sockfd);
}

void sigintHandler(int signal)
{
    std::cout << "Received SIGINT (Ctrl+C). Terminating gracefully..." << std::endl;
    // Perform any cleanup or termination tasks here
    std::exit(0); // Terminate the program
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, sigintHandler);

    int port = 69;            // Default TFTP port
    std::string root_dirpath; // Directory path

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            if (i + 1 < argc)
            {
                port = std::atoi(argv[i + 1]);
                i++; // Skip the next argument
            }
            else
            {
                std::cout << "Error: Missing value for '-p' option" << std::endl;
                return 1;
            }
        }
        else
        {
            root_dirpath = argv[i];
        }
    }

    if (root_dirpath.empty())
    {
        std::cout << "Error: root_dirpath must be specified" << std::endl;
        return 1;
    }

    runTFTPServer(port, root_dirpath);
    return 0;
}