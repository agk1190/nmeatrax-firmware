/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax webserver header file.
 */

#include <esp_wifi.h>

/**
 * @brief Look up the timezone definition based on the offset number provided
 * @param tz Timezone offset number. Integer between -11 and +14.
 * @returns The timezone string used to set the ESP32s internal timezone settings 
*/
const char* getTZdefinition(double tz);

/**
 * @brief Set up webserver functions
 * @returns True if succeeded
*/
bool webSetup();

/**
 * @brief Send new NMEA data
*/
void webLoop();

/**
 * @brief Send email progress
*/
void sendEmailData(String text);

/**
 * @brief Send data to client(s)
 */
void sendDataTask(void *parameter);

/**
 * @brief Pass data to the queue to be send to client(s)
 * @param data The data to be sent
 */
void sendToWebQueue(String data);
