/**
 * NMEATrax Communication Manager Implementation
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Manages WiFi and BLE communication modes with runtime switching capability.
 */

#include "CommunicationManager.h"
#include "nmeaWifi.h"
#include "nmeaBLE.h"
#include "preferences.h"
#include "webserv.h"

// External references to existing functions
extern bool wifiSetup();
extern void bleSetup();
extern void sendBLEmessage(String &message);

CommunicationManager& CommunicationManager::getInstance() {
    static CommunicationManager instance;
    return instance;
}

bool CommunicationManager::initialize(CommunicationMode mode) {
    if (initialized) {
        return true;
    }
    
    currentMode = mode;
    bool success = false;
    
    switch (mode) {
        case CommunicationMode::BLE_ONLY:
            success = initializeBle();
            break;
            
        case CommunicationMode::WIFI_ONLY:
            success = initializeWifi();
            break;
            
        case CommunicationMode::AUTO:
            // Auto-select based on configuration
            // Default to BLE, enable WiFi if configured
            success = initializeBle();
            if (settings.isLocalAP || !settings.wifiCredentials.isEmpty()) {
                // Also initialize WiFi if credentials are available
                if (initializeWifi()) {
                    currentMode = CommunicationMode::WIFI_ONLY;
                }
            }
            break;
    }
    
    initialized = success;
    return success;
}

bool CommunicationManager::switchMode(CommunicationMode newMode) {
    if (newMode == currentMode) {
        return true;
    }
    
    Serial.print("Switching communication mode from ");
    Serial.print((int)currentMode);
    Serial.print(" to ");
    Serial.println((int)newMode);
    
    // Shutdown current mode
    if (wifiEnabled) {
        shutdownWifi();
    }
    if (bleEnabled) {
        shutdownBle();
    }
    
    // Initialize new mode
    currentMode = newMode;
    return initialize(newMode);
}

void CommunicationManager::sendData(const String& data) {
    if (bleEnabled) {
        String dataCopy = data;  // sendBLEmessage expects non-const reference
        sendBLEmessage(dataCopy);
    }
    
    if (wifiEnabled) {
        // Send to web queue (existing function)
        sendToWebQueue(data);
    }
}

void CommunicationManager::sendSettings(const String& settingsJson) {
    sendData(settingsJson);
}

void CommunicationManager::shutdown() {
    shutdownWifi();
    shutdownBle();
    initialized = false;
}

bool CommunicationManager::initializeWifi() {
    if (wifiEnabled) {
        return true;
    }
    
    Serial.println("Initializing WiFi communication...");
    bool success = wifiSetup();
    if (success) {
        wifiEnabled = true;
        Serial.println("WiFi communication initialized successfully");
    } else {
        Serial.println("Failed to initialize WiFi communication");
    }
    return success;
}

bool CommunicationManager::initializeBle() {
    if (bleEnabled) {
        return true;
    }
    
    Serial.println("Initializing BLE communication...");
    bleSetup();
    bleEnabled = true;
    Serial.println("BLE communication initialized successfully");
    return true;
}

void CommunicationManager::shutdownWifi() {
    if (!wifiEnabled) {
        return;
    }
    
    Serial.println("Shutting down WiFi communication...");
    // Note: WiFi shutdown would need to be implemented in nmeaWifi.cpp
    // For now, just mark as disabled
    wifiEnabled = false;
}

void CommunicationManager::shutdownBle() {
    if (!bleEnabled) {
        return;
    }
    
    Serial.println("Shutting down BLE communication...");
    // Note: BLE shutdown would need to be implemented in nmeaBLE.cpp
    // For now, just mark as disabled
    bleEnabled = false;
}