/**
 * @file tftp_server.h
 * @brief Header file containing declarations for TFTP server functions and variables.
 * @author xnovos14 - Denis Novosád
 */

#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <map>
#include <iomanip>
#include <csignal>
#include <sys/statvfs.h>
#include <filesystem>
#include <chrono>
#include <thread>

// Function for receiving acknowledgment ACK packet
bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr, sockaddr_in &serverAddr, int timeout);

// TFTP operations
const uint16_t RRQ = 1;
const uint16_t WRQ = 2;
const uint16_t DATA = 3;
const uint16_t ACK = 4;
const uint16_t ERROR = 5;
const uint16_t OACK = 6;

// Maximum data packet size
const size_t MAX_DATA_SIZE = 514;

// Request operation codes
const uint16_t OP_RRQ = 1;
const uint16_t OP_WRQ = 2;

// TFTP Error Codes
const uint16_t ERROR_UNDEFINED = 0;
const uint16_t ERROR_FILE_NOT_FOUND = 1;
const uint16_t ERROR_ACCESS_VIOLATION = 2;
const uint16_t ERROR_DISK_FULL = 3;
const uint16_t ERROR_ILLEGAL_OPERATION = 4;
const uint16_t ERROR_UNKNOWN_TRANSFER_ID = 5;
const uint16_t ERROR_FILE_ALREADY_EXISTS = 6;

// Structure representing a TFTP packet
struct TFTPPacket
{
    uint16_t opcode;
    char data[MAX_DATA_SIZE];
};

bool lastblockfromoutside = false;
bool blocksizeOptionUsed = false;
bool timeoutOptionUsed = false;
bool transfersizeOptionUsed = false;

// Structure for holding options
struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout;
    int transfersize;
};

/**
 * @brief Sends an error packet to the client.
 *
 * @param sockfd TFTP transmission socket.
 * @param errorCode TFTP error code.
 * @param errorMsg Error message text.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param serverAddr sockaddr_in structure representing the server.
 */
void sendError(int sockfd, uint16_t errorCode, const std::string &errorMsg, sockaddr_in &clientAddr, sockaddr_in &serverAddr);

/**
 * @brief Checks the existence of a file at the given path.
 *
 * @param filepath Path to the file.
 * @return True if the file exists, otherwise False.
 */
bool fileExists(const std::string &filepath);

/**
 * @brief Processes an incoming TFTP packet and performs the corresponding action based on the opcode.
 *
 * @param sockfd TFTP transmission socket.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param opcode Opcode of the incoming packet.
 * @param serverAddr sockaddr_in structure representing the server.
 * @return 0 on success, 1 on error.
 */
int handleIncomingPacket(int sockfd, sockaddr_in &clientAddr, int opcode, sockaddr_in &serverAddr);

/**
 * @brief Checks available disk space for writing a file.
 *
 * @param size_of_file Size of the file to be written.
 * @param path Path to the file location on disk.
 * @return 0 if there is enough space, otherwise an error code.
 */
uint16_t checkDiskSpace(int size_of_file, const std::string &path);

/**
 * @brief Sends a data packet to the client.
 *
 * @param sockfd TFTP transmission socket.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param blockNum Block number.
 * @param data Data to be sent.
 * @param dataSize Size of the data.
 * @param blockSize Block size.
 * @return True if the packet was successfully sent, otherwise False.
 */
bool sendDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum, const char *data, size_t dataSize, uint16_t blockSize);

/**
 * @brief Sends an OACK (Option Acknowledgment) packet to the client with optional parameters.
 *
 * @param sockfd TFTP transmission socket.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param options_map Map of optional parameters.
 * @param params TFTP communication parameters, including block size and timeout.
 * @param filesize File size for transmission.
 * @return True if the OACK packet was successfully sent, otherwise False.
 */
bool sendOACK(int sockfd, sockaddr_in &clientAddr, std::map<std::string, int> &options_map, TFTPOparams &params, std::streampos filesize);

/**
 * @brief Sends file data to the client in DATA packets.
 *
 * @param sockfd TFTP transmission socket.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param serverAddr sockaddr_in structure representing the server.
 * @param filename Name of the file to be sent.
 * @param options_map Map of optional parameters.
 * @param params TFTP communication parameters, including block size and timeout.
 * @return True if the data was successfully sent, otherwise False.
 */
bool sendFileData(int sockfd, sockaddr_in &clientAddr, sockaddr_in &serverAddr, const std::string &filename, std::map<std::string, int> &options_map, TFTPOparams &params);

/**
 * @brief Receives an acknowledgment ACK packet from the client.
 *
 * @param sockfd TFTP transmission socket.
 * @param expectedBlockNum Expected block number.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param serverAddr sockaddr_in structure representing the server.
 * @param timeout Timeout for packet reception.
 * @return True if the acknowledgment ACK packet was received, otherwise False.
 */
bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr, sockaddr_in &serverAddr, int timeout);

/**
 * @brief Receives a DATA packet from the client.
 *
 * @param sockfd TFTP transmission socket.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param serverAddr sockaddr_in structure representing the server.
 * @param expectedBlockNum Expected block number.
 * @param file Open file for writing data.
 * @param params TFTP communication parameters, including block size and timeout.
 * @return True if the data was successfully received, otherwise False.
 */
bool receiveDataPacket(int sockfd, sockaddr_in &clientAddr, sockaddr_in &serverAddr, uint16_t expectedBlockNum, std::ofstream &file, TFTPOparams &params);

/**
 * @brief Sends an acknowledgment ACK packet to the client.
 *
 * @param sockfd TFTP transmission socket.
 * @param clientAddr sockaddr_in structure representing the client.
 * @param blockNum Block number to acknowledge.
 * @return True if the ACK packet was successfully sent, otherwise False.
 */
bool sendAck(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum);

/**
 * @brief Checks for the presence of optional parameters in the request packet.
 *
 * @param requestPacket TFTP request packet.
 * @param filename File name from the request packet.
 * @param mode Transfer mode from the request packet.
 * @param options_map Map of optional parameters.
 * @param params TFTP communication parameters, including block size and timeout.
 * @return True if optional parameters were found, otherwise False.
 */
bool hasOptions(TFTPPacket &requestPacket, std::string &filename, std::string &mode, std::map<std::string, int> &options_map, TFTPOparams &params);

/**
 * @brief Main function of the TFTP server.
 *
 * @param port Port on which the server is listening.
 * @param root_dirpath Root directory of the server for handling requests.
 */
void runTFTPServer(int port, const std::string &root_dirpath);

/**
 * @brief Signal handler function for capturing SIGINT signal.
 *
 * @param signal The signal that was captured (e.g., SIGINT).
 */
void sigintHandler(int signal);

#endif // TFTP_SERVER_H
