/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax main C++ file.
 */

#include "main.h"
#include "nmeaVars.h"
#include "decodeN2K.h"
#include "sdcard.h"
#include "webserv.h"
#include "preferences.h"
#include "recording.h"

// New modular managers
#include "CommunicationManager.h"
#include "ConfigurationManager.h"
#include "TaskManager.h"
#include "HardwareManager.h"

// Legacy task handles for backward compatibility
// TODO: Remove once all code migrated to TaskManager
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t loggingTaskHandle = NULL;
TaskHandle_t bgTaskHandle = NULL;
TaskHandle_t nmeaTaskHandle = NULL;

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

    // Initialize communication based on configuration
    CommunicationMode commMode = config.isLocalAP() ? 
        CommunicationMode::WIFI_ONLY : CommunicationMode::AUTO;
    
    if (!comm.initialize(commMode)) {
        Serial.println("Warning: Communication initialization failed, falling back to BLE");
        comm.initialize(CommunicationMode::BLE_ONLY);
    }

    // Initialize web server and NMEA
    webSetup();
    NMEAsetup();

    // Create tasks using TaskManager
    taskMgr.createTask(TaskType::WEB_TASK, vWebTask, (void *) 1);
    delay(100);
    taskMgr.createTask(TaskType::BACKGROUND_TASK, vBackgroundTasks, (void *) 1);
    delay(100);
    taskMgr.createTask(TaskType::NMEA_TASK, vNmeaTask, (void *) 1);
    
    // Update legacy task handles for backward compatibility
    webTaskHandle = taskMgr.getTaskHandle(TaskType::WEB_TASK);
    bgTaskHandle = taskMgr.getTaskHandle(TaskType::BACKGROUND_TASK);
    nmeaTaskHandle = taskMgr.getTaskHandle(TaskType::NMEA_TASK);
    
    Serial.println("NMEATrax initialization complete");
}

// ***************************************************
/**
 * @brief Main program loop function
*/
void loop() {}

void vNmeaTask(void * pvParameters) {
    TickType_t delay = 1 / portTICK_PERIOD_MS;
    for (;;) {
        NMEAloop();
        vTaskDelay(delay);
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
                vTaskResume(nmeaTaskHandle);
            } else {
                nmeaSleepCount++;
            }
        } else {
            nmeaSleepCount = 0;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}