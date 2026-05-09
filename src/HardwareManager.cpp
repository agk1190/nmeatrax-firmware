/**
 * NMEATrax Hardware Manager Implementation
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes hardware control and GPIO management.
 */

#include "HardwareManager.h"

// External function reference
extern bool getSDcardStatus();

HardwareManager& HardwareManager::getInstance() {
    static HardwareManager instance;
    return instance;
}

bool HardwareManager::initialize() {
    if (initialized) {
        return true;
    }
    
    initializePins();
    
    // Set initial states
    setLed(Led::POWER, LedState::ON);
    setLed(Led::N2K, LedState::OFF);
    setLed(Led::SD, LedState::OFF);
    setN2KStandby(false);
    
    initialized = true;
    Serial.println("Hardware Manager initialized");
    return true;
}

void HardwareManager::setLed(Led led, LedState state) {
    gpio_num_t pin;
    LedState* stateVar;
    
    switch (led) {
        case Led::POWER:
            pin = LED_PWR;
            stateVar = &powerLedState;
            break;
        case Led::N2K:
            pin = LED_N2K;
            stateVar = &n2kLedState;
            break;
        case Led::SD:
            pin = LED_SD;
            stateVar = &sdLedState;
            break;
        default:
            return;
    }
    
    digitalWrite(pin, (int)state);
    *stateVar = state;
}

LedState HardwareManager::getLedState(Led led) const {
    switch (led) {
        case Led::POWER: return powerLedState;
        case Led::N2K: return n2kLedState;
        case Led::SD: return sdLedState;
        default: return LedState::OFF;
    }
}

void HardwareManager::toggleLed(Led led) {
    LedState currentState = getLedState(led);
    LedState newState = (currentState == LedState::ON) ? LedState::OFF : LedState::ON;
    setLed(led, newState);
}

void HardwareManager::setN2KStandby(bool standby) {
    digitalWrite(N2K_STBY, standby ? HIGH : LOW);
    n2kStandby = standby;
}

bool HardwareManager::isN2KStandby() const {
    return n2kStandby;
}

bool HardwareManager::isSDCardPresent() const {
    return digitalRead(SD_Detect) == LOW; // Assuming LOW means card present
}

int HardwareManager::getSDCardStatus() const {
    // Use existing function to maintain compatibility
    return getSDcardStatus();
}

void HardwareManager::restart() {
    Serial.println("Hardware restart requested");
    ESP.restart();
}

void HardwareManager::enterDeepSleep(uint64_t sleepTimeUs) {
    Serial.print("Entering deep sleep for ");
    Serial.print(sleepTimeUs);
    Serial.println(" microseconds");
    esp_deep_sleep(sleepTimeUs);
}

String HardwareManager::getHardwareInfo() const {
    String info = "Hardware Status:\n";
    info += "  Power LED: " + String((powerLedState == LedState::ON) ? "ON" : "OFF") + "\n";
    info += "  N2K LED: " + String((n2kLedState == LedState::ON) ? "ON" : "OFF") + "\n";
    info += "  SD LED: " + String((sdLedState == LedState::ON) ? "ON" : "OFF") + "\n";
    info += "  N2K Standby: " + String(n2kStandby ? "YES" : "NO") + "\n";
    info += "  SD Card Present: " + String(isSDCardPresent() ? "YES" : "NO") + "\n";
    info += "  Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
    info += "  Chip Model: " + String(ESP.getChipModel()) + "\n";
    return info;
}

void HardwareManager::initializePins() {
    pinMode(LED_PWR, OUTPUT);
    pinMode(LED_N2K, OUTPUT);
    pinMode(LED_SD, OUTPUT);
    pinMode(SD_Detect, INPUT_PULLUP);
    pinMode(N2K_STBY, OUTPUT);
    
    Serial.println("GPIO pins initialized");
}