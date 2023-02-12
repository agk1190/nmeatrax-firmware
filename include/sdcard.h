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
 * @param fileName The file name to look for
 * @returns True if file found. False if file not found.
*/
bool searchForFile(const char * fileName);

/**
 * @brief Write data to a file. This will overwrite the file with the contents.
 * @param path The path and file name to create
 * @param message The contents to write to the file
 * @param newLine New line at the end of the message (true/false)
 * @returns True if succeeded
*/
bool writeFile(const char * path, const char * message, bool newLine);

/**
 * @brief Append data to a file. This will add to the existing file.
 * @param path The path and file name to appened to
 * @param message The contents to write to the file
 * @param newLine New line at the end of the message (true/false)
 * @returns True if succeeded
*/
bool appendFile(const char * path, const char * message, bool newLine);

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
 * @param filePath The file to retrieve
 * @returns A string with the contents of the file
 * @link https://www.codeproject.com/Articles/5300719/ESP32-WEB-Server-with-Asynchronous-Technology-and
*/
String getFile(String filePath);

/**
 * @brief Write a point to the GPX file
 * @param fileName GPX filename
 * @param wptNum Waypoint number
 * @param lat Latitude
 * @param lon Longitude
 * @returns True if succeeded
*/
bool writeGPXpoint(const char * fileName, int wptNum, double lat, double lon);

/**
 * @brief Create and setup gpx file
 * @param fileName GPX filename
 * @param timeStamp Current time stamp
 * @returns True if succeeded
*/
bool createGPXfile(const char * fileName, const char * timeStamp);

/**
 * @brief Put a </gpx> at the end of the file
 * @param fileName GPX filename
 * @returns True if succeeded
*/
bool endGPXfile(const char * fileName);

/**
 * @brief Setup SD card
 * @returns True if succeeded
*/
bool sdSetup();
