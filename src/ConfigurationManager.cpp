/**
 * NMEATrax Configuration Manager Implementation
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes configuration management and reduces global variable usage.
 */

#include "ConfigurationManager.h"
#include <ArduinoJson.h>

// External preferences functions
extern bool readPreferences();
extern bool addWifiPair(const char* ssid, const char* password);
extern bool clearWifiCredentials();
extern Settings settings; // Will be replaced by this manager

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
    
    // Copy to global settings for backward compatibility
    // TODO: Remove this once all code uses ConfigurationManager
    settings = config;
    
    return success;
}

bool ConfigurationManager::loadFromStorage() {
    // Use existing readPreferences function
    bool success = readPreferences();
    if (success) {
        // Copy from global settings
        config = settings;
        validateSettings();
    }
    return success;
}

bool ConfigurationManager::saveToStorage() {
    // Update global settings for backward compatibility
    settings = config;
    
    // Settings are automatically saved when using updatePreference
    // This just validates the current state
    return validateSettings();
}

bool ConfigurationManager::setLocalAP(bool value) {
    config.isLocalAP = value;
    return updateSetting("isLocalAP", value);
}

bool ConfigurationManager::setWifiSSID(const String& ssid) {
    config.wifiSSID = ssid;
    return updateSetting("wifiSSID", ssid.c_str());
}

bool ConfigurationManager::setWifiPass(const String& password) {
    config.wifiPass = password;
    return updateSetting("wifiPass", password.c_str());
}

bool ConfigurationManager::setRecMode(RecMode mode) {
    config.recMode = mode;
    return updateSetting("recMode", (int)mode);
}

bool ConfigurationManager::setRecInterval(int interval) {
    if (interval < 1) {
        interval = 1;
    }
    config.recInt = interval;
    return updateSetting("recInt", interval);
}

bool ConfigurationManager::setWifiCredentials(const String& credentials) {
    config.wifiCredentials = credentials;
    return updateSetting("wifiCredentials", credentials.c_str());
}

bool ConfigurationManager::addWifiCredential(const String& ssid, const String& password) {
    return addWifiPair(ssid.c_str(), password.c_str());
}

bool ConfigurationManager::clearWifiCredentials() {
    bool success = ::clearWifiCredentials(); // Call global function
    if (success) {
        config.wifiCredentials = "";
    }
    return success;
}

bool ConfigurationManager::validateSettings() const {
    // Validate recording interval
    if (config.recInt < 1) {
        return false;
    }
    
    // Validate recording mode
    if (config.recMode < OFF || config.recMode > AUTO_RPM_IDLE) {
        return false;
    }
    
    // If not local AP, should have WiFi credentials
    if (!config.isLocalAP && config.wifiCredentials.isEmpty()) {
        Serial.println("Warning: Not in AP mode but no WiFi credentials configured");
    }
    
    return true;
}

void ConfigurationManager::setDefaults() {
    config.isLocalAP = true;
    config.wifiSSID = "NMEATrax";
    config.wifiPass = "password123";
    config.recMode = OFF;
    config.recInt = 1;
    config.wifiCredentials = "";
}

String ConfigurationManager::toJson() const {
    JsonDocument doc;
    doc["isLocalAP"] = config.isLocalAP;
    doc["wifiSSID"] = config.wifiSSID;
    doc["wifiPass"] = config.wifiPass;
    doc["recMode"] = (int)config.recMode;
    doc["recInt"] = config.recInt;
    doc["wifiCredentials"] = config.wifiCredentials;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool ConfigurationManager::fromJson(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.print("Failed to parse configuration JSON: ");
        Serial.println(error.c_str());
        return false;
    }
    
    config.isLocalAP = doc["isLocalAP"];
    config.wifiSSID = doc["wifiSSID"].as<String>();
    config.wifiPass = doc["wifiPass"].as<String>();
    config.recMode = (RecMode)doc["recMode"].as<int>();
    config.recInt = doc["recInt"];
    config.wifiCredentials = doc["wifiCredentials"].as<String>();
    
    return validateSettings();
}

template<typename T>
bool ConfigurationManager::updateSetting(const char* key, const T& value) {
    // Use existing updatePreference template function
    bool success = updatePreference(key, value);
    
    if (success) {
        // Update global settings for backward compatibility
        settings = config;
    }
    
    return success;
}