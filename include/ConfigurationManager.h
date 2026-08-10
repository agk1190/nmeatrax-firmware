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

// SD card recording state
enum RecMode {
    OFF = 0,
    ON = 1,
    AUTO_SPD = 2,
    AUTO_RPM = 3,
    AUTO_SPD_IDLE = 4,
    AUTO_RPM_IDLE = 5
};

class ConfigurationManager {
public:
    static ConfigurationManager& getInstance();
    
    // Initialization and persistence
    bool initialize();
    bool loadFromStorage();
    bool saveToStorage();
    
    // Device-hosted access point configuration
    bool isLocalAP() const { return localAP; }
    const String& getWifiSSID() const { return wifiSSID; }
    const String& getWifiPass() const { return wifiPass; }

    // External Wi-Fi access points as a JSON array string
    const String& getWifiCredentials() const { return wifiCredentials; }
    
    // Recording Configuration
    RecMode getRecMode() const { return recMode; }
    int getRecInterval() const { return recInterval; }
    
    // Individual setting setters with auto-save
    bool setLocalAP(bool value);
    bool setWifiSSID(const String& ssid);
    bool setWifiPass(const String& password);
    bool setRecMode(RecMode mode);
    bool setRecInterval(int interval);
    // bool setWifiCredentials(const String& credentialsJson);
    
    // External Wi-Fi credential-list management
    bool addWifiCredentialFromJson(const String& credentialJson);
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
    
    // Configuration data
    bool localAP;
    int commMode;
    String wifiSSID;
    String wifiPass;
    RecMode recMode;
    int recInterval;
    String wifiCredentials;
    
    bool initialized = false;
};

#endif // CONFIGURATION_MANAGER_H