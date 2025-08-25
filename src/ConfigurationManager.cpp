/**
 * NMEATrax Configuration Manager Implementation
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes configuration management and reduces global variable usage.
 */

#include "ConfigurationManager.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"

// External preferences functions
extern bool addWifiPair(const char* ssid, const char* password);
extern bool clearWifiCredentials();

ConfigurationManager& ConfigurationManager::getInstance() {
    static ConfigurationManager instance;
    return instance;
}

bool ConfigurationManager::initialize() {
    if (initialized) {
        return true;
    }
    
    setDefaults();
    bool success = loadFromStorage();
    initialized = true;
    
    // Validate configuration after loading
    if (!validateSettings()) {
        Serial.println("Configuration validation failed, using defaults");
        setDefaults();
        saveToStorage();
    }
    
    return success;
}

bool ConfigurationManager::loadFromStorage() {
    Serial.println("Loading configuration from storage...");
    
    File file = SPIFFS.open("/prefs.txt", "r");
    if (!file) {
        Serial.println("Preferences file not found, using defaults");
        return false;
    }
    
    String fileContents = file.readString();
    file.close();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, fileContents);
    if (error) {
        Serial.print("Failed to parse preferences JSON: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Load settings from JSON
    if (doc["isLocalAP"].is<bool>()) {
        localAP = doc["isLocalAP"];
    }
    if (doc["wifiSSID"].is<const char*>()) {
        wifiSSID = doc["wifiSSID"].as<String>();
    }
    if (doc["wifiPass"].is<const char*>()) {
        wifiPass = doc["wifiPass"].as<String>();
    }
    if (doc["recMode"].is<int>()) {
        recMode = (RecMode)doc["recMode"].as<int>();
    }
    if (doc["recInt"].is<int>()) {
        recInterval = doc["recInt"];
    }
    if (doc["wifiCredentials"].is<const char*>()) {
        wifiCredentials = doc["wifiCredentials"].as<String>();
    }
    
    Serial.println("Configuration loaded successfully");
    return true;
}

bool ConfigurationManager::saveToStorage() {
    Serial.println("Saving configuration to storage...");
    
    JsonDocument doc;
    doc["isLocalAP"] = localAP;
    doc["wifiSSID"] = wifiSSID;
    doc["wifiPass"] = wifiPass;
    doc["recMode"] = (int)recMode;
    doc["recInt"] = recInterval;
    doc["wifiCredentials"] = wifiCredentials;
    
    String json;
    if (serializeJson(doc, json) == 0) {
        Serial.println("Failed to serialize configuration");
        return false;
    }
    
    File file = SPIFFS.open("/prefs.txt", "w");
    if (!file) {
        Serial.println("Failed to open preferences file for writing");
        return false;
    }
    
    file.print(json);
    file.close();
    
    Serial.println("Configuration saved successfully");
    return true;
}

bool ConfigurationManager::setLocalAP(bool value) {
    localAP = value;
    return updateSetting("isLocalAP", value);
}

bool ConfigurationManager::setWifiSSID(const String& ssid) {
    wifiSSID = ssid;
    return updateSetting("wifiSSID", ssid.c_str());
}

bool ConfigurationManager::setWifiPass(const String& password) {
    wifiPass = password;
    return updateSetting("wifiPass", password.c_str());
}

bool ConfigurationManager::setRecMode(RecMode mode) {
    recMode = mode;
    return updateSetting("recMode", (int)mode);
}

bool ConfigurationManager::setRecInterval(int interval) {
    if (interval < 1) {
        interval = 1;
    }
    recInterval = interval;
    return updateSetting("recInt", interval);
}

bool ConfigurationManager::setWifiCredentials(const String& credentials) {
    wifiCredentials = credentials;
    return updateSetting("wifiCredentials", credentials.c_str());
}

bool ConfigurationManager::addWifiCredential(const String& ssid, const String& password) {
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
        if (strcmp(wifiPair["ssid"], ssid.c_str()) == 0) {
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

bool ConfigurationManager::clearWifiCredentials() {
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

    file = SPIFFS.open("/prefs.txt", FILE_WRITE);
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to file (clear wifi credentials)");
        file.close();
        return false;
    }
    file.close();
    Serial.println("Wifi credentials cleared successfully");
    wifiCredentials = "";
    return true;
}

bool ConfigurationManager::validateSettings() const {
    // Validate recording interval
    if (recInterval < 1) {
        return false;
    }
    
    // Validate recording mode
    if (recMode < OFF || recMode > AUTO_RPM_IDLE) {
        return false;
    }
    
    // If not local AP, should have WiFi credentials
    if (!localAP && wifiCredentials.isEmpty()) {
        Serial.println("Warning: Not in AP mode but no WiFi credentials configured");
    }
    
    return true;
}

void ConfigurationManager::setDefaults() {
    localAP = true;
    wifiSSID = "NMEATrax";
    wifiPass = "12345678";
    recMode = OFF;
    recInterval = 5;
    wifiCredentials = "";
    
    Serial.println("Configuration set to defaults");
}

String ConfigurationManager::toJson() const {
    JsonDocument doc;
    
    doc["isLocalAP"] = localAP;
    doc["wifiSSID"] = wifiSSID;
    doc["wifiPass"] = wifiPass;
    doc["recMode"] = (int)recMode;
    doc["recInt"] = recInterval;
    doc["wifiCredentials"] = wifiCredentials;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool ConfigurationManager::fromJson(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("JSON parsing error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Update configuration from JSON
    if (doc["isLocalAP"].is<bool>()) {
        localAP = doc["isLocalAP"];
    }
    if (doc["wifiSSID"].is<const char*>()) {
        wifiSSID = doc["wifiSSID"].as<String>();
    }
    if (doc["wifiPass"].is<const char*>()) {
        wifiPass = doc["wifiPass"].as<String>();
    }
    if (doc["recMode"].is<int>()) {
        recMode = (RecMode)doc["recMode"].as<int>();
    }
    if (doc["recInt"].is<int>()) {
        recInterval = doc["recInt"];
    }
    if (doc["wifiCredentials"].is<const char*>()) {
        wifiCredentials = doc["wifiCredentials"].as<String>();
    }
    
    return validateSettings();
}

template<typename T>
bool ConfigurationManager::updateSetting(const char* key, const T& value) {
    JsonDocument doc;
    
    File file = SPIFFS.open("/prefs.txt", "r");
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
        return false;
    }

    doc[key] = value;

    if (serializeJson(doc, fileContents) == 0) {
        Serial.println("Failed to create JSON during update");
        return false;
    }

    file = SPIFFS.open("/prefs.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing during update");
        return false;
    }
    file.print(fileContents);
    file.close();
    Serial.println("Preferences updated successfully");
    return true;
}