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

// Funkce pro příjem potvrzovacího ACK packetu
bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr, sockaddr_in &serverAddr, int timeout);

// TFTP operace
const uint16_t RRQ = 1;
const uint16_t WRQ = 2;
const uint16_t DATA = 3;
const uint16_t ACK = 4;
const uint16_t ERROR = 5;
const uint16_t OACK = 6;

// Maximální velikost datového paketu
const size_t MAX_DATA_SIZE = 514;

// operační kody požadavků
const uint16_t OP_RRQ = 1;
const uint16_t OP_WRQ = 2;

// Chybové kódy TFTP
const uint16_t ERROR_UNDEFINED = 0;
const uint16_t ERROR_FILE_NOT_FOUND = 1;
const uint16_t ERROR_ACCESS_VIOLATION = 2;
const uint16_t ERROR_DISK_FULL = 3;
const uint16_t ERROR_ILLEGAL_OPERATION = 4;
const uint16_t ERROR_UNKNOWN_TRANSFER_ID = 5;
const uint16_t ERROR_FILE_ALREADY_EXISTS = 6;

// Struktura reprezentující TFTP paket
struct TFTPPacket
{
    uint16_t opcode;
    char data[MAX_DATA_SIZE];
};

bool lastblockfromoutside = false;
bool blocksizeOptionUsed = false;
bool timeoutOptionUsed = false;
bool transfersizeOptionUsed = false;

// Strukura pro uchování options
struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout;
    int transfersize;
};

/**
 * @brief Odešle chybový paket klientovi.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param errorCode Chybový kód TFTP.
 * @param errorMsg Textová zpráva chyby.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param serverAddr Struktura sockaddr_in reprezentující server.
 */
void sendError(int sockfd, uint16_t errorCode, const std::string &errorMsg, sockaddr_in &clientAddr, sockaddr_in &serverAddr);

/**
 * @brief Ověřuje existenci souboru na zadané cestě.
 *
 * @param filepath Cesta k souboru.
 * @return True, pokud soubor existuje, jinak False.
 */
bool fileExists(const std::string &filepath);

/**
 * @brief Zpracuje příchozí TFTP packet a provede příslušnou akci na základě opcode.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param opcode Opcode příchozího packetu.
 * @param serverAddr Struktura sockaddr_in reprezentující server.
 * @return 0 při úspěchu, jinak 1 při chybě.
 */
int handleIncomingPacket(int sockfd, sockaddr_in &clientAddr, int opcode, sockaddr_in &serverAddr);

/**
 * @brief Zkontroluje dostupný diskový prostor pro zápis souboru.
 *
 * @param size_of_file Velikost souboru, který se má zapsat.
 * @param path Cesta k umístění souboru na disku.
 * @return 0, pokud je dostatek místa, jinak chybový kód.
 */
uint16_t checkDiskSpace(int size_of_file, const std::string &path);

/**
 * @brief Odešle datový packet klientovi.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param blockNum Číslo bloku.
 * @param data Data, která se mají odeslat.
 * @param dataSize Velikost dat.
 * @param blockSize Velikost bloku.
 * @return True, pokud byl packet úspěšně odeslán, jinak False.
 */
bool sendDataPacket(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum, const char *data, size_t dataSize, uint16_t blockSize);

/**
 * @brief Odešle OACK (Option Acknowledgment) packet klientovi s volitelnými parametry.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param options_map Mapa s volitelnými parametry.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @param filesize Velikost souboru pro přenos.
 * @return True, pokud byl OACK packet úspěšně odeslán, jinak False.
 */
bool sendOACK(int sockfd, sockaddr_in &clientAddr, std::map<std::string, int> &options_map, TFTPOparams &params, std::streampos filesize);

/**
 * @brief Odešle data souboru klientovi v DATA packetech.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param serverAddr Struktura sockaddr_in reprezentující server.
 * @param filename Název souboru k odeslání.
 * @param options_map Mapa s volitelnými parametry.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @return True, pokud byla data úspěšně odeslána, jinak False.
 */
bool sendFileData(int sockfd, sockaddr_in &clientAddr, sockaddr_in &serverAddr, const std::string &filename, std::map<std::string, int> &options_map, TFTPOparams &params);

/**
 * @brief Přijme potvrzovací ACK packet od klienta.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param expectedBlockNum Očekávané číslo bloku.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param serverAddr Struktura sockaddr_in reprezentující server.
 * @param timeout Timeout pro příjem packetu.
 * @return True pokud byl potvrzovací ACK packet přijat, jinak False.
 */
bool receiveAck(int sockfd, uint16_t expectedBlockNum, sockaddr_in &clientAddr, sockaddr_in &serverAddr, int timeout);

/**
 * @brief Přijme DATA packet od klienta.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param serverAddr Struktura sockaddr_in reprezentující server.
 * @param expectedBlockNum Očekávané číslo bloku.
 * @param file Otevřený soubor pro zápis dat.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @return True, pokud byla data úspěšně přijata, jinak False.
 */
bool receiveDataPacket(int sockfd, sockaddr_in &clientAddr, sockaddr_in &serverAddr, uint16_t expectedBlockNum, std::ofstream &file, TFTPOparams &params);

/**
 * @brief Odešle ACK (Acknowledgment) packet klientovi.
 *
 * @param sockfd Socket pro TFTP přenos.
 * @param clientAddr Struktura sockaddr_in reprezentující klienta.
 * @param blockNum Číslo bloku k potvrzení.
 * @return True, pokud byl ACK packet úspěšně odeslán, jinak False.
 */
bool sendAck(int sockfd, sockaddr_in &clientAddr, uint16_t blockNum);

/**
 * @brief Zjišťuje přítomnost volitelných parametrů v request packetu.
 *
 * @param requestPacket TFTP request packet.
 * @param filename Název souboru z request packetu.
 * @param mode Režim přenosu z request packetu.
 * @param options_map Mapa s volitelnými parametry.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @return True, pokud byly nalezeny volitelné parametry, jinak False.
 */
bool hasOptions(TFTPPacket &requestPacket, std::string &filename, std::string &mode, std::map<std::string, int> &options_map, TFTPOparams &params);

/**
 * @brief Hlavní funkce TFTP serveru.
 *
 * @param port Port, na kterém server naslouchá.
 * @param root_dirpath Kořenový adresář serveru pro zpracování požadavků.
 */
void runTFTPServer(int port, const std::string &root_dirpath);

/**
 * @brief Funkce pro zachycení SIGINT signálu.
 *
 * @param signal Signál, který byl zachycen (např. SIGINT).
 */
void sigintHandler(int signal);

#endif // TFTP_SERVER_H
