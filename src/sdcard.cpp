/**
 * NMEATrax
 * 
 * NMEATrax SD Card functions
*/

#include "FS.h"
#include "SPI.h"
#include "sdcard.h"

SPIClass spi = SPIClass(VSPI);

String addCRLF(const String& str) {
  String result = str;
  if (result.length() < 2 || result.substring(result.length() - 2) != "\r\n") {
    if (result.substring(result.length() - 1) == "\n") {
        result.replace("\n", "\r\n");
    } else {
        result.concat("\r\n");
    }
  }
  return result;
}

// created by ChatGPT Jan 28, 2023
bool searchForFile(fs::FS &fs, const char* fileName) {
    File root = fs.open("/");
    File file = root.openNextFile();
    while (file) {
        if (strcmp(file.name(), fileName) == 0) {
            Serial.print("Found file: ");
            Serial.println(file.name());
            file.close();
            return true;
        }
        file = root.openNextFile();
    }
    
    Serial.println("File not found");
    return false;
}

String listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    File root = fs.open(dirname);
    if (!root){
        Serial.println("Failed to open directory");
        return ("None");
    }
    if (!root.isDirectory()){
        Serial.println("Not a directory");
        return ("None");
    }
    String fileList;
    File file = root.openNextFile();
    while (file){
        if (file.isDirectory()){
            if (levels){listDir(fs, file.name(), levels - 1);}
        } else {
            fileList += file.name();
            fileList += ",";
        }
        file = root.openNextFile();
    }
    if (fileList == ""){
        return ("No Logs");
    } else {
        return (fileList);
    }
}

String getFile(fs::FS &fs, String filePath) {
    File file = fs.open(filePath);
    String s;
    if (!file){
        Serial.println("Failed to open file for reading");
        return("null");
    }
    if (file.available() && (file.size() < 100000)){
        s = (file.readString());
        file.close();
        return(s);
    } else {
        return("null");
    }
}

bool appendFile(fs::FS &fs, const char * path, const char * message, bool ensureCRLF){
    File file = fs.open(path, FILE_APPEND);
    const char * toSend;
    if (!file){
        Serial.println("Failed to open file for appending");
        return (false);
    }
    
    if (ensureCRLF)
    {
        toSend = addCRLF(message).c_str();
    } else {
        toSend = message;
    }
    
    if (file.print(toSend)){
    } else {
        Serial.println("Append failed");
        return (false);
    }
    file.close();
    return (true);
}

bool writeFile(fs::FS &fs, const char * path, const char * message, bool newLine){
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return(false);
    }
    if(file.print(message)){
        if (newLine){file.print("\r\n");}
    } else {
        Serial.println("Write failed");
        return(false);
    }
    file.close();
    return(true);
}

bool deleteFile(fs::FS &fs, const char * path){
    File root = fs.open(path);
    if(!root){
        Serial.println("Failed to open file/directory");
        return(false);
    }
    if(root.isDirectory()){
        File file = root.openNextFile();
        file = root.openNextFile();
        while (file) 
        {
            String fileName = "/";
            fileName += file.name();
            if (!fs.remove(fileName)) {
                return(false);
            }
            file = root.openNextFile();
        }
        return(true);
    } else {
        return(fs.remove(path));
    }
}

bool sdSetup(){
    const uint8_t SCK = 18;
    const uint8_t MISO = 19;
    const uint8_t MOSI = 23;
    const uint8_t CS = 5;

    spi.begin(SCK, MISO, MOSI, CS);

    if (!SD.begin(CS,spi,8000000)) {
        Serial.println("Card Mount Failed");
        return(false);
    }

    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return(false);
    }
    Serial.println("SD Card Initialized");
    return(true);
}