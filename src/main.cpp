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

#include <tinyGpsPlus.h>
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

// Local NMEATrax access point IP settings
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Json variable to store data to send to the web server
JSONVar readings;

// Json variable to store data to save to SPIFFS
JSONVar jsettings;

enum voyState voyState;

// Structure to store device settings
Settings settings;

bool outOfIdle = true;
String GPXFileName;

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
        String(fuel_rate),      // 4
        String(flevel),         // 5
        String(leg_tilt),       // 6
        String(speed),          // 7
        String(heading),        // 8
        String(depth),          // 9
        String(wtemp),          // 10
        String(battV),          // 11
        String(ehours),         // 12
        String(gear),           // 13
        String(lat, 6),         // 14
        String(lon, 6),         // 15
        String(mag_var, 2),       // 16
        String(settings.tempUnit),  // 17
        String(settings.depthUnit),  // 18
        String(timeString)      // 19
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
    readings["rpm"] = String(rpm);
    readings["etemp"] = String(etemp);
    readings["otemp"] = String(otemp);
    readings["opres"] = String(opres);
    readings["fuel_rate"] = String(fuel_rate);
    readings["flevel"] = String(flevel);
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
    readings["time"] = String(timeString);

    String jsonString = JSON.stringify(readings);
    return jsonString;
}

bool saveSettings()
{
    settings.voyState = voyState;
    jsettings["wifiMode"] = settings.wifiMode;
    // jsettings["wifiSSID"] = settings.wifiSSID;
    // jsettings["wifiPass"] = settings.wifiPass;
    jsettings["wifiSSID"] = "NMEATrax";
    jsettings["wifiPass"] = "nmeatrax";
    jsettings["voyState"] = settings.voyState;
    jsettings["recInt"] = settings.recInt;
    jsettings["depthUnit"] = settings.depthUnit;
    jsettings["tempUnit"] = settings.tempUnit;
    jsettings["timeZone"] = settings.timeZone;
    String settingsString = JSON.stringify(jsettings);
    File file = SPIFFS.open("/settings.txt", "w");
    if (!file){
        Serial.println("Error opening settings file");
        return(false);
    }
    file.print(settingsString);
    file.close();
    return(true);
}

bool readSettings()
{
    File file = SPIFFS.open("/settings.txt", "r");
    if (!file){return(false);}
    String settingsString = file.readString();
    file.close();
    jsettings = JSON.parse(settingsString);
    settings.wifiMode = jsettings["wifiMode"];
    settings.wifiSSID = jsettings["wifiSSID"];
    settings.wifiPass = jsettings["wifiPass"];
    settings.voyState = jsettings["voyState"];
    settings.recInt = jsettings["recInt"];
    settings.depthUnit = jsettings["depthUnit"];
    settings.tempUnit = jsettings["tempUnit"];
    settings.timeZone = jsettings["timeZone"];
    switch (settings.voyState)
    {
    case 0:
        voyState = OFF;
        break;

    case 1:
        voyState = ON;
        break;

    case 2:
        voyState = AUTO_SPD;
        break;

    case 3:
        voyState = AUTO_RPM;
        break;
    
    default:
        break;
    }
    return(true);
}

bool getSDcardStatus()
{
    digitalWrite(LED_SD, !digitalRead(SD_Detect));
    return(!digitalRead(SD_Detect));
}

void createWifiText()
{
    //https://forum.arduino.cc/t/how-to-manipulate-ipaddress-variables-convert-to-string/222693/6
    String wifiText;
    IPAddress ipAddress = WiFi.localIP();
    wifiText = String(ipAddress[0]) + String(".") +\
                String(ipAddress[1]) + String(".") +\
                String(ipAddress[2]) + String(".") +\
                String(ipAddress[3]) + "\r\n"; 
    wifiText += WiFi.macAddress();
    writeFile("/wifi.txt", wifiText.c_str(), false);
}

void crash() {
    Serial.println("Device Crashed!");
    ESP.restart();
}

// Timer callback function
void nmeaTimerCallback(TimerHandle_t xTimer) {
    // Run the nmeaLoop() function
    NMEAloop();
}

// Timer callback function
void webTimerCallback(TimerHandle_t xTimer) {
    // rpm = random(650, 700);
    webLoop();
}

