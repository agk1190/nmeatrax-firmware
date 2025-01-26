/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax main header file.
 */

#include <Arduino.h>
// #include <Arduino_JSON.h>

#define LED_PWR GPIO_NUM_32
#define LED_N2K GPIO_NUM_25
#define LED_SD GPIO_NUM_14
#define SD_Detect GPIO_NUM_34
#define N2K_STBY GPIO_NUM_4

#define FW_VERSION "10.0.0"
#define UI_VERSION1 "3.1.0"

extern bool outOfIdle;
extern bool NMEAsleep;
extern String nmeaData[];

// SD card recording state
enum recMode {
    OFF = 0,
    ON = 1,
    AUTO_SPD = 2,
    AUTO_RPM = 3,
    AUTO_SPD_IDLE = 4,
    AUTO_RPM_IDLE = 5
};

/**
 * SD card logging mode
 * @param OFF
 * @param ON
 * @param AUTO_SPD
 * @param AUTO_RPM
 * @param AUTO_SPD_IDLE
 * @param AUTO_RPM_IDLE
*/
extern enum recMode recMode;

struct settings {
    bool isLocalAP;
    const char *wifiSSID;
    const char *wifiPass;
    int recMode;
    int recInt;
    const char *wifiCredentials;
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

// /**
//  * @brief Called when something fails
// */
// void crash();

/**
 * @brief Check if SD card is present.
 * @return True if present
*/
bool getSDcardStatus();

void vWebTask(void * pvParameters);

void vBackgroundTasks(void * pvParameters);

void vNmeaTask(void * pvParameters);