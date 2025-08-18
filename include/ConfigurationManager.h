/**
 * NMEATrax Configuration Manager
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes configuration management and reduces global variable usage.
 */

#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <Arduino.h>
#include "preferences.h"

class ConfigurationManager {
public:
    static ConfigurationManager& getInstance();
    
    // Initialization and persistence
    bool initialize();
    bool loadFromStorage();
    bool saveToStorage();
    
    // Settings accessors
    const Settings& getSettings() const { return config; }
    Settings& getSettingsRef() { return config; }
    
    // Individual setting getters
    bool isLocalAP() const { return config.isLocalAP; }
    const String& getWifiSSID() const { return config.wifiSSID; }
    const String& getWifiPass() const { return config.wifiPass; }
    RecMode getRecMode() const { return config.recMode; }
    int getRecInterval() const { return config.recInt; }
    const String& getWifiCredentials() const { return config.wifiCredentials; }
    
    // Individual setting setters with auto-save
    bool setLocalAP(bool value);
    bool setWifiSSID(const String& ssid);
    bool setWifiPass(const String& password);
    bool setRecMode(RecMode mode);
    bool setRecInterval(int interval);
    bool setWifiCredentials(const String& credentials);
    
    // WiFi credential management
    bool addWifiCredential(const String& ssid, const String& password);
    bool clearWifiCredentials();
    
    // Validation
    bool validateSettings() const;
    void setDefaults();
    
    // JSON serialization
    String toJson() const;
    bool fromJson(const String& json);

private:
    ConfigurationManager() = default;
    ~ConfigurationManager() = default;
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    
    Settings config;
    bool initialized = false;
    
    // Helper for updating individual preferences
    template<typename T>
    bool updateSetting(const char* key, const T& value);
};

#endif // CONFIGURATION_MANAGER_H