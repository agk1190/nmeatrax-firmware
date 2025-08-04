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

// /**
//  * @brief Sends settings over BLE.
//  * @param settings The settings to send.
//  */
// void sendSettings(String &settings);

// /**
//  * @brief Sends download data over BLE.
//  * @param data The download data to send.
//  */
// void sendDownloadData(String &data);