/**
 * NMEATrax Hardware Manager
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes hardware control and GPIO management.
 */

#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Arduino.h>

enum class LedState {
    OFF = LOW,
    ON = HIGH
};

enum class Led {
    POWER,
    N2K,
    SD
};

class HardwareManager {
public:
    static HardwareManager& getInstance();
    
    // Initialization
    bool initialize();
    
    // LED Control
    void setLed(Led led, LedState state);
    LedState getLedState(Led led) const;
    void toggleLed(Led led);
    
    // Power Management
    void setN2KStandby(bool standby);
    bool isN2KStandby() const;
    
    // SD Card Detection
    bool isSDCardPresent() const;
    int getSDCardStatus() const; // Returns status code used throughout the system
    
    // System Control
    void restart();
    void enterDeepSleep(uint64_t sleepTimeUs);
    
    // Hardware Status
    String getHardwareInfo() const;

private:
    HardwareManager() = default;
    ~HardwareManager() = default;
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;
    
    void initializePins();
    
    bool initialized = false;
    LedState powerLedState = LedState::OFF;
    LedState n2kLedState = LedState::OFF;
    LedState sdLedState = LedState::OFF;
    bool n2kStandby = false;
};

#endif // HARDWARE_MANAGER_H