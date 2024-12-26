/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax main C++ file.
 * 
 * Resources
 * https://randomnerdtutorials.com/esp32-web-server-gauges/
 * https://cplusplus.com/reference/ctime/
 *
 */

// #include <tinyGpsPlus.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include "decodeN2K.h"
#include "sdcard.h"
#include "webserv.h"
#include "main.h"
#include <time.h>
#include "sdkconfig.h"
#include <FREERTOS/FreeRTOS.h>
#include <FREERTOS/timers.h>
#include <Preferences.h>

// Local NMEATrax access point IP settings
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Json variable to store data to send to the web server
JSONVar readings;

// Json variable to store data to save to SPIFFS
JSONVar jsettings;

Preferences preferences;

enum recMode recMode;

// Structure to store device settings
Settings settings;

bool outOfIdle = true;
String CSVFileName;

bool NMEAsleep = false;

String nmeaData[20];

TaskHandle_t webTaskHandle = NULL;
TaskHandle_t loggingTaskHandle = NULL;
TaskHandle_t bgTaskHandle = NULL;
TaskHandle_t nmeaTaskHandle = NULL;

/**
 * @brief Complie data in CSV format to send to the user.
 * @param none
 * @return A string of comma separated values containing the NMEA data.
*/
String getCSV() {
    // String data[] = {
    //     String(rpm),            // 0
    //     String(etemp, 2),          // 1
    //     String(otemp, 2),          // 2
    //     String(opres),          // 3
    //     String(fuel_rate, 1),   // 4
    //     String(flevel, 1),      // 5
    //     String(lpkm, 3),        // 6
    //     String(leg_tilt),       // 7
    //     String(speed),          // 8
    //     String(heading),        // 9
    //     String(depth, 2),          // 10
    //     String(wtemp, 2),          // 11
    //     String(battV),          // 12
    //     String(ehours),         // 13
    //     String(gear),           // 14
    //     String(lat, 6),         // 15
    //     String(lon, 6),         // 16
    //     String(mag_var, 2),     // 17
    //     String(unixTime)        // 18
    // };
    
    String rdata;

    //https://www.geeksforgeeks.org/how-to-find-size-of-array-in-cc-without-using-sizeof-operator/
    int dataSize = sizeof(nmeaData)/sizeof(nmeaData[0]);

    for (size_t i = 0; i < dataSize; i++) {
        rdata += nmeaData[i];
        if (i < dataSize - 1) {
            rdata += ",";
        }   
    }
    return(rdata);
}

bool saveSettings() {
    bool success = false;
    success = preferences.begin("settings");
    preferences.putBool("isLocalAP", settings.isLocalAP);
    preferences.putString("wifiSSID", settings.wifiSSID);
    preferences.putString("wifiPass", settings.wifiPass);
    preferences.putInt("recMode", settings.recMode);
    preferences.putInt("recInt", settings.recInt);
    preferences.end();
    return success;
}

bool readSettings() {
    bool success = false;
    success = preferences.begin("settings");
    settings.isLocalAP = preferences.getBool("isLocalAP", false);
    settings.wifiSSID = preferences.getString("wifiSSID", "NMEATrax").c_str();
    settings.wifiPass = preferences.getString("wifiPass", "nmeatrax").c_str();
    settings.recMode = preferences.getInt("recMode", 5);
    settings.recInt = preferences.getInt("recInt", 5);
    preferences.end();

    settings.wifiSSID = "NMEATrax";
    settings.wifiPass = "nmeatrax";

    switch (settings.recMode) {
        case 0:
            recMode = OFF;
            break;

        case 1:
            recMode = ON;
            break;

        case 2: 
        case 4:
            recMode = AUTO_SPD_IDLE;
            break;

        case 3: 
        case 5:
            recMode = AUTO_RPM_IDLE;
            break;
        
        default:
            recMode = AUTO_RPM_IDLE;
            break;
    }
    return success;
}

bool getSDcardStatus() {
    digitalWrite(LED_SD, digitalRead(SD_Detect));
    return(digitalRead(SD_Detect));
}

void createWifiText() {
    //https://forum.arduino.cc/t/how-to-manipulate-ipaddress-variables-convert-to-string/222693/6
    if (getSDcardStatus()) {
        IPAddress ipAddress = WiFi.localIP();
        String wifiText = String(ipAddress[0]) + String(".") +\
                        String(ipAddress[1]) + String(".") +\
                        String(ipAddress[2]) + String(".") +\
                        String(ipAddress[3]) + "\r\n"; 
        wifiText += WiFi.macAddress();
        writeFile(SD, "/wifi.txt", wifiText.c_str(), false);
    }
}

void crash() {
    Serial.println("Device Crashed!");
    ESP.restart();
}

