/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax main header file.
 */

#include <Arduino.h>
#include <Arduino_JSON.h>

#define LED_PWR GPIO_NUM_32
#define LED_0183 GPIO_NUM_33
#define LED_N2K GPIO_NUM_25
#define LED_SD GPIO_NUM_14
#define SD_Detect GPIO_NUM_34
#define N2K_STBY GPIO_NUM_4

// SD card recording state
enum voyState {
    STOP = 0,
    RUN = 1,
    START = 2
};

/**
 * SD card logging mode
 * @param STOP
 * @param RUN
 * @param START
*/
extern enum voyState voyState;

struct settings
{
    String wifiMode;
    const char *wifiSSID;
    const char *wifiPass;
    int voyNum;
    int recInt;
    String depthUnit;
    String tempUnit;
    double timeZone;
};

/**
 * @brief Structure to store device settings
*/
typedef struct settings Settings;

/**
 * @brief Save current settings to the SPIFFS.
 * @return True if succeeded
*/
bool saveSettings();

/**
 * @brief Read current settings from the SPIFFS.
 * @return True if succeeded
*/
bool readSettings();

/**
 * @brief Compile NMEA data into a JSON string to send to the web server.
 * @param none
 * @return A string comtaining the NMEA data in a JSON format.
*/
String JSONValues();

/**
 * @brief Called when something fails
*/
void crash();