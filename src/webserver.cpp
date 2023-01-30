/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax webserver file.
 * 
 * Resources
 * https://randomnerdtutorials.com/esp32-web-server-gauges/
 * https://cplusplus.com/reference/ctime/
 *
 */

#include <WebServer.h>
#include "ESPAsyncWebServer.h"
#include "sdcard.h"
#include "main.h"
#include "SPIFFS.h"
#include "webserv.h"
#include <ESPmDNS.h>

// WebSever object
AsyncWebServer server(80);

// WiFi manager object
WiFiManager wifiManager;

// JSON variable to store data to send to the webpage
JSONVar values;

// Structure to store device settings
extern Settings settings;

// Create an Event Source on /events
AsyncEventSource events("/events");

const char* getTZdefinition(double tz) {
    if (tz == 0)
    {
        return("GMT+0");
    }
    else if (tz == +1) {
        return("CET-1");
    }
    else if (tz == +2) {
        return("EET-2");
    }
    else if (tz == +3) {
        return("MSK-3");
    }
    else if (tz == +4) {
        return("SAMT-4");
    }
    else if (tz == +5) {
        return("PKT-5");
    }
    else if (tz == +6) {
        return("ALMT-6");
    }
    else if (tz == +7) {
        return("KRAT-7");
    }
    else if (tz == +8) {
        return("CST-8");
    }
    else if (tz == +9) {
        return("JST-9");
    }
    else if (tz == +10) {
        return("AEST-10");
    }
    else if (tz == +11) {
        return("AEDT-11");
    }
    else if (tz == +12) {
        return("FJT-12");
    }
    else if (tz == +13) {
        return("NZDT-13");
    }
    else if (tz == +14) {
        return("LINT-14");
    }

    else if (tz == -1) {
        return("EGT+1");
    }
    else if (tz == -2) {
        return("GST+2");
    }
    else if (tz == -3) {
        return("WGT+3");
    }
    else if (tz == -4) {
        return("AST+4");
    }
    else if (tz == -5) {
        return("EST+5");
    }
    else if (tz == -6) {
        return("CST+6");
    }
    else if (tz == -7) {
        return("MST+7");
    }
    else if (tz == -8) {
        // return("PST+8PDT,M3.2.0/2,M11.1.0/2");
        return("PST+8");
    }
    else if (tz == -9) {
        return("AKST+9");
    }
    else if (tz == -10) {
        return("HST+10");
    }
    else if (tz == -11) {
        return("NUT+11");
    }
    else {
        return("GMT+0");
    }
}

