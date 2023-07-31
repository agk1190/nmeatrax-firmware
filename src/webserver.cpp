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
#include <AsyncElegantOTA.h>
#include "myemail.h"

// WebSever object
AsyncWebServer server(80);

// WiFi manager object
WiFiManager wifiManager;

// Structure to store device settings
extern Settings settings;

// Create an Event Source on /NMEATrax
AsyncEventSource NMEATrax("/NMEATrax");

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

    // serve content of sd card
    if (getSDcardStatus()) {server.serveStatic("/sdCard", SD, "/");}   

    // redirect request to 192.168.1.1 to 192.168.1.1/index.html
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { 
        request->redirect("/index.html"); 
    });

    // Get all files on the SD card
    server.on("/listDir", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (getSDcardStatus()) {
            String fileList = listDir(SD, "/", 0);
            request->send(200, "text/plain", fileList);
        } else {
            request->send(200, "text/plain", "");
        }
    });

    // all functions related to doing or setting something
    server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        String _response = "<h1>OK</h1><script>setTimeout(function(){window.location.href = \"/options.html\"}, 2000);</script>";
        if (request->hasParam("AP_SSID")) {
            settings.wifiSSID = request->getParam("AP_SSID")->value().c_str();
            if (!saveSettings()) {crash();}
            // request->send(200, "text/plain", "OK");
            request->send(200, "text/html", _response);
            ESP.restart();
        }
        else if (request->hasParam("AP_PASS")) {
            settings.wifiPass = request->getParam("AP_PASS")->value().c_str();
            if (!saveSettings()) {crash();}
            request->send(200, "text/html", _response);
            ESP.restart();
        }
        else if (request->hasParam("recInt")) {
            settings.recInt = atoi(request->getParam("recInt")->value().c_str());
            if (settings.recInt < 1){settings.recInt = 1;}                
            if (!saveSettings()) {crash();}
            request->send(200, "text/html", _response);
        }
        else if (request->hasParam("timeZone")) {
            settings.timeZone = atol(request->getParam("timeZone")->value().c_str());
            if (!saveSettings()) {crash();}
            setenv("TZ", getTZdefinition(settings.timeZone), 1);     // set time zone
            tzset();
            request->send(200, "text/html", _response);
        }
        else if (request->hasParam("isMeters")) {
            settings.isMeters = !settings.isMeters;
            if (!saveSettings()) {crash();}
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("isDegF")) {
            settings.isDegF = !settings.isDegF;
            if (!saveSettings()) {crash();}
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("eraseWiFi")) {
            wifiManager.resetSettings();
            settings.isLocalAP = false;
            if (!saveSettings()) {crash();}
            request->send(200, "text/html", _response);
            ESP.restart();
        }
        else if (request->hasParam("eraseData")) {
            if (!deleteFile(SD, "/")) {crash();}
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("email")) {
            request->send(200, "text/plain", "OK");
            xTaskCreate(sendEmail, "Send Email", 8192, NULL, 1, NULL);
        } 
        else if (request->hasParam("otaUpdate")) {
            request->send(200, "text/plain", "OK");
            digitalWrite(N2K_STBY, HIGH);
            backupSettings = true;
        }
        else if (request->hasParam("recMode")) {
            int mode = atoi(request->getParam("recMode")->value().c_str());
            switch (mode) {
            case 0:
                recMode = OFF;
                if (getSDcardStatus()) endGPXfile(GPXFileName.c_str());
                break;
            case 1:
                recMode = ON;
                outOfIdle = true;
                break;
            case 2:
                recMode = AUTO_SPD_IDLE;
                break;
            case 3:
                recMode = AUTO_RPM_IDLE;
                break;
            default:
                recMode = OFF;
                if (getSDcardStatus()) endGPXfile(GPXFileName.c_str());
                break;
            }
            if (!saveSettings()) {crash();}
            request->send(200, "text/plain", "OK");
        }
        else {
            request->send(200, "text/plain", "Nothing Set");
        }       
    });

    // send current settings to client
    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        JSONVar values;
        values["isMeters"] = settings.isMeters;
        values["isDegF"] = settings.isDegF;
        values["recInt"] = settings.recInt;
        values["timeZone"] = settings.timeZone;
        values["recMode"] = recMode;
        request->send(200, "application/json", JSON.stringify(values));
    });

    server.addHandler(&NMEATrax);

    // Start ElegantOTA
    AsyncElegantOTA.begin(&server);

    // Start server
    server.begin();

    Serial.println("HTTP server started");

    // this advertises the device locally at "nmeatrax.local"
    // https://www.reddit.com/r/esp32/comments/sayiah/comment/htyvhf3/?utm_source=share&utm_medium=web2x&context=3
    #define HOSTNAME "nmeatrax"
    mdns_init(); 
    mdns_hostname_set(HOSTNAME); 
    mdns_instance_name_set(HOSTNAME); 
    MDNS.addService("http","tcp",80);
    MDNS.begin("NMEATrax");
    Serial.printf("MDNS responder started at http://%s.local\n", HOSTNAME);

    return(true);
}

void webLoop() {
    NMEATrax.send(JSONValues().c_str(), "nmeaData", millis());
}

void sendEmailData(String text) {
    NMEATrax.send(text.c_str(), "emailData", millis());
}