#include "nmeaBLE.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <map>
#include "sdcard.h"
#include "ConfigurationManager.h"
#include "recording.h"
#include "webserv.h"

// BLE UUIDs
#define SERVICE_UUID                "ddbf54c4-f88d-4358-b2a5-cfbf2ce4dd37"
#define NMEADATA_UUID               "67fa3483-a670-4f3e-8e1c-79a106c35567"
#define SETTINGS_UUID               "2e99e907-2587-43f9-8865-5a02f39a322a"
#define DOWNLOADS_UUID              "5a3446e2-cab6-4bbe-b4c4-d7be7284a4b5"
#define FILE_DOWNLOAD_CONTROL_UUID  "b946d82c-2878-472b-ae34-9d47f84e1a58"
#define FILE_DOWNLOAD_UUID          "2661cd56-cd1f-47f6-b404-6f5bde95793b"

NimBLECharacteristic *pNmeaCharacteristic;
NimBLECharacteristic *pSettingsCharacteristic;
NimBLECharacteristic *pDownloadsListCharacteristic;
NimBLECharacteristic *pFileDownloadControlCharacteristic;
NimBLECharacteristic *pFileDownloadCharacteristic;
NimBLEServer *pServer;

class DownloadsListCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        String value = pCharacteristic->getValue();
        if (value.equalsIgnoreCase("listDir")) {
            pDownloadsListCharacteristic->setValue(listDir(SD, "/", 0).c_str());
            pDownloadsListCharacteristic->notify();
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
                ConfigurationManager& config = ConfigurationManager::getInstance();
                int recInt = value.toInt();
                if (recInt < 1) { recInt = 1; }
                config.setRecInterval(recInt);
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "wifiSSID", [](String value) {
                ConfigurationManager& config = ConfigurationManager::getInstance();
                config.setWifiSSID(value);
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "wifiPass", [](String value) {
                ConfigurationManager& config = ConfigurationManager::getInstance();
                config.setWifiPass(value);
                pSettingsCharacteristic->setValue(makeSettingsJson().c_str());
                pSettingsCharacteristic->notify();
            }},
            { "wifiMode", [](String value) {
                ConfigurationManager& config = ConfigurationManager::getInstance();
                config.setLocalAP(value == "true");
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
                pDownloadsListCharacteristic->setValue(listDir(SD, "/", 0).c_str());
                pDownloadsListCharacteristic->notify();
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

static File bleFile;
static size_t bleFileSize = 0;
static size_t bleFilePos = 0;
static const size_t chunkSize = 180;

class FileDownloadControlCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        String value = pCharacteristic->getValue();
        if (value.length() > 0 && !value.equalsIgnoreCase("ack") && !value.equalsIgnoreCase("end")) {
            // File request
            if (bleFile) bleFile.close();
            String filePath = "/" + value;
            bleFile = SD.open(filePath, FILE_READ);
            if (!bleFile) {
                Serial.println("Failed to open file");
                pFileDownloadControlCharacteristic->setValue("error");
                pFileDownloadControlCharacteristic->notify();
                Serial.println("File Control Notified");
                return;
            }
            bleFileSize = bleFile.size();
            bleFilePos = 0;
            Serial.printf("Starting BLE file transfer: %s, size: %u B\n", filePath.c_str(), (unsigned int)bleFileSize);
            // Send the first chunk
            uint8_t buffer[chunkSize];
            size_t bytesRead = bleFile.readBytes((char*)buffer, chunkSize);
            bleFilePos += bytesRead;
            if (bytesRead > 0) {
                pFileDownloadCharacteristic->setValue(buffer, bytesRead);
                pFileDownloadCharacteristic->notify();
            } else {
                bleFile.close();
                Serial.println("File sent (empty or error).\n");
            }
        } else if (value.equalsIgnoreCase("ack")) {
            // Send next chunk
            if (bleFile && bleFile.available()) {
                uint8_t buffer[chunkSize];
                size_t bytesRead = bleFile.readBytes((char*)buffer, chunkSize);
                bleFilePos += bytesRead;
                if (bytesRead > 0) {
                    pFileDownloadCharacteristic->setValue(buffer, bytesRead);
                    pFileDownloadCharacteristic->notify();
                    // Serial.printf("Sent chunk of size: %zu, total sent: %zu/%u\n", bytesRead, bleFilePos, (unsigned int)bleFileSize);
                } else if (!bleFile.available() || bytesRead == 0) {
                    bleFile.close();
                    Serial.println("No more file chunks");
                }
            } else {
                if (bleFile) bleFile.close();
                Serial.println("File transfer complete");
            }
        } else if (value.equalsIgnoreCase("end")) {
            // End the file transfer
            pFileDownloadCharacteristic->setValue("");
        } else {
            Serial.println("No file name provided for download");
        }
    }
};

void bleSetup() {
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
    pDownloadsListCharacteristic = pService->createCharacteristic(
                        DOWNLOADS_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY |
                        NIMBLE_PROPERTY::WRITE
                        );
    pFileDownloadControlCharacteristic = pService->createCharacteristic(
                        FILE_DOWNLOAD_CONTROL_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY |
                        NIMBLE_PROPERTY::WRITE
                        );
    pFileDownloadCharacteristic = pService->createCharacteristic(
                        FILE_DOWNLOAD_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::NOTIFY
                        );
    pSettingsCharacteristic->setCallbacks(new SettingsCallback());
    pDownloadsListCharacteristic->setCallbacks(new DownloadsListCallback());
    pFileDownloadControlCharacteristic->setCallbacks(new FileDownloadControlCallback());
    // pFileDownloadCharacteristic does not need callbacks, it just sends data

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
