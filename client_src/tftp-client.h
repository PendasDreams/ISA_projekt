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

// volitelné options
struct TFTPOparams
{
    uint16_t blksize;
    uint16_t timeout_max;
    int transfersize;
};

// Počáteční block ID
uint16_t blockID = 0;

// flags na rozpoznání jaké byly použity options
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

// typy požadavků
enum TFTPRequestType
{
    READ_REQUEST,
    WRITE_REQUEST
};

/**
 * @brief Funkce pro ověření, zda je soubor v ASCII formátu.
 *
 * Tato funkce zjišťuje, zda je zadaný soubor v ASCII formátu na základě jeho přípony.
 * Pokud je přípona "txt", "html" nebo "xml", soubor je považován za textový a je vrácena hodnota `true`.
 * V opačném případě je vrácena hodnota `false`, což indikuje binární formát.
 *
 * @param fileName Název souboru ke kontrole.
 * @return True, pokud je soubor v ASCII formátu, jinak False.
 */
bool isAscii(const std::string &fileName);

/**
 * @brief Funkce pro určení režimu TFTP na základě souboru.
 *
 * Tato funkce určuje režim přenosu TFTP ("netascii" nebo "octet") na základě obsahu souboru.
 * Pokud je soubor v ASCII formátu, je vrácen režim "netascii". V opačném případě je vrácen režim "octet".
 *
 * @param filePath Cesta k souboru pro určení režimu.
 * @return Režim TFTP pro daný soubor ("netascii" nebo "octet").
 */
std::string determineMode(const std::string &filePath);

/**
 * @brief Funkce pro převod hexadecimálního řetězce na řetězec znaků.
 *
 * Tato funkce převádí hexadecimální řetězec na řetězec znaků.
 *
 * @param hexStr Hexadecimální řetězec k převodu.
 * @return Řetězec znaků převedený z hexadecimálního řetězce.
 */
std::string hexStringToCharString(const std::string &hexStr);

/**
 * @brief Funkce pro nastavení timeoutu soketu.
 *
 * Tato funkce nastavuje timeout soketu na základě zadaného soketu a timeoutu.
 *
 * @param sock Socket pro nastavení timeoutu.
 * @param timeout Timeout v sekundách.
 * @return True, pokud byl timeout úspěšně nastaven, jinak False.
 */
bool setSocketTimeout(int sock, int timeout);

/**
 * @brief Funkce pro určení, zda je soubor v binárním formátu.
 *
 * Tato funkce zjišťuje, zda je zadaný soubor v binárním formátu na základě jeho přípony.
 * Pokud nemá soubor příponu nebo jeho přípona není v seznamu binárních formátů, je vrácena hodnota `true`.
 * Jinak je vrácena hodnota `false`, což indikuje textový formát.
 *
 * @param filename Název souboru ke kontrole.
 * @return True, pokud je soubor v binárním formátu, jinak False.
 */
bool isBinaryFormat(const std::string &filename);

/**
 * @brief Funkce pro zpracování chyb.
 *
 * Tato funkce slouží k vytvoření a odeslání ERROR packetu serveru, pokud došlo k chybě během komunikace.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Hostname serveru.
 * @param srcPort Zdrojový port klienta.
 * @param serverPort Port serveru.
 * @param errorCode Kód chyby.
 * @param errorMsg Textová zpráva popisující chybu.
 */
void handleError(int sock, const std::string &hostname, int srcPort, int serverPort, uint16_t errorCode, const std::string &errorMsg);

/**
 * @brief Funkce pro přijetí ACK (Acknowledgment) packetu a získání portu serveru.
 *
 * Tato funkce přijímá ACK packet a zároveň získává port serveru z odeslaného packetu.
 * Také získává volitelné parametry z OACK packetu, pokud je přijatý ACK packet označený jako OACK.
 *
 * @param sock Socket pro komunikaci.
 * @param receivedBlockID ID přijatého bloku dat.
 * @param serverPort Port serveru.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @param receivedOptions Mapa pro uložení volitelných parametrů OACK packetu.
 * @return True, pokud byl ACK úspěšně přijat a získána potřebná data, jinak False.
 */
bool receiveAck(int sock, uint16_t &receivedBlockID, int &serverPort, TFTPOparams &params, std::map<std::string, std::string> &receivedOptions);

/**
 * @brief Funkce pro odeslání datového packetu.
 *
 * Tato funkce vytváří a odesílá datový packet obsahující zadaná data na základě aktuálního bloku dat.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Hostname serveru.
 * @param port Port serveru.
 * @param data Data k odeslání.
 * @return True, pokud byl datový packet úspěšně odeslán, jinak False.
 */
bool sendData(int sock, const std::string &hostname, int port, const std::string &data);

