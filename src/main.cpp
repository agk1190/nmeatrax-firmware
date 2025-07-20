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

TaskHandle_t webTaskHandle = NULL;
TaskHandle_t loggingTaskHandle = NULL;
TaskHandle_t bgTaskHandle = NULL;
TaskHandle_t nmeaTaskHandle = NULL;

// ***************************************************
/**
 * @brief Main program setup function
*/
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        ESP.restart();
    }

    pinMode(LED_PWR, OUTPUT);
    pinMode(LED_N2K, OUTPUT);
    pinMode(LED_SD, OUTPUT);
    pinMode(SD_Detect, INPUT_PULLUP);
    pinMode(N2K_STBY, OUTPUT);

    digitalWrite(LED_PWR, HIGH);
    digitalWrite(LED_N2K, LOW);
    digitalWrite(LED_SD, LOW);
    digitalWrite(N2K_STBY, LOW);

    if (getSDcardStatus() == 1) {
        if (sdSetup()) {
            digitalWrite(LED_SD, HIGH);
            hostSdCard();
        } else {
            digitalWrite(LED_SD, LOW);
        }
    }

    readPreferences();
    delay(500);

    webSetup();
    NMEAsetup();

    xTaskCreate(vWebTask, "webTask", 4096, (void *) 1, 2, &webTaskHandle);
    delay(100);
    xTaskCreate(vBackgroundTasks, "bgTasks", 4096, (void *) 1, 3, &bgTaskHandle);
    delay(100);
    xTaskCreate(vNmeaTask, "nmeaTask", 8192, (void *) 1, 1, &nmeaTaskHandle);
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
    for (;;) {
        static int nmeaSleepCount = 0;

        switch (getSDcardStatus()) {
            case 0b00:
                digitalWrite(LED_SD, LOW);
                break;
            case 0b01:
                if (sdSetup()) {
                    digitalWrite(LED_SD, HIGH);
                    hostSdCard();
                } else {
                    digitalWrite(LED_SD, LOW);
                }
                break;
            case 0b10:
                digitalWrite(LED_SD, LOW);
                break; 
            case 0b11:
                digitalWrite(LED_SD, HIGH);
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