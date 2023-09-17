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

Timings timings;

bool outOfIdle = true;
String GPXFileName;
bool backupSettings;

bool testBG = false;

/**
 * @brief Complie data in CSV format to send to the user.
 * @param none
 * @return A string of comma separated values containing the NMEA data.
*/
String getCSV()
{
    String data[] = {
        String(rpm),            // 0
        String(etemp),          // 1
        String(otemp),          // 2
        String(opres),          // 3
        String(fuel_rate, 1),   // 4
        String(flevel),         // 5
        String(lpkm, 3),        // 6
        String(leg_tilt),       // 7
        String(speed),          // 8
        String(heading),        // 9
        String(depth),          // 10
        String(wtemp),          // 11
        String(battV),          // 12
        String(ehours),         // 13
        String(gear),           // 14
        String(lat, 6),         // 15
        String(lon, 6),         // 16
        String(mag_var, 2),     // 17
        String(timeString)      // 18
    };
    String rdata;

    //https://www.geeksforgeeks.org/how-to-find-size-of-array-in-cc-without-using-sizeof-operator/
    int dataSize = sizeof(data)/sizeof(data[0]);

    for (size_t i = 0; i < dataSize; i++)
    {
        rdata += data[i];
        if (i < dataSize-1)
        {
            rdata += ",";
        }   
    }
    return(rdata);
}

String JSONValues()
{
    String _s = String(timings.webTook);
    _s.concat("/");
    _s.concat(timings.webRest);
    _s.concat(" ");
    _s.concat(timings.bgTook);
    _s.concat("/");
    _s.concat(timings.bgRest);
    _s.concat(" ");
    _s.concat(timings.nmeaTook);
    _s.concat("/");
    _s.concat(timings.nmeaRest);
    timeString.remove(timeString.length() - 1, 1);
    readings["rpm"] = String(rpm);
    readings["etemp"] = String(etemp);
    readings["otemp"] = String(otemp);
    readings["opres"] = String(opres);
    readings["fuel_rate"] = String(fuel_rate, 1);
    readings["flevel"] = String(flevel);
    readings["efficiency"] = String(lpkm, 3);
    readings["leg_tilt"] = String(leg_tilt);
    readings["speed"] = String(speed);
    readings["heading"] = String(heading);
    readings["depth"] = String(depth);
    readings["wtemp"] = String(wtemp);
    readings["battV"] = String(battV);
    readings["ehours"] = String(ehours);
    readings["gear"] = String(gear);
    readings["lat"] = String(lat, 6);
    readings["lon"] = String(lon, 6);
    readings["mag_var"] = String(mag_var, 2);
    readings["time"] = timeString;
    readings["timings"] = _s;

    String jsonString = JSON.stringify(readings);
    return jsonString;
}

bool saveSettings()
{
    bool success = false;
    success = preferences.begin("settings");
    preferences.putBool("isLocalAP", settings.isLocalAP);
    preferences.putString("wifiSSID", settings.wifiSSID);
    preferences.putString("wifiPass", settings.wifiPass);
    preferences.putInt("recMode", settings.recMode);
    preferences.putInt("recInt", settings.recInt);
    preferences.putBool("isMeters", settings.isMeters);
    preferences.putBool("isDegF", settings.isDegF);
    preferences.putInt("timeZone", settings.timeZone);
    preferences.end();
    return success;
}

bool readSettings()
{
    bool success = false;
    success = preferences.begin("settings");
    settings.isLocalAP = preferences.getBool("isLocalAP", false);
    settings.wifiSSID = preferences.getString("wifiSSID", "NMEATrax").c_str();
    settings.wifiPass = preferences.getString("wifiPass", "nmeatrax").c_str();
    settings.recMode = preferences.getInt("recMode", 5);
    settings.recInt = preferences.getInt("recInt", 5);
    settings.isMeters = preferences.getBool("isMeters", false);
    settings.isDegF = preferences.getBool("isDegF", false);
    settings.timeZone = preferences.getInt("timeZone", 0);
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

// Timer callback function
// void nmeaTimerCallback(TimerHandle_t xTimer) {
//     // Run the nmeaLoop() function
//     NMEAloop();
// }

// Timer callback function
void webTimerCallback(TimerHandle_t xTimer) {
    static int webRunTime;
    static int webRestTime = 0;
    timings.webRest = millis() - webRestTime;
    webRunTime = millis();
    webLoop();
    timings.webTook = (millis() - webRunTime);
    webRestTime = millis();
}

void bgTimerCallback(TimerHandle_t xTimer) {
    TaskHandle_t xHandle = NULL;
    static int bgRunTime;
    static int bgRestTime = 0;
    timings.bgRest = millis() - bgRestTime;
    bgRunTime = millis();
    if (!testBG) {
        testBG = true;
        xTaskCreate(vBackgroundTasks, "bgTasks", 4096, (void *) 1, 3, &xHandle);
    }
    timings.bgTook = millis() - bgRunTime;
    bgRestTime = millis();
}

// ***************************************************
/**
 * @brief Main program setup function
*/
void setup()
{
    Serial.begin(460800);
    delay(500);
    Serial.println();

    pinMode(LED_PWR, OUTPUT);
    pinMode(LED_N2K, OUTPUT);
    pinMode(LED_SD, OUTPUT);
    pinMode(SD_Detect, INPUT_PULLUP);
    pinMode(N2K_STBY, OUTPUT);

    digitalWrite(LED_PWR, HIGH);
    digitalWrite(LED_N2K, LOW);
    digitalWrite(LED_SD, LOW);
    digitalWrite(N2K_STBY, LOW);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        crash();
    }

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

    // https://forum.arduino.cc/t/esp32-settimeofday-functionality-giving-odd-results/676136
    struct timeval tv;
    tv.tv_sec = 1672560000;     // Jan 1 2023 00:00
    settimeofday(&tv, NULL);    // set default time
    setenv("TZ", getTZdefinition(settings.timeZone), 1);     // set time zone
    tzset();

    if (!webSetup()) {crash();}
    if (!NMEAsetup()) {crash();}
    createWifiText();

    // Create timers
    // TimerHandle_t nmeaTimer = xTimerCreate("NMEATimer", 1 / portTICK_PERIOD_MS, pdTRUE, NULL, nmeaTimerCallback);
    TimerHandle_t webTimer = xTimerCreate("webTimer", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, webTimerCallback);
    TimerHandle_t bgTimer = xTimerCreate("bgTimer", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, bgTimerCallback);

    // Start the timer
    // xTimerStart(nmeaTimer, 0);
    xTimerStart(webTimer, 0);
    xTimerStart(bgTimer, 0);
}

