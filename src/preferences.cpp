/**
 * NMEATrax
 * 
 * NMEATrax SPIFFS preferences functions
*/

#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJSON.h>
#include "preferences.h"

// Structure to store device settings
Settings settings;

bool readPreferences() {
    JsonDocument doc;

    File file = SPIFFS.open("/prefs.txt", FILE_READ);
    if (!file) {
        Serial.println("Failed to read file during read");
        return false;
    }
    String fileContents = file.readString();
    file.close();

    DeserializationError error = deserializeJson(doc, fileContents);
    if (error) {
        Serial.print("Failed to parse JSON during read: ");
        Serial.println(error.c_str());
        return false;
    }

    settings.isLocalAP = doc["isLocalAP"];
    settings.wifiSSID = doc["wifiSSID"].as<String>();
    settings.wifiPass = doc["wifiPass"].as<String>();
    settings.recMode = doc["recMode"];
    settings.recInt = doc["recInt"];
    settings.wifiCredentials = doc["wifiCredentials"].as<String>();

    Serial.println("Preferences read successfully");
    return true;
}

bool addWifiPair(const char* ssid, const char* password) {
    JsonDocument doc;

    File file = SPIFFS.open("/prefs.txt", FILE_READ);
    String fileContents = file.readString();
    file.close();

    DeserializationError error = deserializeJson(doc, fileContents);
    if (error) {
        Serial.println("Failed to parse JSON (addWifiPair):");
        Serial.println(error.c_str());
        return false;
    }

    if (!doc["wifiCredentials"].is<JsonArray>()) {
        JsonArray wifiArray = doc["wifiCredentials"].as<JsonArray>();
        wifiArray.clear();
    }

    JsonArray wifiArray = doc["wifiCredentials"].as<JsonArray>();

    bool found = false;
    for (JsonObject wifiPair : wifiArray) {
        if (strcmp(wifiPair["ssid"], ssid) == 0) {
            wifiPair["password"] = password;
            found = true;
            break;
        }
    }

    if (!found) {
        JsonObject newWifiPair = wifiArray.add<JsonObject>();
        newWifiPair["ssid"] = ssid;
        newWifiPair["password"] = password;
    }

    settings.wifiCredentials = doc["wifiCredentials"].as<String>();

    file = SPIFFS.open("/prefs.txt", FILE_WRITE);
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to file (addWifiPair)");
        file.close();
        return false;
    }
    file.close();
    Serial.println("Wifi pair added successfully");
    return true;
}

bool clearWifiCredentials() {
    JsonDocument doc;
    Serial.println("Clearing Wifi credentials");

    File file = SPIFFS.open("/prefs.txt", FILE_READ);
    String fileContents = file.readString();
    file.close();

    DeserializationError error = deserializeJson(doc, fileContents);
    if (error) {
        Serial.println("Failed to parse JSON (clear wifi credentials):");
        Serial.println(error.c_str());
        return false;
    }

    JsonArray wifiArray = doc["wifiCredentials"].as<JsonArray>();
    wifiArray.clear();
    settings.wifiCredentials = doc["wifiCredentials"].as<String>();

    file = SPIFFS.open("/prefs.txt", FILE_WRITE);
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to file (clear wifi credentials)");
        file.close();
        return false;
    }
    file.close();
    Serial.println("Wifi credentials cleared successfully");
    return true;
}