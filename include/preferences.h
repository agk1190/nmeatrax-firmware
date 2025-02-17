/**
 * NMEATrax
 * 
 * @authors Alex Klouda
 * 
 * NMEATrax email header file.
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJSON.h>

/**
 * @brief Structure to store device settings
*/
struct settings {
    bool isLocalAP;
    String wifiSSID;
    String wifiPass;
    int recMode;
    int recInt;
    String wifiCredentials;
};

typedef struct settings Settings;

/**
 * @brief Update a value in the preferences file
 * @param key The key to update
 * @param value The value to update
 * @returns True if succeeded
 */
template <typename T>
bool updatePreference(const char* key, T value) {
    JsonDocument doc;
    
    File file = SPIFFS.open("/prefs.txt", "rw");
    if (!file) {
        Serial.println("Failed to read file during write");
        return false;
    }
    String fileContents = file.readString();
    file.close();
    
    DeserializationError error = deserializeJson(doc, fileContents);
    if (error) {
        Serial.print("Failed to parse JSON during update: ");
        Serial.println(error.c_str());
        file.close();
        return false;
    }

    doc[key] = value;

    if (serializeJson(doc, fileContents) == 0) {
        Serial.println("Failed to create JSON during update");
        file.close();
        return false;
    }

    file = SPIFFS.open("/prefs.txt", "w");
    file.print(fileContents);
    file.close();
    Serial.println("Preferences updated successfully");
    return true;
}

/**
 * @brief Read the preferences file
 * @returns True if succeeded
 */
bool readPreferences();

/**
 * @brief Add a wifi pair to the preferences file
 * @param ssid The wifi ssid
 * @param password The wifi password
 * @returns True if succeeded
 */
bool addWifiPair(const char* ssid, const char* password);

// /**
//  * @brief Remove a wifi pair from the preferences file
//  * @param ssid The wifi ssid
//  * @returns True if succeeded
//  */
// bool removeWifiPair(const char* ssid);

/**
 * @brief Clear the wifi credentials from the preferences file
 * @returns True if succeeded
 */
bool clearWifiCredentials();

#endif // PREFERENCES_H