#include "nmeaBLE.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <map>
#include "sdcard.h"
#include "preferences.h"
#include "recording.h"
#include "webserv.h"

// BLE UUIDs
#define SERVICE_UUID        "ddbf54c4-f88d-4358-b2a5-cfbf2ce4dd37"
#define NMEADATA_UUID       "67fa3483-a670-4f3e-8e1c-79a106c35567"
#define SETTINGS_UUID       "2e99e907-2587-43f9-8865-5a02f39a322a"
#define DOWNLOADS_UUID      "5a3446e2-cab6-4bbe-b4c4-d7be7284a4b5"
#define FILE_DOWNLOAD_UUID  "2661cd56-cd1f-47f6-b404-6f5bde95793b"

NimBLECharacteristic *pNmeaCharacteristic;
NimBLECharacteristic *pSettingsCharacteristic;
NimBLECharacteristic *pDownloadsCharacteristic;
NimBLECharacteristic *pFileDownloadCharacteristic;
NimBLEServer *pServer;

// String settingsJson;

class DownloadsListCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        String value = pCharacteristic->getValue();
        if (value.equalsIgnoreCase("listDir")) {
            pDownloadsCharacteristic->setValue(listDir(SD, "/", 0).c_str());
            pDownloadsCharacteristic->notify();
        }    
    }

    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        Serial.println("Downloads list requested");
    }
};

class SettingsCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        String data = pCharacteristic->getValue();
        String key = data.substring(0, data.indexOf('='));
        String value = data.substring(data.indexOf('=') + 1);

        std::map<String, std::function<void(String)>> settingsMap = {
            { "fetch", [](String value) {
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "recMode", [](String value) {
                setRecordingMode(value.toInt());
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "recInt", [](String value) {
                settings.recInt = value.toInt();
                if (settings.recInt < 1) { settings.recInt = 1; }
                updatePreference("recInt", settings.recInt);
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "wifiSSID", [](String value) {
                settings.wifiSSID = value;
                updatePreference("wifiSSID", settings.wifiSSID.c_str());
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "wifiPass", [](String value) {
                settings.wifiPass = value;
                updatePreference("wifiPass", settings.wifiPass.c_str());
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "wifiMode", [](String value) {
                settings.isLocalAP = (value == "true");
                updatePreference("isLocalAP", settings.isLocalAP);
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "email", [](String value) {
                startEmailTask();
            }},
            { "otaUpdate", [](String value) {
                startOTAupdate();
            }},
            { "setWifiCred", [](String value) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, value);
                if (error) {
                    Serial.println("Failed to parse JSON (ble receive):");
                    Serial.println(error.c_str());
                } else {
                    addWifiPair(doc["ssid"], doc["password"]);
                }
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }}, 
            { "clrWifiCred", [](String value) {
                clearWifiCredentials();
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "eraseData", [](String value) {
                deleteFile(SD, "/");
                pDownloadsCharacteristic->setValue(listDir(SD, "/", 0).c_str());
                pDownloadsCharacteristic->notify();
            }},
            { "reboot", [](String value) {
                ESP.restart();
            }}
        };

        auto it = settingsMap.find(key);
        if (it != settingsMap.end()) {
            it->second(value); // Call the function associated with the setting
            // pCharacteristic->setValue("OK");
            // pCharacteristic->notify();
        } else {
            Serial.println("Unknown setting: " + key);
            pCharacteristic->setValue("Unknown setting");
            pCharacteristic->notify();
        }
    }

    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        Serial.println("Settings requested");
    }
};

class FileDownloadCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        String fileName = pCharacteristic->getValue();
        if (fileName.length() > 0) {
            Serial.println("File download requested: " + fileName);
            sendFileOverBLE(fileName.c_str());
        } else {
            Serial.println("No file name provided for download");
        }
    }
};

void bleSetup() {
    Serial.begin(115200);

    // Start BLE
    NimBLEDevice::init("NMEATrax");
    pServer = NimBLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pNmeaCharacteristic = pService->createCharacteristic(
                        NMEADATA_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY
                        );
    pSettingsCharacteristic = pService->createCharacteristic(
                        SETTINGS_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY |
                        NIMBLE_PROPERTY::WRITE
                        );
    pDownloadsCharacteristic = pService->createCharacteristic(
                        DOWNLOADS_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY |
                        NIMBLE_PROPERTY::WRITE
                        );
    pFileDownloadCharacteristic = pService->createCharacteristic(
                        FILE_DOWNLOAD_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY |
                        NIMBLE_PROPERTY::WRITE
                        );
    pSettingsCharacteristic->setCallbacks(new SettingsCallback());
    pDownloadsCharacteristic->setCallbacks(new DownloadsListCallback());
    pFileDownloadCharacteristic->setCallbacks(new FileDownloadCallback());

    pServer->advertiseOnDisconnect(true);
    
    
    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setName("NMEATrax");
    pAdvertising->start();

    Serial.println("BLE service started");
}

void sendBLEmessage(String &message) {
    pNmeaCharacteristic->setValue(message.c_str());
    pNmeaCharacteristic->notify();
}

void sendFileOverBLE(const char* path) {
    String filePath = "/" + String(path);
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file");
        return;
    }

    const size_t chunkSize = 180; // Must fit within BLE MTU (~185 for NimBLE)
    uint8_t buffer[chunkSize];

    while (file.available()) {
        size_t bytesRead = file.readBytes((char*)buffer, chunkSize);
        if (bytesRead > 0) {
            pFileDownloadCharacteristic->setValue(buffer, bytesRead);
            pFileDownloadCharacteristic->notify();
            //   delay(10);  // pacing helps avoid drops
            vTaskDelay(10 / portTICK_PERIOD_MS); // Yield to allow BLE stack to process
        }
    }

    pFileDownloadCharacteristic->setValue("done"); // Notify end of file transfer
    pFileDownloadCharacteristic->notify();

    file.close();
    Serial.println("File sent.");
}
