#include <Arduino.h>

/**
 * @brief NMEATrax BLE header file.
 */

// extern String settingsJson;

/**
 * @brief Initializes the BLE service and characteristic.
 */
void bleSetup();

/**
 * @brief Sends a message over BLE.
 * @param message The message to send.
 */
void sendBLEmessage(String &message);

/**
 * @brief Sends settings over BLE.
 */
void notifySettingsJson();

/**
 * @brief Sends the list of downloadable files over BLE.
 */
void notifyDownloadsList();

/**
 * @brief Sends a file over BLE.
 * @param fileName The name of the file to send.
 */
void sendFileOverBLE(const char *fileName);