/**
 * NMEATrax
 *
 * @authors Alex Klouda
 *
 * NMEATrax email sending file.
 *
 * Resources
 * https://randomnerdtutorials.com/esp32-send-email-smtp-server-arduino-ide/
 *
 */

#include "myemail.h"
#include <WiFiClientSecure.h>
#include <ESP32Ping.h>
#include "webserv.h"
#include "ConfigurationManager.h"
#include "sdcard.h"
#include "ArduinoJson.h"

#include <ReadyMail.h>

#include "TaskManager.h"
#include "CommunicationManager.h"

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

File myFile;
#define MY_FS SD

void smtpCb(SMTPStatus status) {
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

void fileCb(File &file, const char *filename, readymail_file_operating_mode mode) {
    switch (mode) {
    case readymail_file_mode_open_read:
        myFile = MY_FS.open(filename, FILE_OPEN_MODE_READ);
        break;
    case readymail_file_mode_open_write:
        myFile = MY_FS.open(filename, FILE_OPEN_MODE_WRITE);
        break;
    case readymail_file_mode_open_append:
        myFile = MY_FS.open(filename, FILE_OPEN_MODE_APPEND);
        break;
    case readymail_file_mode_remove:
        MY_FS.remove(filename);
        break;
    default:
        break;
    }

    // This is required by library to get the file object
    // that uses in its read/write processes.
    file = myFile;
}

time_t getCurrentTime() {
    configTime(-28800, 3600, "pool.ntp.org", "time.nist.gov");
    sendEmailData("Waiting for NTP time sync...");
    unsigned long start = millis();
    time_t now;
    while (((now = time(nullptr)) < 1700000000) && (millis() - start < 10000)) { // 1700000000 ~ year 2024
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    if (now < 1700000000) {
        sendEmailData("Failed. Using internal time.");
        now = time(nullptr);
        return now;
    }
    sendEmailData("NTP time synced.");
    return now;
}

void quitAndDelete() {
    ConfigurationManager& config = ConfigurationManager::getInstance();
    if (config.isLocalAP()) {WiFi.mode(WIFI_MODE_AP);}
    TaskManager& taskMgr = TaskManager::getInstance();
    sendEmailData("Exiting...");
    taskMgr.deleteTask(TaskType::EMAIL_TASK);
}

void sendEmail(void *pvParameters) {
    sendEmailData("Starting...");

    connectToWifi();

    sendEmailData("Testing for internet...");
    unsigned long t = millis();
    while (!Ping.ping("smtp.gmail.com") && (t + 20000) > millis()) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    if ((t + 20000) < millis()) {
        sendEmailData("Not connected to internet");
        quitAndDelete();
    }
    sendEmailData("Connected to internet");

    // Free Wi-Fi scan buffer and shrink TLS record buffers to maximise contiguous
    // heap available for BIGNUM / MPI operations during the TLS key-exchange phase.
    WiFi.scanDelete();
    vTaskDelay(10 / portTICK_PERIOD_MS);

    char heapMsg[96];
    snprintf(heapMsg, sizeof(heapMsg), "Heap before SSL: free=%u min=%u largest=%u",
             esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size(),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    sendEmailData(heapMsg);

    ssl_client.setInsecure();
    // ssl_client.setHandshakeTimeout(20);

    smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb);
    if (!smtp.isConnected()) {
        sendEmailData("Failed to connect to SMTP server");
        quitAndDelete();
    }
    sendEmailData("Connected to email server");

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated()) {
        sendEmailData("Failed to authenticate with SMTP server");
        quitAndDelete();
    }
    sendEmailData("Authenticated to email server");

    time_t now = getCurrentTime();

    SMTPMessage msg;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char datetimeStr[20];
    strftime(datetimeStr, sizeof(datetimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    String subject = "NMEATrax Recordings - " + String(datetimeStr);
    msg.headers.add(rfc822_subject, subject);
    msg.headers.add(rfc822_from, String(SENDER_NAME) + " <" + AUTHOR_EMAIL + ">");
    msg.headers.add(rfc822_to, String(RECIPIENT_NAME) + " <" + RECIPIENT_EMAIL + ">");
    #ifndef TESTMODE
    msg.headers.add(rfc822_to, String(RECIPIENT_NAME2) + " <" + RECIPIENT_EMAIL2 + ">");
    #endif
    msg.text.body("New voyage recordings!");
    msg.timestamp = now;

    File root = SD.open("/");
    if (!root) {
        sendEmailData("Failed to read SD card!");
        quitAndDelete();
    }
    if (!root.isDirectory()) {
        sendEmailData("Failed to read SD card!");
        quitAndDelete();
    }
    File file = root.openNextFile();
    sendEmailData("Adding attachments...");
    while (file) {
        if (file.isDirectory()) {} 
        else {
            String filename = file.name();
            if (filename.equalsIgnoreCase("wifi.txt")) {
                // Skip wifi.txt
            } else {
                Attachment attachment;
                if (filename.substring(filename.length() - 3).equalsIgnoreCase("csv")) {
                    attachment.mime = "text/csv";
                } else {
                    attachment.mime = "text/plain";
                }

                attachment.name = filename;
                attachment.filename = filename;
                attachment.attach_file.callback = fileCb;
                attachment.attach_file.path = file.path();
                msg.attachments.add(attachment, attach_type_attachment);
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        }
        file = root.openNextFile();
    }

    sendEmailData("Sending email...");
    String result;
    smtp.send(msg, result);
    sendEmailData("Email result: " + result);
    sendEmailData("Email sent successfully!");
    quitAndDelete();
}

void connectToWifi() {
    String chosenSSID = "";
    String chosenPassword = "";
    String textBuffer = "";
    JsonDocument doc;
    ConfigurationManager& config = ConfigurationManager::getInstance();
    
    DeserializationError error = deserializeJson(doc, config.getWifiCredentials());
    if (error) {
        sendEmailData("Failed to parse WiFi credentials JSON");
        quitAndDelete();
    }

    if (config.isLocalAP()) {
        WiFi.mode(WIFI_MODE_APSTA);

        sendEmailData("Starting scan of access points");
        int numberOfNetworksFound = WiFi.scanNetworks();
        sendEmailData("Completed scan of access points");

        if (numberOfNetworksFound == 0) {
            sendEmailData("No WiFi networks found. Unable to send email.");
            quitAndDelete();
        } else {
            bool _exit = false;
            JsonArray wifiArray = doc.as<JsonArray>();
            for (int i = 0; i < numberOfNetworksFound; ++i) { // for each access point found
                if (_exit) break;
                for (JsonObject cred : wifiArray) { // Iterate through parsed credentials doc.as<JsonArray>()
                    if (WiFi.SSID(i) == cred["ssid"].as<String>()) {
                        chosenSSID = WiFi.SSID(i);
                        chosenPassword = cred["password"].as<String>();
                        _exit = true;
                        break;
                    }
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }

        if (!chosenSSID.isEmpty()) {
            textBuffer = "Connecting to ";
            textBuffer.concat(chosenSSID);
            sendEmailData(textBuffer);
            WiFi.begin(chosenSSID.c_str(), chosenPassword.c_str());
            unsigned long t = millis();
            while ((WiFi.status() != WL_CONNECTED) && ((t + 20000) > millis())) {
                vTaskDelay(200 / portTICK_PERIOD_MS);
            }
            if ((t + 20000) < millis() || WiFi.status() != WL_CONNECTED) {
                textBuffer = "Failed to connect to access point: ";
                textBuffer.concat(String(WiFi.status()));
                sendEmailData(textBuffer);
                quitAndDelete();
            }
            sendEmailData("Connected to access point");
        } else {
            sendEmailData("Failed to find matching access point credentials");
            quitAndDelete();
        }
    }
}

void sendEmailData(String text) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"messageType\":\"email\",\"instanceID\":0,\"data\":{\"msg\":\"%s\"}}",
        text.c_str()
    );
    
    CommunicationManager& comm = CommunicationManager::getInstance();
    comm.sendData(buf);
}
