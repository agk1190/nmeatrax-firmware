#include <Arduino.h>

/**
 * @brief Setup Wifi connection based on the settings
 * @returns True if succeeded
 */
bool wifiSetup();

/**
 * @brief Connect to Wi-Fi using the provided JSON string containing SSID and password
 * @param jsonString JSON string with Wi-Fi credentials
 * @returns True if connected successfully, false otherwise
 */
bool connectToWiFi(String jsonString);

/**
 * @brief Create a text file with the current Wi-Fi IP and MAC address
 */
void createWifiText();

String getMacAddress();