/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax SD card header file.
 */

#include "SD.h"

/**
 * @brief Find the file requested.
 * @param fs Filesystem to look in
 * @param fileName The file name to look for
 * @returns True if file found. False if file not found.
*/
bool searchForFile(fs::FS &fs, const char * fileName);

/**
 * @brief Write data to a file. This will overwrite the file with the contents.
 * @param fs Filesystem to look in
 * @param path The path and file name to create
 * @param message The contents to write to the file
 * @param newLine New line at the end of the message (true/false)
 * @returns True if succeeded
*/
bool writeFile(fs::FS &fs, const char * path, const char * message, bool newLine);

/**
 * @brief Append data to a file. This will add to the existing file.
 * @param fs Filesystem to look in
 * @param path The path and file name to appened to
 * @param message The contents to write to the file
 * @param ensureCRLF Ensure the line ending is always CRLF
 * @returns True if succeeded
*/
bool appendFile(fs::FS &fs, const char * path, const char * message, bool ensureCRLF);

/**
 * @brief Delete all files in the path
 * @param fs The file system to look in
 * @param path The path and file name to delete
 * @returns True if succeeded
*/
bool deleteFile(fs::FS &fs, const char * path);

/**
 * @brief Gets a string with all files listed
 * @param fs The file system to look in
 * @param dirname The directory to look in
 * @param levels How many levels to go down
 * @returns A list of files as a String
*/
String listDir(fs::FS &fs, const char * dirname, uint8_t levels);

/**
 * @brief Get the contents of the file specified
 * @param fs Filesystem to look in
 * @param filePath The file to retrieve
 * @returns A string with the contents of the file
 * @link https://www.codeproject.com/Articles/5300719/ESP32-WEB-Server-with-Asynchronous-Technology-and
*/
String getFile(fs::FS &fs, String filePath);

/**
 * @brief Setup SD card
 * @returns True if succeeded
*/
bool sdSetup();

/**
 * @brief Ensure line ending is CRLF
 * @returns String with CRLF line ending
 * @author ChatGPT
*/
String addCRLF(const String& str);