// ***************************************************
/**
 * @brief Main program setup function
*/
void setup()
{
    Serial.begin(460800);
    delay(500);

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
    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        crash();
    }

    // load settings
    if (!readSettings()) {crash();}

    if (settings.wifiMode == "lan")     // if in local AP mode, create AP
    {
        WiFi.mode(WIFI_MODE_AP);
        WiFi.softAPConfig(local_ip, gateway, subnet);
        WiFi.softAP(settings.wifiSSID, settings.wifiPass);
        delay(100);
        Serial.println("Started NMEA Access Point");
    }
    else        // if the device should connect to an Access Point
    {
        bool connected;
        wifiManager.setAPStaticIPConfig(local_ip, gateway, subnet);
        connected = wifiManager.autoConnect("NMEATrax");
        if (connected)
        {
            settings.wifiMode = "wan";
            if (!saveSettings()) {crash();}
        }
        else
        {
            settings.wifiMode = "lan";
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

    if (getSDcardStatus()) {sdSetup();}
    if (!webSetup()) {crash();}
    if (!NMEAsetup()) {crash();}
    createWifiText();

    // Create timers
    TimerHandle_t nmeaTimer = xTimerCreate("NMEATimer", 1 / portTICK_PERIOD_MS, pdTRUE, NULL, nmeaTimerCallback);
    TimerHandle_t webTimer = xTimerCreate("webTimer", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, webTimerCallback);

    // Start the timer
    xTimerStart(nmeaTimer, 0);
    xTimerStart(webTimer, 0);
}

// ***************************************************
/**
 * @brief Main program loop function
*/
void loop()
{
    static long statDelay = millis();
    static int count = 0;
    static int localRecInt;

    if (statDelay + 1000 < millis())
    {
        // time keeping
        time_t now;
        struct tm timeDetails;
        time(&now);
        localtime_r(&now, &timeDetails);
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
        // fuel_rate = random(2, 6);
        // ehours = 122;
        // flevel = random(40.2, 60.9);
        // gear = "N";
        // lat = random(40.0, 60.0);
        // lon = random(120.0, 140.0);
        // timeString = asctime(&timeDetails);

        getSDcardStatus();
        statDelay = millis();
        count++;
    }

    switch (voyState){
        case AUTO_RPM:
            if (rpm==0){
                voyState=AUTO_RPM_IDLE;
                endGPXfile(GPXFileName.c_str());
            }
            break;

        case AUTO_RPM_IDLE:
            if (rpm>0){
                outOfIdle=true;
                voyState=AUTO_RPM;
            }
            break;

        case AUTO_SPD:
            if (speed==0){
                voyState=AUTO_SPD_IDLE;
                endGPXfile(GPXFileName.c_str());
            }
            break;

        case AUTO_SPD_IDLE:
            if (speed>0){
                outOfIdle=true;
                voyState=AUTO_SPD;
            }
            break;
        
        default:
            break;
    }

    if (voyState == AUTO_RPM){
        if (rpm > 3000){
            localRecInt = 30;
        } else {
            localRecInt = settings.recInt;
        }
    } else if (voyState == AUTO_SPD){
        if (speed > 15){
            localRecInt = 30;
        } else {
            localRecInt = settings.recInt;
        }
    } else {
        localRecInt = settings.recInt;
    }
    
    if ((voyState == AUTO_RPM || voyState == AUTO_SPD || voyState == ON) && count >= localRecInt){
        static String CSVFileName;
        static int gpxWPTcount = 1;

        if (outOfIdle) {
            int voyageNum = 0;
            gpxWPTcount = 1;
            String lastCSVfileName;

            do
            {
                voyageNum++;
                lastCSVfileName = "Voyage";
                lastCSVfileName += voyageNum;
                lastCSVfileName += ".csv";
            } while (searchForFile(lastCSVfileName.c_str()));

            CSVFileName = "/";
            CSVFileName += lastCSVfileName;       // current = last because search function failed on search for current file name
            GPXFileName = "/Voyage";
            GPXFileName += voyageNum;
            GPXFileName += ".gpx";
            createGPXfile(GPXFileName.c_str(), timeString.c_str());
            outOfIdle = false;
        }
        
        if (!appendFile(CSVFileName.c_str(), getCSV().c_str(), false)) {crash();}
        if (!writeGPXpoint(GPXFileName.c_str(), gpxWPTcount, lat, lon)) {crash();}
        gpxWPTcount++;
        count = 0;
    }
}