// ***************************************************
/**
 * @brief Main program loop function
*/
void loop()
{
    static int nmeaRunTime;
    static int nmeaRestTime = 0;
    timings.nmeaRest = millis() - nmeaRestTime;
    nmeaRunTime = millis();
    NMEAloop();
    timings.nmeaTook = millis() - nmeaRunTime;
    nmeaRestTime = millis();
    // Serial.println(millis() - nmeaRunTime);
}

void vBackgroundTasks(void * pvParameters)
{
    // static long statDelay = millis();
    static int count = 0;
    static int localRecInt;

    // time keeping
    // time_t now;
    // struct tm timeDetails;
    // time(&now);
    // localtime_r(&now, &timeDetails);
    // Serial.println(&timeDetails, "%A, %B %d %Y %H:%M:%S");

    // rpm = random(650, 700);
    // heading = random(15, 30);
    // wtemp = random(8, 12);
    // otemp = random(104, 108);
    // etemp = random(73, 76);
    // mag_var = random(14, 16);
    // leg_tilt = random(0, 25);
    // opres = random(483, 626);
    // battV = random(12.0, 15.0);
    // fuel_rate = random(40, 44);
    // speed = random(22, 24);
    // ehours = 122;
    // flevel = random(40.2, 60.9);
    // gear = "N";
    // lat = random(40.0, 60.0);
    // lon = random(120.0, 140.0);
    // timeString = asctime(&timeDetails);

    getSDcardStatus();
    // statDelay = millis();
    count++;

    switch (recMode) {
        case AUTO_RPM:
            if (rpm <= 0) {
                recMode=AUTO_RPM_IDLE;
                endGPXfile(GPXFileName.c_str());
            }
            break;

        case AUTO_RPM_IDLE:
            if (rpm>0) {
                outOfIdle=true;
                recMode=AUTO_RPM;
            }
            break;

        case AUTO_SPD:
            if (speed <= 0) {
                recMode=AUTO_SPD_IDLE;
                endGPXfile(GPXFileName.c_str());
            }
            break;

        case AUTO_SPD_IDLE:
            if (speed>0) {
                outOfIdle=true;
                recMode=AUTO_SPD;
            }
            break;
        
        default:
            break;
    }

    if (recMode == AUTO_RPM) {
        if (rpm > 3500) {
            localRecInt = 15;
        } else if (rpm > 4100) {
            localRecInt = 1;
        } else {
            localRecInt = settings.recInt;
        }
    } else if (recMode == AUTO_SPD) {
        if (speed > 15) {
            localRecInt = 15;
        } else {
            localRecInt = settings.recInt;
        }
    } else {
        localRecInt = settings.recInt;
    }
    
    if ((recMode == AUTO_RPM || recMode == AUTO_SPD || recMode == ON) && count >= localRecInt && getSDcardStatus()) {
        static String CSVFileName;
        static int gpxWPTcount = 1;

        if (outOfIdle) {
            int voyageNum = 0;
            gpxWPTcount = 1;
            String lastCSVfileName;
            const char* csvHeaders = "RPM,Engine Temp (C),Oil Temp (C),Oil Pressure (kpa),Fuel Rate (L/h),Fuel Level (%),Fuel Efficiency (L/km),Leg Tilt (%),Speed (kn),Heading (*),Depth (ft),Water Temp (C),Battery Voltage (V),Engine Hours (h),Gear,Latitude,Longitude,Magnetic Variation (*),Time Stamp";
            do {
                voyageNum++;
                lastCSVfileName = "Voyage";
                lastCSVfileName += voyageNum;
                lastCSVfileName += ".csv";
            } while (searchForFile(SD, lastCSVfileName.c_str()));
            CSVFileName = "/";
            CSVFileName += lastCSVfileName;       // current = last because search function failed on search for current file name
            GPXFileName = "/Voyage";
            GPXFileName += voyageNum;
            GPXFileName += ".gpx";
            createGPXfile(GPXFileName.c_str());
            writeFile(SD, CSVFileName.c_str(), csvHeaders, true);
            outOfIdle = false;
        }
        
        if (!appendFile(SD, CSVFileName.c_str(), getCSV().c_str(), true)) {crash();}
        if (lat != -273) {
            if (!writeGPXpoint(GPXFileName.c_str(), gpxWPTcount, lat, lon)) {crash();}
        }
        gpxWPTcount++;
        count = 0;
    }
    testBG = false;
    vTaskDelete(NULL);
}