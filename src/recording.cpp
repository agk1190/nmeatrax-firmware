
#include "recording.h"
#include "ConfigurationManager.h"
#include "TaskManager.h"
#include "nmeaVars.h"
#include "sdcard.h"

bool outOfIdle = true;
String CSVFileName;

NMEAData blankData = {
    .rpm = -273,
    .eTemp = -273.0,
    .oTemp = -273.0,
    .oPres = -273.0,
    .fuelRate = -273.0,
    .fLevel = -273.0,
    .fEfficiency = -273.0,
    .legTilt = -273,
    .speed = -273.0,
    .heading = -273.0,
    .depth = -273.0,
    .wTemp = -273.0,
    .battV = -273.0,
    .eHours = -273,
    .gear = 'N',
    .lat = -273.0,
    .lon = -273.0,
    .magVar = -273.0,
    .unixTime = 0,
    .errorBits = 0
};
NMEAData *nmeaData = &blankData;

String getCSV() {
/*     String rdata;

    rdata += String(nmeaData->rpm) + ",";
    rdata += String(nmeaData->eTemp) + ",";
    rdata += String(nmeaData->oTemp) + ",";
    rdata += String(nmeaData->oPres) + ",";
    rdata += String(nmeaData->fuelRate) + ",";
    rdata += String(nmeaData->fLevel) + ",";
    rdata += String(nmeaData->fEfficiency) + ",";
    rdata += String(nmeaData->legTilt) + ",";
    rdata += String(nmeaData->speed) + ",";
    rdata += String(nmeaData->heading) + ",";
    rdata += String(nmeaData->depth) + ",";
    rdata += String(nmeaData->wTemp) + ",";
    rdata += String(nmeaData->battV) + ",";
    rdata += String(nmeaData->eHours) + ",";
    rdata += String(nmeaData->gear) + ",";
    rdata += String(nmeaData->lat, 6) + ",";
    rdata += String(nmeaData->lon, 6) + ",";
    rdata += String(nmeaData->magVar) + ",";
    rdata += String(nmeaData->unixTime) + ",";
    rdata += String(nmeaData->errorBits);

    return rdata; */
    char buffer[1024]; // Adjust size as needed
    snprintf(buffer, sizeof(buffer),
        "%d,%.2f,%.2f,%.2f,%.1f,%.1f,%.3f,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%c,%.6f,%.6f,%.2f,%" PRIu32 ",%lu",
        nmeaData->rpm,
        nmeaData->eTemp,
        nmeaData->oTemp,
        nmeaData->oPres,
        nmeaData->fuelRate,
        nmeaData->fLevel,
        nmeaData->fEfficiency,
        nmeaData->legTilt,
        nmeaData->speed,
        nmeaData->heading,
        nmeaData->depth,
        nmeaData->wTemp,
        nmeaData->battV,
        nmeaData->eHours,
        nmeaData->gear,
        nmeaData->lat,
        nmeaData->lon,
        nmeaData->magVar,
        nmeaData->errorBits,
        nmeaData->unixTime
    );
    return String(buffer);
}

