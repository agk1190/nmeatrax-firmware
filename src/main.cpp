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

#include <SPI.h>
#include "printf.h"
#include "RF24.h"

// instantiate an object for the nRF24L01 transceiver
RF24 radio(7, 8);  // using pin 7 for the CE pin, and pin 8 for the CSN pin

// Let these addresses be used for the pair
uint8_t address[][6] = { "1Node", "2Node" };
// It is very helpful to think of an address as a path instead of as
// an identifying device destination

// to use different addresses on a pair of radios, we need a variable to
// uniquely identify which address this radio will use to transmit
bool radioNumber = 1;  // 0 uses address[0] to transmit, 1 uses address[1] to transmit

// Used to control whether this node is sending or receiving
bool role = false;  // true = TX role, false = RX role

// For this example, we'll be using a payload containing
// a single float number that will be incremented
// on every successful transmission
float payload = 0.0;

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
bool backupSettings;

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
    return(writeFile(SPIFFS, "/settings.txt", settingsString.c_str(), false));
}

bool readSettings()
{
    String settingsString;
    bool usedSdCard = true;
    settingsString = getFile(SD, "/settings.txt");
    Serial.print("SD Card - ");
    Serial.println(settingsString);
    if (settingsString == "null" || settingsString == "") {
        settingsString = getFile(SPIFFS, "/settings.txt");
        Serial.print("SPIFFS - ");
        Serial.println(settingsString);
        usedSdCard = false;
    }
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
    if (usedSdCard) {
        if (!saveSettings()) {crash();}
        deleteFile(SD, "/settings.txt");
    };
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
    writeFile(SD, "/wifi.txt", wifiText.c_str(), false);
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

    if (getSDcardStatus()) {sdSetup();}

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

    if (!webSetup()) {crash();}
    // if (!NMEAsetup()) {crash();}
    createWifiText();

    // Create timers
    // TimerHandle_t nmeaTimer = xTimerCreate("NMEATimer", 1 / portTICK_PERIOD_MS, pdTRUE, NULL, nmeaTimerCallback);
    TimerHandle_t webTimer = xTimerCreate("webTimer", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, webTimerCallback);

    // Start the timer
    // xTimerStart(nmeaTimer, 0);
    xTimerStart(webTimer, 0);

    // initialize the transceiver on the SPI bus
    if (!radio.begin()) {
        Serial.println(F("radio hardware is not responding!!"));
        while (1) {}  // hold in infinite loop
    }

    // print example's introductory prompt
    Serial.println(F("RF24/examples/GettingStarted"));

    // To set the radioNumber via the Serial monitor on startup
    Serial.println(F("Which radio is this? Enter '0' or '1'. Defaults to '0'"));
    while (!Serial.available()) {
        // wait for user input
    }
    char input = Serial.parseInt();
    radioNumber = input == 1;
    Serial.print(F("radioNumber = "));
    Serial.println((int)radioNumber);

    // role variable is hardcoded to RX behavior, inform the user of this
    Serial.println(F("*** PRESS 'T' to begin transmitting to the other node"));

    // Set the PA Level low to try preventing power supply related problems
    // because these examples are likely run with nodes in close proximity to
    // each other.
    radio.setPALevel(RF24_PA_LOW);  // RF24_PA_MAX is default.

    // save on transmission time by setting the radio to only transmit the
    // number of bytes we need to transmit a float
    radio.setPayloadSize(sizeof(payload));  // float datatype occupies 4 bytes

    // set the TX address of the RX node into the TX pipe
    radio.openWritingPipe(address[radioNumber]);  // always uses pipe 0

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(1, address[!radioNumber]);  // using pipe 1

    // additional setup specific to the node's role
    if (role) {
        radio.stopListening();  // put radio in TX mode
    } else {
        radio.startListening();  // put radio in RX mode
    }

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
            } while (searchForFile(SD, lastCSVfileName.c_str()));

            CSVFileName = "/";
            CSVFileName += lastCSVfileName;       // current = last because search function failed on search for current file name
            GPXFileName = "/Voyage";
            GPXFileName += voyageNum;
            GPXFileName += ".gpx";
            createGPXfile(GPXFileName.c_str(), timeString.c_str());
            outOfIdle = false;
        }
        
        if (!appendFile(SD, CSVFileName.c_str(), getCSV().c_str(), false)) {crash();}
        if (!writeGPXpoint(GPXFileName.c_str(), gpxWPTcount, lat, lon)) {crash();}
        gpxWPTcount++;
        count = 0;
    
    }
    // backup settings to sd card when doing ota update
    if (backupSettings) {
        if (!writeFile(SD, "/settings.txt", getFile(SPIFFS, "/settings.txt").c_str(), false)){crash();}
        Serial.println("Backup Complete");
        backupSettings = false;
    }

    if (role) {
        // This device is a TX node

        unsigned long start_timer = micros();                // start the timer
        bool report = radio.write(&payload, sizeof(float));  // transmit & save the report
        unsigned long end_timer = micros();                  // end the timer

        if (report) {
        Serial.print(F("Transmission successful! "));  // payload was delivered
        Serial.print(F("Time to transmit = "));
        Serial.print(end_timer - start_timer);  // print the timer result
        Serial.print(F(" us. Sent: "));
        Serial.println(payload);  // print payload sent
        payload += 0.01;          // increment float payload
        } else {
        Serial.println(F("Transmission failed or timed out"));  // payload was not delivered
        }

        // to make this example readable in the serial monitor
        delay(1000);  // slow transmissions down by 1 second

    } else {
        // This device is a RX node

        uint8_t pipe;
        if (radio.available(&pipe)) {              // is there a payload? get the pipe number that recieved it
        uint8_t bytes = radio.getPayloadSize();  // get the size of the payload
        radio.read(&payload, bytes);             // fetch payload from FIFO
        Serial.print(F("Received "));
        Serial.print(bytes);  // print the size of the payload
        Serial.print(F(" bytes on pipe "));
        Serial.print(pipe);  // print the pipe number
        Serial.print(F(": "));
        Serial.println(payload);  // print the payload's value
        }
    }  // role

    if (Serial.available()) {
        // change the role via the serial monitor

        char c = toupper(Serial.read());
        if (c == 'T' && !role) {
        // Become the TX node

        role = true;
        Serial.println(F("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK"));
        radio.stopListening();

        } else if (c == 'R' && role) {
        // Become the RX node

        role = false;
        Serial.println(F("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK"));
        radio.startListening();
        }
    }

    vTaskDelay(1/portTICK_PERIOD_MS);
}