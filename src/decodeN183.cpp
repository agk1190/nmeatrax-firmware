/**
 * NMEATrax
 *  
 * NMEATrax NMEA 0183 decoding file.
 * 
 * Resource
 * TinyGPSPlus kitchensink.ino demo by Mikal Hart
 */
// #define DEBUG_EN
#include <stdlib.h>
#include <TinyGPSPlus.h>
#include "decodeN2K.h"
#include "main.h"
#include "N183.h"
#include "TimeLib.h"

// UART 1
#define ESP32_RS232_TX_PIN GPIO_NUM_12
#define ESP32_RS232_RX_PIN GPIO_NUM_13
#define NMEA183_BAUD 4800

// The TinyGPSPlus object
TinyGPSPlus N183;

// Custom water sentences
TinyGPSCustom depthN183(N183, "SDDPT", 1); // $SDDPT sentence, 1st element (depth in meters)
TinyGPSCustom depthOffset(N183, "SDDPT", 2); // $SDDPT sentence, 2nd element (offset)
TinyGPSCustom tempN183(N183, "SDMTW", 1); // $IIMTW sentence, 1st element (temp)

// Custom heading sentence
TinyGPSCustom COG(N183, "GPVTG", 1); // $GPVTG sentence, 1st element (course)
TinyGPSCustom SOG(N183, "GPVTG", 5); // $GPVTG sentence, 1st element (speed)
TinyGPSCustom mag_var(N183, "GPRMC", 10); // $GPRMC sentence, 10th element (magnetic variation)

// For stats that happen every 5 seconds
unsigned long last = 0UL;

double n183lat;
double n183lon;
String n183time;
String n183date;
String n183timeString;
double n183speed;
int n183heading;
double n183wtemp;
double n183depth;
int lmonth, lday, lyear;
double n183mag_var;

// Structure to store device settings
extern Settings settings;

bool setupN183()
{
    Serial2.begin(NMEA183_BAUD, SERIAL_8N1, ESP32_RS232_RX_PIN, ESP32_RS232_TX_PIN);
    return(true);
}

void loopN183()
{
    digitalWrite(LED_0183, LOW);
    // Dispatch incoming characters
    while (Serial2.available() > 0)
        N183.encode(Serial2.read());

    // location
    if (N183.location.isUpdated())
    {
        #ifdef DEBUG_EN
        Serial.print(F("Lat= "));
        Serial.print(N183.location.lat(), 6);
        Serial.print(F(" Long= "));
        Serial.println(N183.location.lng(), 6);
        #endif
        
        digitalWrite(LED_0183, HIGH);
        n183lat = N183.location.lat();
        n183lon = N183.location.lng();
    }

    // Magnetic Variation
    else if (mag_var.isUpdated()) 
    {
        #ifdef DEBUG_EN
        Serial.print(F("Magnetic Variation= "));
        Serial.println(mag_var.value());
        #endif

        digitalWrite(LED_0183, HIGH);
        n183mag_var = atof(mag_var.value());
    }

    // date
    else if (N183.date.isUpdated())
    {
        #ifdef DEBUG_EN
        Serial.print(F("Year= "));
        Serial.print(N183.date.year());
        Serial.print(F(" Month= "));
        Serial.print(N183.date.month());
        Serial.print(F(" Day= "));
        Serial.println(N183.date.day());
        #endif

        digitalWrite(LED_0183, HIGH);
        
        lmonth = N183.date.month();
        lday = N183.date.day();
        lyear = N183.date.year();
        // n183date = lm + "/" + ld + "/" + ly + " ";
    }

    // time
    else if (N183.time.isUpdated())
    {
        #ifdef DEBUG_EN
        Serial.print(F("Hour= "));
        Serial.print(N183.time.hour());
        Serial.print(F(" Minute= "));
        Serial.print(N183.time.minute());
        Serial.print(F(" Second= "));
        Serial.println(N183.time.second());
        #endif

        digitalWrite(LED_0183, HIGH);
        //https://arduino.stackexchange.com/a/69621
        tmElements_t my_time;  // time elements structure
        time_t unix_timestamp; // a timestamp
        my_time.Second = N183.time.second();
        my_time.Hour = N183.time.hour();
        my_time.Minute = N183.time.minute();
        my_time.Day = lday;
        my_time.Month = lmonth;      
        my_time.Year = lyear - 1970; // years since 1970, so deduct 1970
        unix_timestamp = makeTime(my_time);
        unix_timestamp = unix_timestamp+(settings.timeZone*3600);
        n183timeString = asctime(gmtime(&unix_timestamp));
        struct timeval tv;
        tv.tv_sec = unix_timestamp;
        settimeofday(&tv, NULL);    // set ESP32 time to GPS time

        #ifdef DEBUG_EN
        Serial.print("Time: ");
        Serial.print(N183.time.value());
        Serial.print(" || ");
        Serial.println(n183timeString);
        #endif
    }

    // speed
    else if (SOG.isUpdated())
    {
        #ifdef DEBUG_EN
        Serial.print(F("Knots= "));
        Serial.println(SOG.value());
        #endif
        
        digitalWrite(LED_0183, HIGH);
        n183speed = atof(SOG.value()); 
    }
  
    // course
    else if (COG.isUpdated())
    {
        #ifdef DEBUG_EN
        Serial.print(F("Heading= "));
        Serial.println(COG.value());
        #endif

        digitalWrite(LED_0183, HIGH);
        n183heading = atoi(COG.value()); 
    }

    // water depth
    else if (depthN183.isUpdated())
    {
        #ifdef DEBUG_EN
        Serial.print(F("Depth= "));
        Serial.print(depthN183.value());
        #endif

        digitalWrite(LED_0183, HIGH);
        if (settings.depthUnit == "1") {
            n183depth = atof(depthN183.value());
            n183depth = n183depth + atof(depthOffset.value());
        } else {
            n183depth = (atof(depthN183.value())*3.28084);
            n183depth = n183depth + (atof(depthOffset.value())*3.28084);
        }
        #ifdef DEBUG_EN
        Serial.print(" || ");
        Serial.println(n183depth);
        #endif
    }

    // water temp
    else if (tempN183.isUpdated()) 
    {
        #ifdef DEBUG_EN
        Serial.print(F("Water Temperature= "));
        Serial.println(tempN183.value());
        #endif

        digitalWrite(LED_0183, HIGH);
        if (settings.tempUnit == "1") 
        {
            n183wtemp = atof(tempN183.value()); 
            n183wtemp = (n183wtemp*9.00/5.00)+32.00;
        } else {
            n183wtemp = atof(tempN183.value());
        }
    }

    // every (5) seconds
    else if (millis() - last > 5000)
    {
        #ifdef DEBUG_EN2
        Serial.println();

        Serial.print(F(" DIAGS: Chars="));
        Serial.print(N183.charsProcessed());
        Serial.print(F(" Sentences-with-Fix="));
        Serial.print(N183.sentencesWithFix());
        Serial.print(F(" Failed-checksum="));
        Serial.print(N183.failedChecksum());
        Serial.print(F(" Passed-checksum="));
        Serial.println(N183.passedChecksum());

        if (N183.charsProcessed() < 10)
        Serial.println(F(" WARNING: No GPS data.  Check wiring."));

        last = millis();
        Serial.println();
        #endif
    }
}