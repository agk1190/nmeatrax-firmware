/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax webserver file.
 *
 */

#include "webserv.h"
#include <WebServer.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <ESPmDNS.h>
#include <ElegantOTA.h>

#include "nmeaVars.h"
#include "recording.h"
#include "myemail.h"
#include "preferences.h"
#include "nmeaWifi.h"
#include "sdcard.h"
#include "nmeaBLE.h"

// New modular managers
#include "CommunicationManager.h"
#include "ConfigurationManager.h"
#include "TaskManager.h"
#include "HardwareManager.h"

// WebSever object
AsyncWebServer server(80);

AsyncEventSource events("/NMEATrax");

QueueHandle_t webQueue;
TaskHandle_t webSendTaskHandle = NULL;

Settings settings;

bool useWifi = false;

bool webSetup() {
    if (useWifi) {
        wifiSetup();

        // serve content of root of web server directory
        server.serveStatic("/web", SPIFFS, "/");

        // redirect request to 192.168.1.1 to 192.168.1.1/web/index.html
        // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { 
        //     request->redirect("/web/index.html"); 
        // });

        // Get all files on the SD card
        server.on("/listDir", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (getSDcardStatus() == 3) {
                String fileList = listDir(SD, "/", 0);
                request->send(200, "text/plain", fileList);
            } else {
                request->send(503);
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
                startEmailTask();
            } 
            else if (request->hasParam("otaUpdate")) {
                request->send(200, "text/plain", "OK");
                startOTAupdate();
            }
            else if (request->hasParam("recMode")) {
                // int mode = atoi(request->getParam("recMode")->value().c_str());
                // switch (mode) {
                // case 0:
                //     settings.recMode = OFF;
                //     break;
                // case 1:
                //     settings.recMode = ON;
                //     outOfIdle = true;
                //     break;
                // case 2:
                //     settings.recMode = AUTO_SPD_IDLE;
                //     break;
                // case 3:
                //     settings.recMode = AUTO_RPM_IDLE;
                //     break;
                // default:
                //     settings.recMode = OFF;
                //     break;
                // }
                //
                // updatePreference("recMode", settings.recMode);
                setRecordingMode(request->getParam("recMode")->value().toInt());
                request->send(200, "text/plain", "OK");
            }
            else {
                request->send(200, "text/plain", "Nothing Set");
            }       
        });

        // Communication mode switching endpoint
        server.on("/comm", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (request->hasParam("mode")) {
                String modeStr = request->getParam("mode")->value();
                CommunicationManager& comm = CommunicationManager::getInstance();
                ConfigurationManager& config = ConfigurationManager::getInstance();
                
                CommunicationMode newMode;
                if (modeStr.equalsIgnoreCase("wifi")) {
                    newMode = CommunicationMode::WIFI_ONLY;
                } else if (modeStr.equalsIgnoreCase("ble")) {
                    newMode = CommunicationMode::BLE_ONLY;
                } else if (modeStr.equalsIgnoreCase("auto")) {
                    newMode = CommunicationMode::AUTO;
                } else {
                    request->send(400, "text/plain", "Invalid mode. Use: wifi, ble, or auto");
                    return;
                }
                
                bool success = comm.switchMode(newMode);
                if (success) {
                    String response = "Communication mode switched to " + modeStr;
                    request->send(200, "text/plain", response);
                } else {
                    request->send(500, "text/plain", "Failed to switch communication mode");
                }
            }
            else if (request->hasParam("status")) {
                CommunicationManager& comm = CommunicationManager::getInstance();
                JsonDocument status;
                
                status["currentMode"] = (int)comm.getCurrentMode();
                status["wifiEnabled"] = comm.isWifiEnabled();
                status["bleEnabled"] = comm.isBleEnabled();
                
                String response;
                serializeJson(status, response);
                request->send(200, "application/json", response);
            }
            else {
                request->send(400, "text/plain", "Missing parameter. Use 'mode' or 'status'");
            }
        });

        // send current settings to client
        server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", makeSettingsJson());
        });

        // Handle Web Server Events
        events.onConnect([](AsyncEventSourceClient *client){
            if(client->lastId()){
                Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
            }
            client->send("Connected to NMEATrax!", NULL, millis(), 10000);
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
    } else {
        // Use new CommunicationManager instead of direct calls
        CommunicationManager& comm = CommunicationManager::getInstance();
        ConfigurationManager& config = ConfigurationManager::getInstance();
        
        // Initialize communication based on configuration
        CommunicationMode mode = config.isLocalAP() ? 
            CommunicationMode::WIFI_ONLY : CommunicationMode::AUTO;
        comm.initialize(mode);
    }

    webQueue = xQueueCreate(20, sizeof(String *)); // Queue for 20 messages
    
    // Use TaskManager for web send task
    TaskManager& taskMgr = TaskManager::getInstance();
    taskMgr.createTask(TaskType::WEB_SEND_TASK, sendDataTask, NULL);
    webSendTaskHandle = taskMgr.getTaskHandle(TaskType::WEB_SEND_TASK);

    return(true);
}

String makeSettingsJson() {
    ConfigurationManager& config = ConfigurationManager::getInstance();
    CommunicationManager& comm = CommunicationManager::getInstance();
    
    JsonDocument values;
    char buffer[1024];
    values["firmware"] = FW_VERSION;
    values["hardware"] = "2.0";
    values["recMode"] = config.getRecMode();
    values["recInt"] = config.getRecInterval();
    values["wifiMode"] = config.isLocalAP();
    values["wifiSSID"] = config.getWifiSSID();
    values["wifiPass"] = config.getWifiPass();
    values["wifiCredentials"] = config.getWifiCredentials();
    values["buildDate"] = BUILD_DATE;
    
    // Add communication status
    values["commMode"] = (int)comm.getCurrentMode();
    values["wifiEnabled"] = comm.isWifiEnabled();
    values["bleEnabled"] = comm.isBleEnabled();
    
    serializeJson(values, buffer);
    String settingsStr(buffer);
    return buffer;
}

void startEmailTask() {
    xTaskCreate(sendEmail, "Send Email", 8192, NULL, 1, NULL);
}

void startOTAupdate() {
    HardwareManager& hardware = HardwareManager::getInstance();
    TaskManager& taskMgr = TaskManager::getInstance();
    
    hardware.setN2KStandby(true);
    taskMgr.deleteTask(TaskType::NMEA_TASK);
    taskMgr.deleteTask(TaskType::BACKGROUND_TASK);
    taskMgr.deleteTask(TaskType::WEB_SEND_TASK);
    ElegantOTA.begin(&server);  // Start ElegantOTA
}

void hostSdCard() {
    // serve content of sd card
    server.serveStatic("/sdCard", SD, "/");
}

void sendToWebQueue(String data) {
    CommunicationManager& comm = CommunicationManager::getInstance();
    comm.sendData(data);
    
    String *dataToSend = new String(data);
    if (!xQueueSend(webQueue, &dataToSend, 0)) {
        delete dataToSend; // Free memory if queue is full
    }
}

void webLoop() {
    char text[160];
    snprintf(text, sizeof(text),
        "{\"messageType\":\"000000\",\"instanceID\":0,\"data\":{\"millis\":%lu}}",
        millis()
    );
    sendToWebQueue(text);
    ElegantOTA.loop();

    // settingsJson = makeSettingsJson();
    // sendSettings(settingsStr); // Send settings over BLE
}

void sendEmailData(String text) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"messageType\":\"email\",\"instanceID\":0,\"data\":{\"msg\":\"%s\"}}",
        text.c_str()
    );
    sendToWebQueue(buf);
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