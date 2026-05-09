/**
 * NMEATrax Configuration Manager Implementation
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes configuration management and reduces global variable usage.
 * All setters update the in-memory state and then delegate to saveToStorage(),
 * which is the single function that writes to the filesystem.
 */

#include "ConfigurationManager.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"

namespace {

// Normalize station credential list to a JSON array string.
String normalizeStationCredentialsJson(const String& raw) {
    if (raw.isEmpty()) {
        return "[]";
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, raw);
    if (error || !doc.is<JsonArray>()) {
        return "[]";
    }

    String normalized;
    serializeJson(doc, normalized);
    return normalized;
}

String readStationCredentialsFromDoc(JsonDocument& doc) {
    if (doc["wifiCredentials"].is<JsonArray>()) {
        String json;
        serializeJson(doc["wifiCredentials"], json);
        return normalizeStationCredentialsJson(json);
    }

    if (doc["wifiCredentials"].is<const char*>()) {
        return normalizeStationCredentialsJson(doc["wifiCredentials"].as<String>());
    }

    return "[]";
}

void writeStationCredentialsToDoc(JsonDocument& doc, const String& credentialsJson) {
    JsonArray outArray = doc["wifiCredentials"].to<JsonArray>();

    JsonDocument credsDoc;
    DeserializationError credsError = deserializeJson(credsDoc, credentialsJson);
    if (credsError || !credsDoc.is<JsonArray>()) {
        return;
    }

    for (JsonObject cred : credsDoc.as<JsonArray>()) {
        JsonObject outCred = outArray.add<JsonObject>();
        outCred["ssid"] = cred["ssid"] | "";
        outCred["password"] = cred["password"] | "";
    }
}

}

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
        Serial.print("Failed to parse preferences JSON (loadFromStorage): ");
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
    wifiCredentials = readStationCredentialsFromDoc(doc);
    
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
    writeStationCredentialsToDoc(doc, wifiCredentials);
    
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

    Serial.println("Configuration JSON to save:" + json);
    
    file.print(json);
    file.close();
    
    Serial.println("Configuration saved successfully");
    return true;
}

bool ConfigurationManager::setLocalAP(bool value) {
    localAP = value;
    return saveToStorage();
}

bool ConfigurationManager::setWifiSSID(const String& ssid) {
    wifiSSID = ssid;
    return saveToStorage();
}

bool ConfigurationManager::setWifiPass(const String& password) {
    wifiPass = password;
    return saveToStorage();
}

bool ConfigurationManager::setRecMode(RecMode mode) {
    if (mode < 0 || mode > 5) {
        Serial.println("Invalid recording mode");
        return false;
    }
    recMode = mode;
    Serial.printf("Recording mode set to %d\n", mode);
    return saveToStorage();
}

bool ConfigurationManager::setRecInterval(int interval) {
    if (interval < 1) {
        interval = 1;
    }
    recInterval = interval;
    return saveToStorage();
}

// bool ConfigurationManager::setWifiCredentials(const String& credentialsJson) {
//     wifiCredentials = normalizeStationCredentialsJson(credentialsJson);
//     return saveToStorage();
// }

bool ConfigurationManager::addWifiCredentialFromJson(const String& credentialJson) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, credentialJson);
    if (error || !doc.is<JsonObject>()) {
        return false;
    }

    if (!doc["ssid"].is<const char*>() || !doc["password"].is<const char*>()) {
        return false;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    Serial.println("Adding Wi-Fi credential from JSON: " + ssid);

    return addWifiCredential(ssid, password);
}

bool ConfigurationManager::addWifiCredential(const String& ssid, const String& password) {
    if (ssid.isEmpty()) {
        return false;
    }

    Serial.println("Adding Wi-Fi credential: " + ssid);

    JsonDocument credsDoc;
    DeserializationError error = deserializeJson(credsDoc, wifiCredentials);
    if (error || !credsDoc.is<JsonArray>()) {
        credsDoc.to<JsonArray>();
    }

    JsonArray wifiArray = credsDoc.as<JsonArray>();

    serializeJson(wifiArray, Serial);

    // Update password if SSID already exists, otherwise append.
    bool found = false;
    for (JsonObject entry : wifiArray) {
        if (entry["ssid"].as<String>() == ssid) {
            entry["password"] = password;
            found = true;
            break;
        }
    }

    if (!found) {
        JsonObject newEntry = wifiArray.add<JsonObject>();
        newEntry["ssid"] = ssid;
        newEntry["password"] = password;
    }

    serializeJson(credsDoc, wifiCredentials);
    Serial.println("Updated Wi-Fi credentials: " + wifiCredentials);
    return saveToStorage();
}

bool ConfigurationManager::clearWifiCredentials() {
    Serial.println("Clearing Wi-Fi credentials");
    wifiCredentials = "[]";
    return saveToStorage();
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
    wifiPass = "nmeatrax";
    recMode = OFF;
    recInterval = 5;
    wifiCredentials = "[]";
    
    Serial.println("Configuration set to defaults");
}

String ConfigurationManager::toJson() const {
    JsonDocument doc;
    
    doc["isLocalAP"] = localAP;
    doc["wifiSSID"] = wifiSSID;
    doc["wifiPass"] = wifiPass;
    doc["recMode"] = (int)recMode;
    doc["recInt"] = recInterval;
    writeStationCredentialsToDoc(doc, wifiCredentials);
    
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
    wifiCredentials = readStationCredentialsFromDoc(doc);

    return validateSettings();
}
