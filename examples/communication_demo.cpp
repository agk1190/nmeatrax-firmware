/**
 * NMEATrax Communication Mode Demo
 * 
 * This example demonstrates how to use the new CommunicationManager
 * to switch between WiFi and BLE modes at runtime.
 */

#include "CommunicationManager.h"
#include "ConfigurationManager.h"
#include "HardwareManager.h"

void setup() {
    Serial.begin(115200);
    Serial.println("NMEATrax Communication Mode Demo");
    
    // Initialize managers
    HardwareManager& hardware = HardwareManager::getInstance();
    ConfigurationManager& config = ConfigurationManager::getInstance();
    CommunicationManager& comm = CommunicationManager::getInstance();
    
    hardware.initialize();
    config.initialize();
    
    // Demo: Start with BLE mode
    Serial.println("Starting with BLE mode...");
    comm.initialize(CommunicationMode::BLE_ONLY);
    
    // Send test data
    comm.sendData("{\"demo\": \"BLE mode active\"}");
    
    delay(5000);
    
    // Demo: Switch to WiFi mode if credentials available
    if (!config.getWifiCredentials().isEmpty() || config.isLocalAP()) {
        Serial.println("Switching to WiFi mode...");
        if (comm.switchMode(CommunicationMode::WIFI_ONLY)) {
            Serial.println("WiFi mode active");
            comm.sendData("{\"demo\": \"WiFi mode active\"}");
        } else {
            Serial.println("Failed to switch to WiFi mode");
        }
    }
    
    delay(5000);
    
    // Demo: Switch to auto mode
    Serial.println("Switching to AUTO mode...");
    comm.switchMode(CommunicationMode::AUTO);
    comm.sendData("{\"demo\": \"AUTO mode active\"}");
    
    // Show status
    Serial.println("Communication Status:");
    Serial.printf("  Current Mode: %d\n", (int)comm.getCurrentMode());
    Serial.printf("  WiFi Enabled: %s\n", comm.isWifiEnabled() ? "Yes" : "No");
    Serial.printf("  BLE Enabled: %s\n", comm.isBleEnabled() ? "Yes" : "No");
}

void loop() {
    // Demo: Periodically send data
    static unsigned long lastSend = 0;
    static int counter = 0;
    
    if (millis() - lastSend > 10000) { // Every 10 seconds
        CommunicationManager& comm = CommunicationManager::getInstance();
        
        String demoData = "{\"counter\": " + String(counter++) + 
                         ", \"timestamp\": " + String(millis()) + "}";
        
        comm.sendData(demoData);
        Serial.println("Sent: " + demoData);
        
        lastSend = millis();
    }
    
    delay(100);
}