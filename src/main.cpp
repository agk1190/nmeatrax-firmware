/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax main C++ file.
 */

#include "main.h"
#include "decodeN2K.h"
#include "sdcard.h"
#include "webserv.h"
#include "preferences.h"
#include "recording.h"
#include "SPIFFS.h"

// New modular managers
#include "CommunicationManager.h"
#include "ConfigurationManager.h"
#include "TaskManager.h"
#include "HardwareManager.h"

// ***************************************************
/**
 * @brief Main program setup function - Now using modular managers
*/
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("NMEATrax Firmware Starting...");

    // Initialize SPIFFS first
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        ESP.restart();
    }

    // Initialize all managers
    HardwareManager& hardware = HardwareManager::getInstance();
    ConfigurationManager& config = ConfigurationManager::getInstance();
    TaskManager& taskMgr = TaskManager::getInstance();
    CommunicationManager& comm = CommunicationManager::getInstance();
    
    // Initialize hardware (replaces manual pin setup)
    if (!hardware.initialize()) {
        Serial.println("Failed to initialize hardware");
        ESP.restart();
    }

    // Initialize configuration (replaces readPreferences)
    if (!config.initialize()) {
        Serial.println("Failed to initialize configuration");
        // Continue with defaults
    }

    // Initialize SD card and set LED accordingly
    int sdStatus = hardware.getSDCardStatus();
    if (sdStatus == 1) {
        if (sdSetup()) {
            hardware.setLed(Led::SD, LedState::ON);
            hostSdCard();
        } else {
            hardware.setLed(Led::SD, LedState::OFF);
        }
    }

    CommunicationMode commMode = CommunicationMode::BLE_ONLY;
    
    if (!comm.initialize(commMode)) {
        Serial.println("Warning: Communication initialization failed, falling back to BLE");
        comm.initialize(CommunicationMode::BLE_ONLY);
    }

    // Initialize NMEA
    NMEAsetup();

    // Create tasks using TaskManager
    if (comm.isWifiEnabled()) {
        taskMgr.createTask(TaskType::WEB_TASK, vWebTask, nullptr);
        delay(100);
    }
    
    taskMgr.createTask(TaskType::BACKGROUND_TASK, vBackgroundTasks, nullptr);
    delay(100);
    taskMgr.createTask(TaskType::NMEA_TASK, vNmeaTask, nullptr);
    
    Serial.println("NMEATrax initialization complete");
}

// ***************************************************
/**
 * @brief Main program loop function
*/
void loop() {}

void vNmeaTask(void * pvParameters) {
    TickType_t taskDelay = 1 / portTICK_PERIOD_MS;
    for (;;) {
        NMEAloop();
        vTaskDelay(taskDelay);
    }
}

void vWebTask(void * pvParameters) {
    for (;;) {
        webLoop();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void vBackgroundTasks(void * pvParameters) {
    HardwareManager& hardware = HardwareManager::getInstance();
    TaskManager& taskMgr = TaskManager::getInstance();
    CommunicationManager& comm = CommunicationManager::getInstance();
    
    for (;;) {
        static int nmeaSleepCount = 0;

        int sdStatus = hardware.getSDCardStatus();
        switch (sdStatus) {
            case 0b00:
                hardware.setLed(Led::SD, LedState::OFF);
                break;
            case 0b01:
                if (sdSetup()) {
                    hardware.setLed(Led::SD, LedState::ON);
                    hostSdCard();
                } else {
                    hardware.setLed(Led::SD, LedState::OFF);
                }
                break;
            case 0b10:
                hardware.setLed(Led::SD, LedState::OFF);
                break; 
            case 0b11:
                hardware.setLed(Led::SD, LedState::ON);
                recorderLoop();
                break;
            default:
                break;
        }

        if (nmeaSleep) {
            if (nmeaSleepCount >= 4) {  // 5 seconds
                nmeaSleepCount = 0;
                nmeaSleep = false;
                // vTaskResume(nmeaTaskHandle);
                taskMgr.resumeTask(TaskType::NMEA_TASK);
            } else {
                nmeaSleepCount++;
            }
        } else {
            nmeaSleepCount = 0;
        }

        char text[160];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"000000\",\"instanceID\":0,\"data\":{\"millis\":%lu}}",
            millis()
        );
        comm.sendData(text);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}