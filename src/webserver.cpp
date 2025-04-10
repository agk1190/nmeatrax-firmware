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
#include "SPIFFS.h"
#include "sdcard.h"
#include <ESPmDNS.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>

#include "main.h"
#include "webserv.h"
#include "myemail.h"
#include "preferences.h"

// WebSever object
AsyncWebServer server(80);

AsyncEventSource events("/NMEATrax");

// Structure to store device settings
extern Settings settings;

QueueHandle_t webQueue;
TaskHandle_t webSendTaskHandle = NULL;

// const char* getTZdefinition(double tz) {
//     if (tz == 0) {
//         return("GMT+0");
//     } else if (tz == +1) {
//         return("CET-1");
//     } else if (tz == +2) {
//         return("EET-2");
//     } else if (tz == +3) {
//         return("MSK-3");
//     } else if (tz == +4) {
//         return("SAMT-4");
//     } else if (tz == +5) {
//         return("PKT-5");
//     } else if (tz == +6) {
//         return("ALMT-6");
//     } else if (tz == +7) {
//         return("KRAT-7");
//     } else if (tz == +8) {
//         return("CST-8");
//     } else if (tz == +9) {
//         return("JST-9");
//     } else if (tz == +10) {
//         return("AEST-10");
//     } else if (tz == +11) {
//         return("AEDT-11");
//     } else if (tz == +12) {
//         return("FJT-12");
//     } else if (tz == +13) {
//         return("NZDT-13");
//     } else if (tz == +14) {
//         return("LINT-14");
//     }
//
//     else if (tz == -1) {
//         return("EGT+1");
//     } else if (tz == -2) {
//         return("GST+2");
//     } else if (tz == -3) {
//         return("WGT+3");
//     } else if (tz == -4) {
//         return("AST+4");
//     } else if (tz == -5) {
//         return("EST+5");
//     } else if (tz == -6) {
//         return("CST+6");
//     } else if (tz == -7) {
//         return("MST+7");
//     } else if (tz == -8) {
//         // return("PST+8PDT,M3.2.0/2,M11.1.0/2");
//         return("PST+8");
//     } else if (tz == -9) {
//         return("AKST+9");
//     } else if (tz == -10) {
//         return("HST+10");
//     } else if (tz == -11) {
//         return("NUT+11");
//     }else {
//         return("GMT+0");
//     }
// }

bool webSetup() {
    // serve content of root of web server directory
    server.serveStatic("/web", SPIFFS, "/");

    // serve content of sd card
    if (getSDcardStatus()) {server.serveStatic("/sdCard", SD, "/");}

    // redirect request to 192.168.1.1 to 192.168.1.1/web/index.html
    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { 
    //     request->redirect("/web/index.html"); 
    // });

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
        if (request->hasParam("wifiSSID")) {
            settings.wifiSSID = request->getParam("wifiSSID")->value().c_str();
            updatePreference("wifiSSID", settings.wifiSSID.c_str());
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("wifiPass")) {
            settings.wifiPass = request->getParam("wifiPass")->value().c_str();
            updatePreference("wifiPass", settings.wifiPass.c_str());
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("wifiMode")) {
            settings.isLocalAP = request->getParam("wifiMode")->value() == "true" ? true : false;
            updatePreference("isLocalAP", settings.isLocalAP);
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("recInt")) {
            settings.recInt = atoi(request->getParam("recInt")->value().c_str());
            if (settings.recInt < 1){settings.recInt = 1;}                
            updatePreference("recInt", settings.recInt);
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("setWifiCred")) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request->getParam("setWifiCred")->value().c_str());
            if (error) {
                Serial.println("Failed to parse JSON (http receive):");
                Serial.println(error.c_str());
                request->send(200, "text/plain", "Failed to parse JSON");
            }
            else {
                addWifiPair(doc["ssid"], doc["password"]);
                request->send(200, "text/plain", "OK");
            }
        }
        else if (request->hasParam("clrWifiCred")) {
            clearWifiCredentials();
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("eraseData")) {
            deleteFile(SD, "/");
            request->send(200, "text/plain", "OK");
        }
        else if (request->hasParam("reboot")) {
            request->send(200, "text/plain", "OK");
            ESP.restart();
        }
        else if (request->hasParam("email")) {
            request->send(200, "text/plain", "OK");
            xTaskCreate(sendEmail, "Send Email", 8192, NULL, 1, NULL);
        } 
        else if (request->hasParam("otaUpdate")) {
            request->send(200, "text/plain", "OK");
            ElegantOTA.begin(&server);  // Start ElegantOTA
            digitalWrite(N2K_STBY, HIGH);
        }
        else if (request->hasParam("recMode")) {
            int mode = atoi(request->getParam("recMode")->value().c_str());
            switch (mode) {
            case 0:
                settings.recMode = OFF;
                break;
            case 1:
                settings.recMode = ON;
                outOfIdle = true;
                break;
            case 2:
                settings.recMode = AUTO_SPD_IDLE;
                break;
            case 3:
                settings.recMode = AUTO_RPM_IDLE;
                break;
            default:
                settings.recMode = OFF;
                break;
            }

            updatePreference("recMode", settings.recMode);
            request->send(200, "text/plain", "OK");
        }
        else {
            request->send(200, "text/plain", "Nothing Set");
        }       
    });

    // send current settings to client
    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument values;
        char buffer[1024];
        values["firmware"] = FW_VERSION;
        values["hardware"] = "2.0";
        values["recMode"] = settings.recMode;
        values["recInt"] = settings.recInt;
        values["wifiMode"] = settings.isLocalAP;
        values["wifiSSID"] = settings.wifiSSID;
        values["wifiPass"] = settings.wifiPass;
        values["wifiCredentials"] = settings.wifiCredentials;
        values["buildDate"] = BUILD_DATE;
        serializeJson(values, buffer);
        request->send(200, "application/json", buffer);
    });

    webQueue = xQueueCreate(20, sizeof(String *)); // Queue for 20 messages
    xTaskCreatePinnedToCore(sendDataTask, "sendDataTask", 4096, NULL, 1, &webSendTaskHandle, 1);

    // Handle Web Server Events
    events.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()){
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        client->send("hello!", NULL, millis(), 10000);
    });
    server.addHandler(&events);

    // Start webserver
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
    std::string heartbeat = "{\"messageType\":\"000000\",\"instanceID\":0,\"data\":{\"millis\":" + std::to_string(millis()) + "}}";
    sendToWebQueue(heartbeat.c_str());
    ElegantOTA.loop();
}

void sendEmailData(String text) {
    std::string msg = "{\"messageType\":\"email\",\"instanceID\":0,\"data\":{\"msg\":\"" + std::string(text.c_str()) + "\"}}";
    sendToWebQueue(msg.c_str());
}

void sendDataTask(void *parameter) {
    String *dataToSend = nullptr;
    while (true) {
        if (xQueueReceive(webQueue, &dataToSend, portMAX_DELAY)) {
            if (dataToSend != nullptr) {
                events.send(dataToSend->c_str(), "nmeadata", millis());
                delete dataToSend; // Free allocated memory
            }
        }
    }
}

void sendToWebQueue(String data) {
    String *dataToSend = new String(data);
    if (!xQueueSend(webQueue, &dataToSend, 0)) {
        delete dataToSend; // Free memory if queue is full
    }
}