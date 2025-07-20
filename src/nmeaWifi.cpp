#include "nmeaWifi.h"
#include "sdcard.h"
#include "preferences.h"

#include <ArduinoJSON.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Local NMEATrax access point IP settings
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

bool wifiSetup() {
    wifi_country_t wifiCountry = {
        cc: "CA",   // ISO country code
        schan: 1,   // Start channel
        nchan: 11,  // Total number of channels (Canada supports 11 channels)
        policy: WIFI_COUNTRY_POLICY_MANUAL
    };
    esp_wifi_set_country(&wifiCountry);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);

    if (settings.isLocalAP) {     // if in local AP mode, create AP
        WiFi.softAPsetHostname("nmeatrax");
        WiFi.mode(WIFI_MODE_AP);
        WiFi.softAPConfig(local_ip, gateway, subnet);
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        WiFi.softAP(settings.wifiSSID, settings.wifiPass);
        delay(100);
        Serial.println("Hosting Access Point");
    } else {       // if the device should connect to an Access Point
        bool connected;
        connected = connectToWiFi(settings.wifiCredentials);
        if (connected) {
            settings.isLocalAP = false;
            updatePreference("isLocalAP", false);
        } else {
            settings.isLocalAP = true;
            updatePreference("isLocalAP", true);
            ESP.restart();
        }
    }
    createWifiText();
    return true;
}

void createWifiText() {
    //https://forum.arduino.cc/t/how-to-manipulate-ipaddress-variables-convert-to-string/222693/6
    if (getSDcardStatus() == 3) {
        IPAddress ipAddress = WiFi.localIP();
        String wifiText = String(ipAddress[0]) + String(".") +\
                        String(ipAddress[1]) + String(".") +\
                        String(ipAddress[2]) + String(".") +\
                        String(ipAddress[3]) + "\r\n"; 
        wifiText += WiFi.macAddress();
        writeFile(SD, "/wifi.txt", wifiText.c_str(), false);
    }
}

bool connectToWiFi(String jsonString) {
    const size_t maxRetries = 3; // Number of times to loop through the list
    JsonDocument doc;

    // Attempt to connect using last stored Wi-Fi credentials
    Serial.println("Attempting to connect to the last known Wi-Fi...");
    WiFi.begin();

    // Wait for connection (timeout after 10 seconds)
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to the last known Wi-Fi!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        return true; // Exit the function if successfully connected
    } else {
        Serial.println("\nFailed to connect to the last known Wi-Fi. Trying the list...");
    }

    // Parse the JSON string
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.println("Failed to parse JSON:");
        Serial.println(error.c_str());
        return false;
    }

    // Ensure the JSON is an array
    if (!doc.is<JsonArray>()) {
        Serial.println("Invalid JSON format: Expected an array.");
        return false;
    }

    JsonArray wifiList = doc.as<JsonArray>();

    for (size_t attempt = 0; attempt < maxRetries; ++attempt) {
        for (JsonObject wifi : wifiList) {
            const char* ssid = wifi["ssid"];
            const char* password = wifi["password"];

            if (!ssid || !password) {
                Serial.println("Invalid JSON entry: Missing ssid or password.");
                continue;
            }

            Serial.printf("Attempting to connect to SSID: %s\n", ssid);
            WiFi.begin(ssid, password);

            // Wait for connection (timeout after 10 seconds)
            unsigned long startTime = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
                delay(500);
                Serial.print(".");
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\nConnected to %s\n", ssid);
                Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
                return true; // Exit the function upon successful connection
            } else {
                Serial.printf("\nFailed to connect to %s\n", ssid);
            }
        }

        Serial.printf("\nRetrying... (%d/%d)\n", attempt + 1, maxRetries);
    }

    Serial.println("Failed to connect to any WiFi network after multiple attempts.");
    return false;
}

String getMacAddress() {
    uint8_t baseMac[6];
    std::string macAddr;
    String macAddrStr;
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        macAddr = std::to_string(baseMac[3]) + std::to_string(baseMac[4]) + std::to_string(baseMac[5]);
        macAddrStr = macAddr.c_str();
    } else {
        macAddrStr = "707887";
    }
    return macAddrStr;
}