/**
 * NMEATrax Communication Manager
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Manages WiFi and BLE communication modes with runtime switching capability.
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>

enum class CommunicationMode {
    BLE_ONLY,
    WIFI_ONLY,
    AUTO  // Auto-select based on configuration
};

class CommunicationManager {
public:
    static CommunicationManager& getInstance();
    
    bool initialize(CommunicationMode mode = CommunicationMode::AUTO);
    bool switchMode(CommunicationMode newMode);
    void sendData(const String& data);
    void sendSettings(const String& settingsJson);
    CommunicationMode getCurrentMode() const { return currentMode; }
    bool isWifiEnabled() const { return wifiEnabled; }
    bool isBleEnabled() const { return bleEnabled; }
    
    // Cleanup
    void shutdown();

private:
    CommunicationManager() = default;
    ~CommunicationManager() = default;
    CommunicationManager(const CommunicationManager&) = delete;
    CommunicationManager& operator=(const CommunicationManager&) = delete;
    
    bool initializeWifi();
    bool initializeBle();
    void shutdownWifi();
    void shutdownBle();
    
    CommunicationMode currentMode = CommunicationMode::BLE_ONLY;
    bool wifiEnabled = false;
    bool bleEnabled = false;
    bool initialized = false;
};

#endif // COMMUNICATION_MANAGER_H