/**
 * @file tftp_client.h
 * @brief Header file containing declarations for TFTP client functions and variables.
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>
#include <algorithm>
#include <iomanip>
#include <fcntl.h>
#include <sys/statvfs.h>

enum class RequestType
{
    READ,
    WRITE
};

struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout_max;
    int transfersize;
};

extern uint16_t blockID;

extern bool options_used;
extern bool option_blksize_used;
extern bool option_timeout_used;

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

enum TFTPRequestType
{
    READ_REQUEST,
    WRITE_REQUEST
};

bool sendTFTPRequest(TFTPRequestType requestType, int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, TFTPOparams &params);

/**
 * @brief Check if a file is in ASCII format based on its extension.
 *
 * @param fileName The name of the file to check.
 * @return True if the file is in ASCII format, false if it's in binary format.
 */
bool isAscii(const std::string &fileName);

/**
 * @brief Determine the transfer mode based on the file content.
 *
 * @param filePath The path to the file.
 * @return The transfer mode ("netascii" for ASCII, "octet" for binary).
 */
std::string determineMode(const std::string &filePath);

/**
 * @brief Convert a hexadecimal string to a character string.
 *
 * @param hexStr The input hexadecimal string.
 * @return The resulting character string.
 */
std::string hexStringToCharString(const std::string &hexStr);

/**
 * @brief Set socket timeout for receiving.
 *
 * @param sock The socket to set the timeout for.
 * @param timeout The timeout value in seconds.
 * @return True if the timeout was successfully set, false otherwise.
 */
bool setSocketTimeout(int sock, int timeout);

/**
 * @brief Check if a file is in binary format based on its extension.
 *
 * @param filename The name of the file to check.
 * @return True if the file is in binary format, false otherwise.
 */
bool isBinaryFormat(const std::string &filename);

/**
 * @brief Handle errors and send ERROR packets.
 *
 * @param sock The socket to send the ERROR packet on.
 * @param hostname The hostname of the remote server.
 * @param srcPort The source port.
 * @param dstPort The destination port.
 * @param errorCode The error code to include in the ERROR packet.
 * @param errorMsg The error message to include in the ERROR packet.
 */
void handleError(int sock, const std::string &hostname, int srcPort, int dstPort, uint16_t errorCode, const std::string &errorMsg);

/**
 * @brief Send a Write Request (WRQ) packet with optional parameters (OACK).
 *
 * @param sock The socket to send the WRQ packet on.
 * @param hostname The hostname of the remote server.
 * @param port The port of the remote server.
 * @param filepath The file path on the remote server.
 * @param mode The transfer mode ("netascii" or "octet").
 * @param options The additional options for the request.
 * @param params The TFTP parameters including block size and timeout.
 * @return True if the WRQ packet was sent successfully, false otherwise.
 */
bool sendWriteRequest(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Receive an ACK (Acknowledgment) packet and capture the server's port and received options.
 *
 * @param sock The socket to receive the packet on.
 * @param receivedBlockID The received block ID.
 * @param serverPort The server's port (output).
 * @param params The TFTP parameters including block size and timeout.
 * @param receivedOptions The received options (output).
 * @return True if an ACK or OACK packet was received and processed successfully, false otherwise.
 */
bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort, TFTPOparams &params, std::map<std::string, std::string> &receivedOptions);

/**
 * @brief Send a DATA packet with data.
 *
 * @param sock The socket to send the DATA packet on.
 * @param hostname The hostname of the remote server.
 * @param port The port of the remote server.
 * @param data The data to include in the DATA packet.
 * @return True if the DATA packet was sent successfully, false otherwise.
 */
bool sendData(int sock, const std::string &hostname, int port, const std::string &data);

/**
 * @brief Send a file to the server or upload from stdin.
 *
 * @param sock The socket to use for the TFTP transfer.
 * @param hostname The hostname of the remote server.
 * @param port The port of the remote server.
 * @param localFilePath The path to the local file to send.
 * @param remoteFilePath The path to the remote file on the server.
 * @param mode The transfer mode ("netascii" or "octet").
 * @param options The additional options for the request.
 * @param params The TFTP parameters including block size and timeout.
 * @return 0 on success, 1 on failure.
 */
int SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Sends a Read Request (RRQ) packet to request a file from a TFTP server.
 *
 * @param sock The socket to send the RRQ packet on.
 * @param hostname The hostname of the remote server.
 * @param port The port of the remote server.
 * @param remoteFilePath The path to the remote file to request.
 * @param mode The transfer mode ("netascii" or "octet").
 * @param options Optional parameters for the request.
 * @param params The TFTP parameters including block size and timeout.
 */
void sendReadRequest(int sock, const std::string &hostname, int port, const std::string &remoteFilePath, const std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Receives a DATA packet from a TFTP server and captures the received data.
 *
 * @param sock The socket to receive the DATA packet on.
 * @param receivedBlockID The received block ID.
 * @param data The received data (output).
 * @param params The TFTP parameters including block size and timeout.
 * @param hostname The hostname of the remote server.
 * @return True if the DATA packet was received and processed successfully, false otherwise.
 */
bool receiveData(int sock, uint16_t &receivedBlockID, std::string &data, TFTPOparams &params, const std::string &hostname);

/**
 * @brief Receives a DATA packet from a TFTP server without optional parameters and captures the received data.
 *
 * @param sock The socket to receive the DATA packet on.
 * @param receivedBlockID The received block ID.
 * @param data The received data (output).
 * @param params The TFTP parameters including block size and timeout.
 * @param hostname The hostname of the remote server.
 * @return True if the DATA packet was received and processed successfully, false otherwise.
 */
bool receiveData_without_options(int sock, uint16_t &receivedBlockID, std::string &data, TFTPOparams &params, const std::string &hostname);

/**
 * @brief Sends an ACK (Acknowledgment) packet to acknowledge the receipt of data.
 *
 * @param sock The socket to send the ACK packet on.
 * @param blockID The block ID to acknowledge.
 * @param hostname The hostname of the remote server.
 * @param serverPort The port of the remote server.
 * @param params The TFTP parameters including block size and timeout.
 * @return True if the ACK packet was sent successfully, false otherwise.
 */
bool sendAck(int sock, uint16_t blockID, const std::string &hostname, int serverPort, TFTPOparams &params);

/**
 * @brief Receives a file from a TFTP server.
 *
 * @param sock The socket to use for the TFTP transfer.
 * @param hostname The hostname of the remote server.
 * @param port The port of the remote server.
 * @param localFilePath The path to the local file to save the received data.
 * @param remoteFilePath The path to the remote file on the server.
 * @param mode The transfer mode ("netascii" or "octet").
 * @param options Optional parameters for the request.
 * @param params The TFTP parameters including block size and timeout.
 * @return 0 on success, 1 on failure.
 */
int receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Parses TFTP parameters from a string and populates the TFTPOparams struct.
 *
 * @param Oparamstring The string containing TFTP parameters in the format "name=value".
 * @param Oparams The TFTPOparams struct to populate.
 * @return True if parsing and populating the struct was successful, false otherwise.
 */
bool parseTFTPParameters(const std::string &Oparamstring, TFTPOparams &Oparams);