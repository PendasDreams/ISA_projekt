/**
 * @file tftp_client.h
 * @brief Header file containing declarations for TFTP client functions and variables.
 * @author xnovos14 - Denis Novosád
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

// Optional options
struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout_max;
    int transfersize;
};

// Initial block ID
uint16_t blockID = 0;

// Flags to identify which options were used
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

// Request types
enum TFTPRequestType
{
    READ_REQUEST,
    WRITE_REQUEST
};

/**
 * @brief Function to check if a file is in ASCII format.
 *
 * This function checks if the given file is in ASCII format based on its extension.
 * If the extension is "txt," "html," or "xml," the file is considered to be in a text format, and it returns `true`.
 * Otherwise, it returns `false`, indicating a binary format.
 *
 * @param fileName The name of the file to check.
 * @return True if the file is in ASCII format, otherwise False.
 */
bool isAscii(const std::string &fileName);

/**
 * @brief Function to determine the TFTP mode based on a file's content.
 *
 * This function determines the TFTP transfer mode ("netascii" or "octet") based on the content of the file.
 * If the file is in ASCII format, it returns "netascii." Otherwise, it returns "octet."
 *
 * @param filePath The file path to determine the mode for.
 * @return The TFTP mode for the given file ("netascii" or "octet").
 */
std::string determineMode(const std::string &filePath);

/**
 * @brief Function to set the socket timeout.
 *
 * This function sets the socket timeout based on the provided socket and timeout value.
 *
 * @param sock The socket to set the timeout for.
 * @param timeout The timeout in seconds.
 * @return True if the timeout was successfully set, otherwise False.
 */
bool setSocketTimeout(int sock, int timeout);

/**
 * @brief Function to check if a file is in binary format.
 *
 * This function checks if the given file is in binary format based on its extension.
 * If the file has no extension or its extension is not in the list of binary formats, it returns `true`.
 * Otherwise, it returns `false`, indicating a text format.
 *
 * @param filename The name of the file to check.
 * @return True if the file is in binary format, otherwise False.
 */
bool isBinaryFormat(const std::string &filename);

/**
 * @brief Function to handle errors.
 *
 * This function is used to create and send an ERROR packet to the server if an error occurs during communication.
 *
 * @param sock The communication socket.
 * @param hostname The server's hostname.
 * @param srcPort The source port of the client.
 * @param serverPort The server's port.
 * @param errorCode The error code.
 * @param errorMsg The text message describing the error.
 */
void handleError(int sock, const std::string &hostname, int srcPort, int serverPort, uint16_t errorCode, const std::string &errorMsg);

/**
 * @brief Function to receive an ACK (Acknowledgment) packet and get the server's port.
 *
 * This function receives an ACK packet and simultaneously retrieves the server's port from the sent packet.
 * It also retrieves optional parameters from an OACK packet if the received ACK packet is marked as an OACK.
 *
 * @param sock The communication socket.
 * @param receivedBlockID The ID of the received data block.
 * @param serverPort The server's port.
 * @param params TFTP communication parameters, including block size and timeout.
 * @param receivedOptions A map to store optional parameters from the OACK packet.
 * @return True if the ACK was successfully received and the necessary data was obtained, otherwise False.
 */
bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort, TFTPOparams &params, std::map<std::string, std::string> &receivedOptions);

/**
 * @brief Function to send a data packet.
 *
 * This function creates and sends a data packet containing the specified data based on the current data block.
 *
 * @param sock The communication socket.
 * @param hostname The server's hostname.
 * @param port The server's port.
 * @param data The data to be sent.
 * @return True if the data packet was successfully sent, otherwise False.
 */
bool sendData(int sock, const std::string &hostname, int port, const std::string &data);

/**
 * @brief Function to send a file to the server or upload from stdin.
 *
 * This function is used to send a file to the server or upload data from standard input (stdin).
 * It also determines the TFTP mode based on the file's content and sends data in data blocks to the server.
 *
 * @param sock The communication socket.
 * @param hostname The server's hostname.
 * @param port The server's port.
 * @param localFilePath The path to the local file to send or stdin to read data from.
 * @param remoteFilePath The remote file path on the server.
 * @param mode The TFTP transfer mode (netascii or octet).
 * @param options Optional parameters for communication with the server.
 * @param params TFTP communication parameters, including block size and timeout.
 * @return 0 if the transfer was successful, otherwise 1.
 */
int SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Function to send an RRQ (Read Request) packet.
 *
 * This function creates and sends a TFTP RRQ packet based on the provided parameters. The packet type is determined
 * by the `requestType` parameter (READ_REQUEST or WRITE_REQUEST). The packet contains the file name, transfer mode,
 * and optional parameters "blksize," "timeout," and "tsize" if active.
 *
 * @param requestType The request type (READ_REQUEST or WRITE_REQUEST).
 * @param sock The communication socket.
 * @param hostname The target server.
 * @param port The port to send the packet to.
 * @param filepath The server file path.
 * @param mode The transfer mode.
 * @param params TFTP communication parameters, including block size and timeout.
 */
bool sendTFTPRequest(TFTPRequestType requestType, int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, TFTPOparams &params);

/**
 * @brief Function to receive a data packet.
 *
 * This function receives a data packet from the specified socket and extracts information about
 * the data block and the actual data. The received data is stored in the `data` parameter.
 *
 * @param sock The communication socket.
 * @param receivedBlockID The ID of the received data block.
 * @param serverPort The server's port.
 * @param data The received data.
 * @param params TFTP communication parameters, including block size and timeout.
 * @param hostname The server's hostname.
 * @return True if the data was successfully received, otherwise False.
 */
bool receiveData(int sock, uint16_t &receivedBlockID, int &serverPort, std::string &data, TFTPOparams &params, const std::string &hostname);

/**
 * @brief Function to send an acknowledgment (ACK) to the server.
 *
 * This function creates and sends a TFTP ACK packet with the specified data block to acknowledge.
 *
 * @param sock The communication socket.
 * @param blockID The data block ID to acknowledge.
 * @param hostname The server's hostname.
 * @param serverPort The server's port.
 * @param params TFTP communication parameters, including block size and timeout.
 * @return True if the ACK was successfully sent, otherwise False.
 */
bool sendAck(int sock, uint16_t blockID, const std::string &hostname, int serverPort, TFTPOparams &params);

/**
 * @brief Function to receive a file from the server.
 *
 * This function is used to receive a file from the server using the TFTP protocol. Communication is done
 * via RRQ and the reception of data packets. The received data is saved to a local file.
 *
 * @param sock The communication socket.
 * @param hostname The server's hostname.
 * @param port The server's port.
 * @param localFilePath The path to the local file where the received content should be saved.
 * @param remoteFilePath The remote file path on the server.
 * @param mode The transfer mode.
 * @param options Optional parameters for communication.
 * @param params TFTP communication parameters, including block size and timeout.
 * @return 0 if the transfer was successful, otherwise 1.
 */
int receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Function to parse optional TFTP parameters.
 *
 * This function is used to parse the string of optional parameters received from the server (OACK).
 * It recognizes parameters such as "blksize," "timeout," and "tsize" and stores them in the `TFTPOparams` structure.
 *
 * @param Oparamstring The string containing optional parameters.
 * @param Oparams The structure for storing optional parameters.
 * @return True if the parameters were successfully parsed, otherwise False.
 */
bool parseTFTPParameters(const std::string &Oparamstring, TFTPOparams &Oparams);
