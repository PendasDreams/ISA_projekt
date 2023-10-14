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
#include <algorithm> // Pro převod názvu souboru na malá písmena
#include <iomanip>   // for std::hex
#include <fcntl.h>

enum class RequestType
{
    READ,
    WRITE
};

/**
 * @brief Přijme ACK (Acknowledgment) packet a získá informace o serverovém portu.
 *
 * @param sock Socket pro komunikaci.
 * @param receivedBlockID ID bloku, který byl potvrzen.
 * @param serverPort Port, na kterém server komunikuje.
 * @return True, pokud byl ACK úspěšně přijat; jinak false.
 */
bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort);

/**
 * @brief Odešle DATA packet.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Adresa hostitele.
 * @param port Port pro TFTP komunikaci.
 * @param data Data k odeslání.
 */
void sendData(int sock, const std::string &hostname, int port, const std::string &data);

/**
 * @brief Zpracuje chybu a odešle ERROR packet.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Adresa hostitele.
 * @param srcPort Port odesílatele.
 * @param dstPort Port příjemce.
 * @param errorCode Kód chyby.
 * @param errorMsg Zpráva chyby.
 */
void handleError(int sock, const std::string &hostname, int srcPort, int dstPort, uint16_t errorCode, const std::string &errorMsg);

/**
 * @brief Odešle soubor na server nebo nahrává ze stdin.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Adresa hostitele.
 * @param port Port pro TFTP komunikaci.
 * @param localFilePath Lokální cesta k souboru pro odeslání nebo cílový soubor pro nahrání.
 * @param remoteFilePath Cílová cesta na vzdáleném serveru.
 * @param mode Režim komunikace (např. "netascii" nebo "octet").
 * @param options Volitelné parametry (OACK).
 */
// void SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options);

/**
 * @brief Přijme DATA packet.
 *
 * @param sock Socket pro komunikaci.
 * @param receivedBlockID ID bloku, který byl přijat.
 * @param data Přijatá data.
 * @return True, pokud byla data úspěšně přijata; jinak false.
 */
bool receiveData(int sock, uint16_t &receivedBlockID, std::string &data);

/**
 * @brief Přijme soubor od serveru.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Adresa hostitele.
 * @param port Port pro TFTP komunikaci.
 * @param localFilePath Lokální cesta k souboru pro uložení přijatých dat.
 * @param remoteFilePath Cílová cesta na vzdáleném serveru.
 * @param mode Režim komunikace (např. "netascii" nebo "octet").
 * @param options Volitelné parametry (OACK).
 */
void receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, const std::string &mode, const std::string &options);

/**
 * @brief Zjistí, zda je soubor pravděpodobně v ASCII formátu.
 *
 * @param filePath Cesta k souboru.
 * @return True, pokud je soubor pravděpodobně v ASCII formátu; jinak false.
 */
bool isAscii(const std::string &filePath);

/**
 * @brief Určí režim komunikace ("netascii" nebo "octet") na základě obsahu souboru.
 *
 * @param filePath Cesta k souboru.
 * @return Režim komunikace.
 */
std::string determineMode(const std::string &filePath);

void sendRequest(int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, const std::string &options, RequestType requestType);
