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

// To use send Email for Gmail to port 465 (SSL), less secure app option should be enabled. https://myaccount.google.com/lesssecureapps?pli=1

#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <ESP32Ping.h>
#include "myemail.h"
#include "decodeN2K.h"
#include "main.h"
#include "webserv.h"

/* The SMTP Session object used for Email sending */
SMTPSession smtp;

// Structure to store device settings
extern Settings settings;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

String WIFI_SSID[] = {WIFI_SSID0, WIFI_SSID1, WIFI_SSID2, WIFI_SSID3};

void sendEmail(void *pvParameters) {
    String chosenSSID = "";
    sendEmailData("Starting...");
    bool exit = false;

    if (settings.isLocalAP) {
        WiFi.mode(WIFI_MODE_APSTA);

        sendEmailData("Started scan of access points");
        int n = WiFi.scanNetworks();
        sendEmailData("Completed scan of access points");
        if (n == 0) {
            sendEmailData("No WiFi networks found. Failed to send email.");
            WiFi.mode(WIFI_MODE_AP);
            vTaskDelete(NULL);
        } else {
            for (int i = 0; i < n; ++i) {   // for each access point found
                if (exit) break;
                for (int j = 0; j < WIFI_SSID->length(); j++) {     // for each selectable access point
                    if (WiFi.SSID(i) == WIFI_SSID[j]) {
                        String s = "Chose ";
                        s += WiFi.SSID(i);
                        sendEmailData(s);
                        chosenSSID = WiFi.SSID(i);
                        exit = true;
                        break;
                    }
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
        String s = "Connecting to ";
        s += chosenSSID;
        s += " using ";
        s += WIFI_PASSWORD;
        sendEmailData(s);
        if (chosenSSID != emptyString) {
            WiFi.begin(chosenSSID.c_str(), WIFI_PASSWORD);
            unsigned long t = millis();
            while ((WiFi.status() != WL_CONNECTED) && ((t + 20000) > millis())) {
                vTaskDelay(200 / portTICK_PERIOD_MS);
            }
            if ((t + 20000) < millis() || WiFi.status() != WL_CONNECTED)
            {
                String f = "Failed to connect to access point: ";
                f += WiFi.status();
                sendEmailData(f);
                WiFi.mode(WIFI_MODE_AP);
                vTaskDelete(NULL);
            }
            sendEmailData("Connnected to access point");
        } else {
            vTaskDelete(NULL);
        }
    }

    sendEmailData("Testing for internet...");
    unsigned long t = millis();
    while (!Ping.ping("smtp.gmail.com") && (t + 20000) > millis()) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    if ((t + 20000) < millis())
    {
        sendEmailData("Not connected to internet");
        if (settings.isLocalAP) {WiFi.mode(WIFI_MODE_AP);}
        vTaskDelete(NULL);
    }
    sendEmailData("Connected to internet");

    /** Enable the debug via Serial port
     * none debug or 0
     * basic debug or 1
     */
    smtp.debug(0);

    /* Set the callback function to get the sending results */
    smtp.callback(smtpCallback);

    /* Declare the Session_Config for user defined session credentials */
    Session_Config config;

    /* Set the session config */
    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;
    config.login.user_domain = "mydomain.net";

    /*
    Set the NTP config time
    For times east of the Prime Meridian use 0-12
    For times west of the Prime Meridian add 12 to the offset.
    Ex. American/Denver GMT would be -6. 6 + 12 = 18
    See https://en.wikipedia.org/wiki/Time_zone for a list of the GMT/UTC timezone offsets
    */
    config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    config.time.gmt_offset = 19;
    config.time.day_light_offset = 0;

    // Serial.println("done config");

    /* Declare the message class */
    SMTP_Message message;

    /* Enable the chunked data transfer with pipelining for large message if server supported */
    message.enable.chunking = true;

    /* Set the message headers */
    message.sender.name = SENDER_NAME;
    message.sender.email = AUTHOR_EMAIL;

    time_t now;
    struct tm timeDetails;
    time(&now);
    localtime_r(&now, &timeDetails);
    String mysubject = asctime(&timeDetails);
    mysubject.concat(" Recordings");
    message.subject = mysubject;
    message.addRecipient(RECIPIENT_NAME, RECIPIENT_EMAIL);
    message.addRecipient(RECIPIENT_NAME2, RECIPIENT_EMAIL2);

    /** Two alternative content versions are sending in this example e.g. plain text and html */
    String htmlMsg = "New voyage recordings!";
    message.html.content = htmlMsg.c_str();
    message.html.charSet = "utf-8";
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_qp;

    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
    message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

    /* The attachment data item */
    SMTP_Attachment att;

    File root = SD.open("/");
    if (!root) {
        // Serial.println("Failed to open directory");
        // return;
    }
    if (!root.isDirectory()) {
        // Serial.println("Not a directory");
        // return;
    }
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {} 
        else if (file.name() == "wifi.txt") {}
        else {
            String filename = file.name();
            if (filename.substring(filename.length() - 3) == "csv") {
                att.descr.mime = "text/csv";
            } else if (filename.substring(filename.length() - 3) == "gpx") {
                att.descr.mime = "text/xml";
            } else {
                att.descr.mime = "text/plain";
            }

            att.descr.filename = filename;
            att.file.path = file.path();
            att.file.storage_type = esp_mail_file_storage_type_sd;
            att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;

            message.addAttachment(att);
            message.resetAttachItem(att);
        }
        file = root.openNextFile();
    }

    /* Connect to server with the session config */
    if (!smtp.connect(&config)) {
        // ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        sendEmailData("Connection error. Failed to send email.");
        sendEmailData(smtp.errorReason().c_str());
        if (settings.isLocalAP) {WiFi.mode(WIFI_MODE_AP);}
        vTaskDelete(NULL);
    }

    if (!smtp.isLoggedIn()) {
        // Serial.println("\nNot yet logged in.");
    } else {
        if (smtp.isAuthenticated()) {
            // Serial.println("\nSuccessfully logged in.");
        } else {
            // Serial.println("\nConnected with no Auth.");
        }
    }

    sendEmailData("Sending email...");

    /* Start sending the Email and close the session */
    if (!MailClient.sendMail(&smtp, &message, true)) {
        // Serial.print("Error sending Email, ");
        // Serial.println(smtp.errorReason());
        sendEmailData("Failed to send email.");
    }
    if (settings.isLocalAP) {
        WiFi.mode(WIFI_MODE_AP);
    }
    // Serial.println("Sent Email");
    sendEmailData("Email sent successfully!");
    vTaskDelete(NULL);
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {
    /* Print the current status */
    // Serial.println(status.info());

    /* Print the sending result */
    if (status.success()) {
        // Serial.println("----------------");
        // ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
        // ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
        // Serial.println("----------------\n");
        struct tm dt;

        for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
            /* Get the result item */
            SMTP_Result result = smtp.sendingResult.getItem(i);
            time_t ts = (time_t)result.timestamp;
            localtime_r(&ts, &dt);

            // ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
            // ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
            // ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
            // ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
            // ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
        }
        // Serial.println("----------------\n");

        // You need to clear sending result as the memory usage will grow up.
        smtp.sendingResult.clear();
    }
}