/**
 * @brief Funkce pro odeslání souboru na server nebo nahrání ze vstupu.
 *
 * Tato funkce slouží k odeslání souboru na server nebo nahrání dat ze standardního vstupu (stdin).
 * Zároveň určuje režim TFTP na základě obsahu souboru a odesílá data v datových blocích na server.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Hostname serveru.
 * @param port Port serveru.
 * @param localFilePath Cesta k lokálnímu souboru pro odeslání nebo stdin pro načtení dat.
 * @param remoteFilePath Vzdálená cesta k souboru na serveru.
 * @param mode Režim TFTP pro přenos (netascii nebo octet).
 * @param options Volitelné parametry pro komunikaci s serverem.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @return 0, pokud byl přenos úspěšný, jinak 1.
 */
int SendFile(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Funkce pro odeslání RRQ (Read Request) packetu.
 *
 * Tato funkce vytváří a odesílá TFTP RRQ packet na základě zadaných parametrů. Typ packetu je určen
 * parametrem `requestType` (READ_REQUEST nebo WRITE_REQUEST). Packet obsahuje název souboru,
 * režim přenosu a volitelné parametry "blksize", "timeout" a "tsize", pokud jsou aktivní.
 *
 * @param requestType Typ requestu (READ_REQUEST nebo WRITE_REQUEST).
 * @param sock Socket pro komunikaci.
 * @param hostname Cílový server.
 * @param port Port, na který se má packet odeslat.
 * @param filepath Cesta k souboru na serveru.
 * @param mode Režim přenosu.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 */
bool sendTFTPRequest(TFTPRequestType requestType, int sock, const std::string &hostname, int port, const std::string &filepath, const std::string &mode, TFTPOparams &params);

/**
 * @brief Funkce pro příjem datového packetu.
 *
 * Tato funkce přijímá datový packet ze zadaného socketu a extrahuje z něj informace o
 * bloku dat a samotná data. Přijatá data jsou uložena do parametru `data`.
 *
 * @param sock Socket pro komunikaci.
 * @param receivedBlockID ID přijatého bloku dat.
 * @param serverPort Port serveru.
 * @param data Přijatá data.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @param hostname Hostname serveru.
 * @return True, pokud byla data úspěšně přijata, jinak False.
 */
bool receiveData(int sock, uint16_t &receivedBlockID, int &serverPort, std::string &data, TFTPOparams &params, const std::string &hostname);

/**
 * @brief Funkce pro odeslání potvrzení (ACK) serveru.
 *
 * Tato funkce vytváří a odesílá TFTP ACK packet s určeným blokem dat na základě přijatých dat.
 *
 * @param sock Socket pro komunikaci.
 * @param blockID ID bloku dat, který se má potvrdit.
 * @param hostname Hostname serveru.
 * @param serverPort Port serveru.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @return True, pokud bylo ACK úspěšně odesláno, jinak False.
 */
bool sendAck(int sock, uint16_t blockID, const std::string &hostname, int serverPort, TFTPOparams &params);

/**
 * @brief Funkce pro příjem souboru ze serveru.
 *
 * Tato funkce slouží k příjmu souboru ze serveru pomocí TFTP protokolu. Komunikace probíhá
 * pomocí RRQ a příjmu datových packetů. Přijatá data jsou ukládána do lokálního souboru.
 *
 * @param sock Socket pro komunikaci.
 * @param hostname Hostname serveru.
 * @param port Port serveru.
 * @param localFilePath Cesta k lokálnímu souboru, do kterého se má uložit přijatý obsah.
 * @param remoteFilePath Cesta k souboru na serveru.
 * @param mode Režim přenosu.
 * @param options Volitelné parametry pro komunikaci.
 * @param params Parametry TFTP komunikace, včetně velikosti bloku a timeoutu.
 * @return 0 pokud byl přenos úspěšný, jinak 1.
 */
int receive_file(int sock, const std::string &hostname, int port, const std::string &localFilePath, const std::string &remoteFilePath, std::string &mode, const std::string &options, TFTPOparams &params);

/**
 * @brief Funkce pro parsování volitelných parametrů TFTP.
 *
 * Tato funkce slouží k parsování řetězce volitelných parametrů přijatých od serveru (OACK).
 * Rozpoznává parametry "blksize", "timeout" a "tsize" a ukládá je do struktury `TFTPOparams`.
 *
 * @param Oparamstring Řetězec obsahující volitelné parametry.
 * @param Oparams Struktura pro ukládání volitelných parametrů.
 * @return True, pokud byly parametry úspěšně načteny, jinak False.
 */
bool parseTFTPParameters(const std::string &Oparamstring, TFTPOparams &Oparams);