bool webSetup() {
    // serve content of root of web server directory
    server.serveStatic("/", SPIFFS, "/");

    // redirect request to 192.168.1.1 to 192.168.1.1/index.html
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
        request->redirect("/index.html"); 
    });

    // Erase wifi callback
    server.on("/eraseWiFi", HTTP_GET, [](AsyncWebServerRequest *request){
        wifiManager.resetSettings();
        settings.wifiMode = "none";
        if (!saveSettings()) {crash();}
        ESP.restart(); 
    });

    // Erase data callback
    server.on("/eraseData", HTTP_GET, [](AsyncWebServerRequest *request){
        voyState = OFF;
        if (!deleteFile(SD, "/")) {
            Serial.println("Erase failed");
            crash();
        } else {
            Serial.println("Erase complete");
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/loggingMode", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("0")) {voyState = OFF;}
        else if (request->hasParam("1")) {voyState = ON; outOfIdle=true;}
        else if (request->hasParam("2")) {voyState = AUTO_SPD; outOfIdle=true;}
        else if (request->hasParam("3")) {voyState = AUTO_RPM; outOfIdle=true;}
        request->send(200, "text/plain", "OK");
        if (!saveSettings()) {crash();}
    });

    // Request for the recording mode
    server.on("/runState", HTTP_GET, [](AsyncWebServerRequest *request) {
        String s;
        if (voyState == OFF) {s="0";}
        else if (voyState == ON) {s="1";}
        else if (voyState == AUTO_SPD || voyState == AUTO_SPD_IDLE) {s="2";}
        else if (voyState == AUTO_RPM || voyState == AUTO_RPM_IDLE) {s="3";}
        request->send(200, "text/plain", s);
    });

    // Get all files on the SD card
    server.on("/listDir", HTTP_GET, [](AsyncWebServerRequest *request){
        String fileList = listDir(SD, "/", 0);
        request->send(200, "text/plain", fileList);
    });

    // download data callback
    server.on("/downloadData", HTTP_ANY, [](AsyncWebServerRequest *request){
        String filePath;
            if (request->hasParam("fileName")) {
                filePath = "/";
                filePath += request->getParam("fileName")->value();
            }
            else return;
        AsyncWebServerResponse *response = request->beginResponse(SD, filePath, getFile(filePath), true);
        String content = "attachment; filename=";
        content += request->getParam("fileName")->value();
        response->addHeader("Content-Disposition",content);
        request->send(response);
        Serial.println("completed download");
    });

    // Record the changed toggle and set the value
    server.on("/toggles", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String inputMessage;
        String inputParam;
        // GET input1 value on <ESP_IP>/update?state=<inputMessage>
        if (request->hasParam("0")) {settings.depthUnit = request->getParam("0")->value();}
        else if (request->hasParam("1")) {settings.tempUnit = request->getParam("1")->value();}
        if (!saveSettings()) {crash();}
        request->send(200, "text/plain", "OK");
    });

    // Send the current state of the toggle switches
    server.on("/toggleState", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String str = "";
        values["depthUnit"] = settings.depthUnit;
        values["tempUnit"] = settings.tempUnit;
        values["recInt"] = settings.recInt;
        values["timeZone"] = settings.timeZone;
        str = JSON.stringify(values);
        request->send(200, "application/json", str);
    });

    // source https://randomnerdtutorials.com/esp32-esp8266-input-data-html-form/
    // Handle the input from the text boxes on the options page
    server.on("/param", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String inputMessage;
        String inputParam;
        const char* PARAM_INPUT_1 = "ssid";
        const char* PARAM_INPUT_2 = "pass";
        const char* PARAM_INPUT_3 = "interval";
        const char* PARAM_INPUT_4 = "timeZone";
        // ssid
        if (request->hasParam(PARAM_INPUT_1)) {
            inputMessage = request->getParam(PARAM_INPUT_1)->value();
            inputParam = PARAM_INPUT_1;
            if (inputParam != "") {
                settings.wifiSSID = inputMessage.c_str();
                if (!saveSettings()) {crash();}
                ESP.restart();
            }
        }
        // password
        else if (request->hasParam(PARAM_INPUT_2)) {
            inputMessage = request->getParam(PARAM_INPUT_2)->value();
            inputParam = PARAM_INPUT_2;
            if (inputParam != "") {
                settings.wifiPass = inputMessage.c_str();
                if (!saveSettings()) {crash();}
                ESP.restart();
            }
        }
        // recording interval
        else if (request->hasParam(PARAM_INPUT_3)) {
            inputMessage = request->getParam(PARAM_INPUT_3)->value();
            inputParam = PARAM_INPUT_3;
            if (inputParam != "") {
                settings.recInt = atoi(inputMessage.c_str());
                if (settings.recInt < 1){settings.recInt = 1;}                
                if (!saveSettings()) {crash();}
            }
        }
        // timezone
        else if (request->hasParam(PARAM_INPUT_4)) {
            inputMessage = request->getParam(PARAM_INPUT_4)->value();
            inputParam = PARAM_INPUT_4;
            if (inputParam != "") {
                settings.timeZone = atol(inputMessage.c_str());
                if (!saveSettings()) {crash();}
                setenv("TZ", getTZdefinition(settings.timeZone), 1);     // set time zone
                tzset();
            }
        }
        else {
            inputMessage = "No message sent";
            inputParam = "none";
        }
        // The response is a webpage that displays what parameter was set to what value. It then redirects back to the options pages after 5 seconds.
        request->send(200, "text/html", "<h1>The " + inputParam + " was sucessfully set with value: " + inputMessage +
                                        "</h1><br><a href=\"/options.html\">Return</a><script>setTimeout(function()" + 
                                        "{window.location.href = \"/options.html\"}, 5000);</script>");
    });

    server.addHandler(&events);

    // Start server
    server.begin();

    Serial.println("HTTP server started");

    // this advertises the device locally at "nmeatrax.local"
    // https://www.reddit.com/r/esp32/comments/sayiah/comment/htyvhf3/?utm_source=share&utm_medium=web2x&context=3
    #define HOSTNAME "nmeatrax"
    mdns_init(); 
    mdns_hostname_set(HOSTNAME); 
    mdns_instance_name_set(HOSTNAME); 
    Serial.printf("MDNS responder started at http://%s.local\n", HOSTNAME);

    return(true);
}

void webLoop() {
    events.send(JSONValues().c_str(), "new_readings", millis());
}