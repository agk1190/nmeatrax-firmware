/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax webserver header file.
 */

#include <WiFiManager.h>

// Wifi Manager class
extern WiFiManager wifiManager;

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
 * @brief Init websocket
*/
void initWebSocket();

/**
 * @brief Send data to websocket
 */
void sendDataTask(void *parameter);

/**
 * @brief Pass data to be send via websocket
 */
void sendToWebSocket(String data);