// ***************************************************
/**
 * @brief Main program setup function
*/
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        crash();
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

    if (getSDcardStatus()) {sdSetup();}

    // load settings
    if (!readSettings()) {crash();}
    delay(500);

    if (settings.isLocalAP) {     // if in local AP mode, create AP
        WiFi.softAPsetHostname("nmeatrax");
        WiFi.mode(WIFI_MODE_AP);
        WiFi.softAPConfig(local_ip, gateway, subnet);
        WiFi.softAP(settings.wifiSSID, settings.wifiPass);
        delay(100);
        Serial.println("Started NMEA Access Point");
    }
    else {       // if the device should connect to an Access Point
        bool connected;
        wifiManager.setAPStaticIPConfig(local_ip, gateway, subnet);
        connected = wifiManager.autoConnect("NMEATrax");
        if (connected) {
            settings.isLocalAP = false;
            if (!saveSettings()) {crash();}
        }
        else {
            settings.isLocalAP = true;
            if (!saveSettings()) {crash();}
            ESP.restart();
        }
    }
    createWifiText();

    // https://forum.arduino.cc/t/esp32-settimeofday-functionality-giving-odd-results/676136
    // struct timeval tv;
    // tv.tv_sec = 1704096000;     // Jan 1 2024 00:00
    // settimeofday(&tv, NULL);    // set default time
    // setenv("TZ", getTZdefinition(settings.timeZone), 1);     // set time zone
    // tzset();

    for (int i = 0; i < sizeof(nmeaData) / sizeof(nmeaData[0]); i++) {
        nmeaData[i] = "-";
    }

    if (!webSetup()) {crash();}
    if (!NMEAsetup()) {crash();}

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

void vWriteRecording(void * pvParameters) {
    for (;;) {
        appendFile(SD, CSVFileName.c_str(), getCSV().c_str(), true);
        vTaskSuspend(loggingTaskHandle);
    } 
}

void vBackgroundTasks(void * pvParameters) {
    for (;;) {
        static int count = 0;
        static int localRecInt;
        static int nmeaSleepCount = 0;

        #ifdef TESTMODE1
        // time keeping
        time_t now;
        // struct tm timeDetails;
        time(&now);
        // localtime_r(&now, &timeDetails);
        // Serial.println(&timeDetails, "%A, %B %d %Y %H:%M:%S");

        rpm = random(650, 700);
        heading = random(15, 30);
        // wtemp = random(276, 286) + (random(1, 99)/100);
        wtemp = 280.48;
        otemp = random(376, 388);
        // etemp = random(343, 347);
        etemp = 348.65;
        depth = 5.26;
        mag_var = random(14, 16);
        leg_tilt = random(0, 15);
        opres = random(483, 626);
        battV = random(12, 15);
        battV += (random(1, 99)/100);
        fuel_rate = random(40, 44);
        speed = random(11, 13);
        speed += (random(1, 99)/100);
        ehours = 720000;
        flevel = random(40.2, 60.9);
        gear = "N";
        lat = random(40.0, 60.0);
        lon = random(120.0, 140.0);
        unixTime = now;
        // evcErrorMsg = getEngineStatus1(random(0, 65535)).c_str();
        #endif

        getSDcardStatus();
        count++;

        switch (recMode) {
            case AUTO_RPM:
                if (nmeaData[0].toInt() <= 0) {
                    recMode=AUTO_RPM_IDLE;
                    if (loggingTaskHandle != NULL) {vTaskDelete(loggingTaskHandle);}
                }
                localRecInt = nmeaData[0].toInt() > 3900 ? 1 : settings.recInt;
                break;

            case AUTO_RPM_IDLE:
                if (nmeaData[0].toInt() > 0) {
                    outOfIdle=true;
                    recMode=AUTO_RPM;
                }
                break;

            case AUTO_SPD:
                if (nmeaData[8].toDouble() <= 0) {
                    recMode=AUTO_SPD_IDLE;
                    if (loggingTaskHandle != NULL) {vTaskDelete(loggingTaskHandle);}
                }
                localRecInt = nmeaData[8].toDouble() > 15 ? 1 : settings.recInt;
                break;

            case AUTO_SPD_IDLE:
                if (nmeaData[8].toDouble() > 0) {
                    outOfIdle=true;
                    recMode=AUTO_SPD;
                }
                break;
            
            default:
                break;
        }
        
        if ((recMode == AUTO_RPM || recMode == AUTO_SPD || recMode == ON) && count >= localRecInt && getSDcardStatus()) {

            if (outOfIdle) {
                int voyageNum = 0;
                String lastCSVfileName;
                const char* csvHeaders = "RPM,Engine Temp (K),Oil Temp (K),Oil Pressure (kpa),Fuel Rate (L/h),Fuel Level (%),Fuel Efficiency (L/km),Leg Tilt (%),Speed (m/s),Heading (*),Depth (m),Water Temp (K),Battery Voltage (V),Engine Hours (s),Gear,Latitude,Longitude,Magnetic Variation (*),Time Stamp,Error Bits";
                do {
                    voyageNum++;
                    lastCSVfileName = "Voyage";
                    lastCSVfileName += voyageNum;
                    lastCSVfileName += ".csv";
                } while (searchForFile(SD, lastCSVfileName.c_str()));
                CSVFileName = "/";
                CSVFileName += lastCSVfileName;       // current = last because search function failed on search for current file name
                writeFile(SD, CSVFileName.c_str(), csvHeaders, true);
                xTaskCreate(vWriteRecording, "recordingTask", 4096, (void *) 1, 5, &loggingTaskHandle);
                outOfIdle = false;
            }
            vTaskResume(loggingTaskHandle);     // trigger log to be written
            count = 0;
        }
        if (NMEAsleep) {
            if (nmeaSleepCount >= 4) {  // 5 seconds
                nmeaSleepCount = 0;
                NMEAsleep = false;
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