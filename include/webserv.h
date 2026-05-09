/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax webserver header file.
 */

#include <Arduino.h>

/**
 * @brief Set up webserver functions
 * @returns True if succeeded
*/
bool webSetup();

/**
 * @brief Host the SD card on the webserver
*/
void hostSdCard();

/**
 * @brief Send new NMEA data
*/
void webLoop();

/**
 * @brief Send data to client(s)
 */
void sendDataTask(void *parameter);

/**
 * @brief Pass data to the queue to be send to client(s)
 * @param data The data to be sent
 */
void sendToWebQueue(String data);

/**
 * @brief Make a JSON string of the settings
 * @returns JSON string of settings
 */
String makeSettingsJson();

void startOTAupdate();