void recorderLoop() {
    static int count = 0;
    ConfigurationManager& config = ConfigurationManager::getInstance();
    int localRecInt = config.getRecInterval();

    #ifdef TESTMODE1
    // time keeping
    time_t now;
    // struct tm timeDetails;
    time(&now);
    // localtime_r(&now, &timeDetails);
    // Serial.println(&timeDetails, "%A, %B %d %Y %H:%M:%S");

    rpm = random(650, 700);
    heading = random(15, 30);
    // wtemp = random(276, 286) + (random(1, 99)/100);
    wtemp = 280.48;
    otemp = random(376, 388);
    // etemp = random(343, 347);
    etemp = 348.65;
    depth = 5.26;
    mag_var = random(14, 16);
    leg_tilt = random(0, 15);
    opres = random(483, 626);
    battV = random(12, 15);
    battV += (random(1, 99)/100);
    fuel_rate = random(40, 44);
    speed = random(11, 13);
    speed += (random(1, 99)/100);
    ehours = 720000;
    flevel = random(40.2, 60.9);
    gear = "N";
    lat = random(40.0, 60.0);
    lon = random(120.0, 140.0);
    unixTime = now;
    // evcErrorMsg = getEngineStatus1(random(0, 65535)).c_str();
    #endif

    count++;
    TaskManager& taskMgr = TaskManager::getInstance();
    RecMode currentRecMode = config.getRecMode();

    switch (currentRecMode) {
        case AUTO_RPM:
            if (nmeaData->rpm <= 0) {
                config.setRecMode(AUTO_RPM_IDLE);
                taskMgr.deleteTask(TaskType::LOGGING_TASK);
            }
            localRecInt = nmeaData->rpm > 3900 ? 1 : config.getRecInterval();
            break;
        case AUTO_RPM_IDLE:
            if (nmeaData->rpm > 0) {
                outOfIdle=true;
                config.setRecMode(AUTO_RPM);
            }
            break;
        case AUTO_SPD:
            if (nmeaData->speed <= 0) {
                config.setRecMode(AUTO_SPD_IDLE);
                taskMgr.deleteTask(TaskType::LOGGING_TASK);
            }
            localRecInt = nmeaData->speed > 15 ? 1 : config.getRecInterval();
            break;
        case AUTO_SPD_IDLE:
            if (nmeaData->speed > 0) {
                outOfIdle=true;
                config.setRecMode(AUTO_SPD);
            }
            break;
        default:
            break;
    }
    
    currentRecMode = config.getRecMode(); // Get updated mode
    if ((currentRecMode == AUTO_RPM || currentRecMode == AUTO_SPD || currentRecMode == ON) && count >= localRecInt) {
        if (outOfIdle) {
            int voyageNum = 0;
            String lastCSVfileName;
            const char* csvHeaders = "RPM,Engine Temp (K),Oil Temp (K),Oil Pressure (kpa),Fuel Rate (L/h),Fuel Level (%),Fuel Efficiency (L/km),Leg Tilt (%),Speed (m/s),Heading (*),Depth (m),Water Temp (K),Battery Voltage (V),Engine Hours (h),Gear,Latitude,Longitude,Magnetic Variation (*),Error Bits,Time Stamp";
            do {
                voyageNum++;
                lastCSVfileName = "Voyage";
                lastCSVfileName += voyageNum;
                lastCSVfileName += ".csv";
            } while (searchForFile(SD, lastCSVfileName.c_str()));
            CSVFileName = "/";
            CSVFileName += lastCSVfileName;       // current = last because search function failed on search for current file name
            writeFile(SD, CSVFileName.c_str(), csvHeaders, true);
            taskMgr.createTask(TaskType::LOGGING_TASK, vWriteRecording, (void*)1);
            outOfIdle = false;
        }
        taskMgr.resumeTask(TaskType::LOGGING_TASK);     // trigger log to be written
        count = 0;
    }
}

bool writeRecording() {
    TaskManager& taskMgr = TaskManager::getInstance();
    if (!taskMgr.isTaskRunning(TaskType::LOGGING_TASK)) {
        Serial.println("Logging task not created");
        return false;
    }

    String csvData = getCSV();
    if (csvData.isEmpty()) {
        Serial.println("No data to write");
        return false;
    }

    if (!appendFile(SD, CSVFileName.c_str(), csvData.c_str(), true)) {
        Serial.println("Failed to write to file");
        return false;
    }

    // Serial.println("Data written successfully");
    return true;
}

void vWriteRecording(void * pvParameters) {
    TaskManager& taskMgr = TaskManager::getInstance();
    for (;;) {
        // appendFile(SD, CSVFileName.c_str(), getCSV().c_str(), true);
        writeRecording();
        taskMgr.suspendTask(TaskType::LOGGING_TASK);
    } 
}

void setRecordingMode(int mode) {
    if (mode < 0 || mode > 5) {
        Serial.println("Invalid recording mode");
        return;
    }
    ConfigurationManager& config = ConfigurationManager::getInstance();
    config.setRecMode(static_cast<RecMode>(mode));
    Serial.printf("Recording mode set to %d\n", mode);
}