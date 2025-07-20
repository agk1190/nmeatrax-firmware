#include <Arduino.h>

extern bool outOfIdle;

// Define a struct for NMEA data fields with named members for clarity
struct NMEAData {
    int rpm;
    double eTemp;
    double oTemp;
    double oPres;
    double fuelRate;
    double fLevel;
    double fEfficiency;
    int legTilt;
    double speed;
    double heading;
    double depth;
    double wTemp;
    double battV;
    int eHours;
    char gear;
    double lat;
    double lon;
    double magVar;
    time_t unixTime;
    uint32_t errorBits;
    // char errorBits[34];
};

// // Store an array of NMEAData structs
// static NMEAData nmeaData[20];
extern NMEAData *nmeaData;

/**
 * @brief Complie data in CSV format to send to the user.
 * @param none
 * @return A string of comma separated values containing the NMEA data.
*/
String getCSV();

/**
 * @brief Main loop for the recorder task.
 * This function checks the recording mode and updates the recording interval
 * based on the current NMEA data. It also handles the transition between
 * different recording modes.
*/
void recorderLoop();

/**
 * @brief Write the current recording data to the SD card.
 * This function checks if the logging task is created and retrieves the CSV data
 * to write to the SD card. If no data is available, it returns false.
 * @return True if data was written successfully, false otherwise.
*/
bool writeRecording();

/**
 * @brief Task to write recording data to the SD card.
 * This function runs in a loop, appending the CSV data to the specified file
 * on the SD card. It suspends itself when not needed.
*/
void vWriteRecording(void * pvParameters);