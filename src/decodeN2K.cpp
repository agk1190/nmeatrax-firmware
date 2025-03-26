/**
 * NMEATrax
 *  
 * NMEATrax NMEA 2000 decoding file.
 * 
 * Resource
 * NMEA2000 by Timo Lappalainen
 */

// UART 2
#define ESP32_CAN_TX_PIN GPIO_NUM_17
#define ESP32_CAN_RX_PIN GPIO_NUM_16

#include <NMEA2000_CAN.h>
#include <N2kMessagesEnumToStr.h>

#include "decodeN2K.h"
#include "main.h"
#include "webserv.h"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip> // For std::setprecision

typedef struct {
    unsigned long PGN;
    void (*Handler)(const tN2kMsg &N2kMsg); 
} tNMEA2000Handler;

void EngineRapid(const tN2kMsg &N2kMsg);
void EngineDynamicParameters(const tN2kMsg &N2kMsg);
void TransmissionParameters(const tN2kMsg &N2kMsg);
void WaterDepth(const tN2kMsg &N2kMsg);
void Temperature(const tN2kMsg &N2kMsg);
void COGSOG(const tN2kMsg &N2kMsg);
void GNSS(const tN2kMsg &N2kMsg);
void MagneticVariation(const tN2kMsg &N2kMsg);
void FluidLevel(const tN2kMsg &N2kMsg);
// void NavigationInfo(const tN2kMsg &N2kMsg);

tNMEA2000Handler NMEA2000Handlers[]={
    {127258L,&MagneticVariation},
    {127488L,&EngineRapid},
    {127489L,&EngineDynamicParameters},
    {127493L,&TransmissionParameters},
    {127505L,&FluidLevel},
    {128267L,&WaterDepth},
    {129026L,&COGSOG},
    {129029L,&GNSS},
    {130312L,&Temperature},
    // {129284L,&NavigationInfo},
    {0,0}
};

uint32_t evcKeepAlive;
uint32_t gpsKeepAlive;
uint32_t depthKeepAlive;

Stream *OutputStream;

void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);

bool NMEAsetup() {
    OutputStream = &Serial;

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

    // Set Product information
    NMEA2000.SetProductInformation(macAddrStr.c_str(),  // Manufacturer's Model serial code
                                    101,                // Manufacturer's product code
                                    "NMEATrax",         // Manufacturer's Model ID
                                    FW_VERSION,         // Manufacturer's Software version code
                                    "2.0",              // Manufacturer's Model version
                                    2                   // Load Equivalency
                                    );
    // Set device information
    NMEA2000.SetDeviceInformation(5,        // Unique number. Use e.g. Serial number.
                                    140,    // Device function=Analog to NMEA 2000 Gateway. See codes on https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                    20,     // Device class=Inter/Intranetwork Device. See codes on  https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                    2040    // Just choosen free from code list on https://web.archive.org/web/20190529161431/http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                                    );

    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode,7);

    NMEA2000.SetN2kCANReceiveFrameBufSize(150);
    // Do not forward bus messages at all
    NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
    NMEA2000.SetForwardStream(OutputStream);
    // Set false below, if you do not want to see messages parsed to HEX withing library
    NMEA2000.EnableForward(false);
    NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
    NMEA2000.SetN2kCANMsgBufSize(8);
    // NMEA2000.SetOnOpen(OnN2kOpen);
    NMEA2000.Open();
    OutputStream->println("Running...");
    return(true);
}

//*****************************************************************************
template<typename T> void PrintLabelValWithConversionCheckUnDef(const char* label, T val, double (*ConvFunc)(double val)=0, bool AddLf=false, int8_t Desim=-1 ) {
    OutputStream->print(label);
    if (!N2kIsNA(val)) {
        if ( Desim<0 ) {
            if (ConvFunc) { OutputStream->print(ConvFunc(val)); } else { OutputStream->print(val); }
        } else {
            if (ConvFunc) { OutputStream->print(ConvFunc(val),Desim); } else { OutputStream->print(val,Desim); }
        }
    } else OutputStream->print("not available");
    if (AddLf) OutputStream->println();
}

template<typename T> double ReturnWithConversionCheckUnDef(T val, double (*ConvFunc)(double val)=0, int8_t Desim=-1 ) {
    if (!N2kIsNA(val)) {
        if ( Desim<0 ) {
            if (ConvFunc) { return(ConvFunc(val)); } else { return(val); }
        } else {
            if (ConvFunc) { return(ConvFunc(val),Desim); } else { return(val,Desim); }
        }
    } else return(-273);
}

std::string to_string_with_precision(double value, int precision = 2) {     // ChatGPT
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    oss << value;
    return oss.str();
}

//*****************************************************************************
void EngineRapid(const tN2kMsg &N2kMsg) {
    unsigned char EngineInstance;
    double EngineSpeed;
    double EngineBoostPressure;
    int8_t EngineTiltTrim;
    
    digitalWrite(LED_N2K, HIGH);
    evcKeepAlive = millis();
    if (ParseN2kEngineParamRapid(N2kMsg,EngineInstance,EngineSpeed,EngineBoostPressure,EngineTiltTrim) ) {
        #ifdef DEBUG_EN
        PrintLabelValWithConversionCheckUnDef("Engine rapid params: ",EngineInstance,0,true);
        PrintLabelValWithConversionCheckUnDef("  RPM: ",EngineSpeed,0,true);
        PrintLabelValWithConversionCheckUnDef("  boost pressure (Pa): ",EngineBoostPressure,0,true);
        PrintLabelValWithConversionCheckUnDef("  tilt trim: ",EngineTiltTrim,0,true);
        #endif
        
        if (EngineSpeed >= 16383.50) {return;}
        if (EngineTiltTrim == N2kInt8NA - 1) return;
        std::string text = "{\"messageType\":\"127488\",\"instanceID\":" + std::to_string(EngineInstance) + 
                            ",\"data\":{\"rpm\":" + to_string_with_precision(!N2kIsNA(EngineSpeed) ? EngineSpeed : -273, 0) + 
                            ",\"legTilt\":" + to_string_with_precision(!N2kIsNA(EngineTiltTrim) ? EngineTiltTrim : -273, 0) + 
                            "}}";
        sendToWebQueue(text.c_str());
        nmeaData[0] = String(!N2kIsNA(EngineSpeed) ? EngineSpeed : -273, 0);        // removed > 10000 check for testing
        nmeaData[7] = String(!N2kIsNA(EngineTiltTrim) ? EngineTiltTrim : -273);

    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
void EngineDynamicParameters(const tN2kMsg &N2kMsg) {
    unsigned char EngineInstance;
    double EngineOilPress;
    double EngineOilTemp;
    double EngineCoolantTemp;
    double AltenatorVoltage;
    double FuelRate;
    double EngineHours;
    double EngineCoolantPress;
    double EngineFuelPress; 
    int8_t EngineLoad;
    int8_t EngineTorque;
    tN2kEngineDiscreteStatus1 Status1;
    tN2kEngineDiscreteStatus2 Status2;
    
    digitalWrite(LED_N2K, HIGH);
    evcKeepAlive = millis();
    if (ParseN2kEngineDynamicParam(N2kMsg,EngineInstance,EngineOilPress,EngineOilTemp,EngineCoolantTemp,
                                    AltenatorVoltage,FuelRate,EngineHours,
                                    EngineCoolantPress,EngineFuelPress,
                                    EngineLoad,EngineTorque,Status1,Status2)) {
        #ifdef DEBUG_EN
        PrintLabelValWithConversionCheckUnDef("Engine dynamic params: ",EngineInstance,0,true);
        PrintLabelValWithConversionCheckUnDef("  oil pressure (Pa): ",EngineOilPress,0,true);
        PrintLabelValWithConversionCheckUnDef("  oil temp (C): ",EngineOilTemp,&KelvinToC,true);
        PrintLabelValWithConversionCheckUnDef("  coolant temp (C): ",EngineCoolantTemp,&KelvinToC,true);
        PrintLabelValWithConversionCheckUnDef("  altenator voltage (V): ",AltenatorVoltage,0,true);
        PrintLabelValWithConversionCheckUnDef("  fuel rate (l/h): ",FuelRate,0,true);
        PrintLabelValWithConversionCheckUnDef("  engine hours (h): ",EngineHours,&SecondsToh,true);
        PrintLabelValWithConversionCheckUnDef("  coolant pressure (Pa): ",EngineCoolantPress,0,true);
        PrintLabelValWithConversionCheckUnDef("  fuel pressure (Pa): ",EngineFuelPress,0,true);
        PrintLabelValWithConversionCheckUnDef("  engine load (%): ",EngineLoad,0,true);
        PrintLabelValWithConversionCheckUnDef("  engine torque (%): ",EngineTorque,0,true);
        #endif

        if (EngineCoolantTemp == (N2kUInt16NA - 1) * 0.01) return;
        if (FuelRate >= 3275.0) return;

        nmeaData[1] = String(N2kIsNA(EngineCoolantTemp) ? -273 : EngineCoolantTemp, 2);
        nmeaData[2] = String(N2kIsNA(EngineOilTemp) ? -273 : EngineOilTemp, 2);
        nmeaData[3] = String(N2kIsNA(EngineOilPress) ? -273 : EngineOilPress / 1000, 0);
        nmeaData[12] = String(N2kIsNA(AltenatorVoltage) ? -273 : AltenatorVoltage, 2);
        nmeaData[4] = String(N2kIsNA(FuelRate) ? -273 : FuelRate, 1);
        nmeaData[13] = String(N2kIsNA(EngineHours) ? -273 : EngineHours/3600, 0);

        double lpkm;
        if (nmeaData[8].toDouble() > 0) {
            double _fuel_rate = (nmeaData[4].toDouble() == -273) ? 0 : nmeaData[4].toDouble();
            lpkm = _fuel_rate / (nmeaData[8].toDouble()*3.6);
        } else {
            lpkm = -273;
        }

        std::string text = "{\"messageType\":\"127489\",\"instanceID\":" + std::to_string(EngineInstance) + 
                            ",\"data\":{\"eTemp\":" + to_string_with_precision(N2kIsNA(EngineCoolantTemp) ? -273 : EngineCoolantTemp) + 
                            ",\"oTemp\":" + to_string_with_precision(N2kIsNA(EngineOilTemp) ? -273 : EngineOilTemp) + 
                            ",\"oPres\":" + to_string_with_precision(N2kIsNA(EngineOilPress) ? -273 : EngineOilPress / 1000) + 
                            ",\"battV\":" + to_string_with_precision(N2kIsNA(AltenatorVoltage) ? -273 : AltenatorVoltage) + 
                            ",\"fuelRate\":" + to_string_with_precision(N2kIsNA(FuelRate) ? -273 : FuelRate) + 
                            ",\"eHours\":" + to_string_with_precision(N2kIsNA(EngineHours) ? -273 : EngineHours/3600, 0) + 
                            ",\"efficiency\":" + to_string_with_precision(lpkm, 3) + 
                            "}}";
        sendToWebQueue(text.c_str());
        nmeaData[6] = String(lpkm, 3);

        std::string errors = "{\"messageType\":\"161616\",\"instanceID\":" + std::to_string(EngineInstance) + 
                            ",\"data\":{\"status1\":" + std::to_string(Status1.Status) + 
                            ",\"status2\":" + std::to_string(Status2.Status) + "}}";
        sendToWebQueue(errors.c_str());
        nmeaData[19] = String(Status1.Status) + ";" + String(Status2.Status);

    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
void TransmissionParameters(const tN2kMsg &N2kMsg) {
    unsigned char EngineInstance;
    tN2kTransmissionGear TransmissionGear;
    double OilPressure;
    double OilTemperature;
    unsigned char DiscreteStatus1;

    digitalWrite(LED_N2K, HIGH);
    evcKeepAlive = millis();
    if (ParseN2kTransmissionParameters(N2kMsg,EngineInstance, TransmissionGear, OilPressure, OilTemperature, DiscreteStatus1)) {
        #ifdef DEBUG_EN
        PrintLabelValWithConversionCheckUnDef("Transmission params: ",EngineInstance,0,true);
                            OutputStream->print("  gear: "); PrintN2kEnumType(TransmissionGear,OutputStream);
        PrintLabelValWithConversionCheckUnDef("  oil pressure (Pa): ",OilPressure,0,true);
        PrintLabelValWithConversionCheckUnDef("  oil temperature (C): ",OilTemperature,&KelvinToC,true);
        PrintLabelValWithConversionCheckUnDef("  discrete status: ",DiscreteStatus1,0,true);
        #endif
        switch(TransmissionGear) {
            case N2kTG_Forward:
                nmeaData[14] = "F";
                break;
            case N2kTG_Neutral:
                nmeaData[14] = "N";
                break;
            case N2kTG_Reverse:
                nmeaData[14] = "R";
                break;
            default:
                nmeaData[14] = "-";
                break;
        }

        std::string text = "{\"messageType\":\"127493\",\"instanceID\":" + std::to_string(EngineInstance) + 
                        ",\"data\":{\"gear\":\"" + nmeaData[14].c_str() + 
                        "\",\"oTemp\":" + to_string_with_precision(N2kIsNA(OilTemperature) ? -273 : OilTemperature) + 
                        ",\"oPres\":" + to_string_with_precision(N2kIsNA(OilPressure) ? -273 : OilPressure / 1000) + 
                        "}}";
        sendToWebQueue(text.c_str());
        
    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
void COGSOG(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    tN2kHeadingReference HeadingReference;
    double COG;
    double SOG;
    
    digitalWrite(LED_N2K, HIGH);
    gpsKeepAlive = millis();
    if (ParseN2kCOGSOGRapid(N2kMsg,SID,HeadingReference,COG,SOG) ) {
        #ifdef DEBUG_EN
                        OutputStream->println("COG/SOG:");
        PrintLabelValWithConversionCheckUnDef("  SID: ",SID,0,true);
                        OutputStream->print("  reference: "); PrintN2kEnumType(HeadingReference,OutputStream);
        PrintLabelValWithConversionCheckUnDef("  COG (deg): ",COG,&RadToDeg,true);
        PrintLabelValWithConversionCheckUnDef("  SOG (m/s): ",SOG,0,true);
        #endif
        if (HeadingReference == 0 || HeadingReference == 1) {
            nmeaData[8] = String(ReturnWithConversionCheckUnDef(SOG));
            nmeaData[9] = String(ReturnWithConversionCheckUnDef(COG,&RadToDeg));

            std::string text = "{\"messageType\":\"129026\",\"instanceID\":" + std::to_string(SID) + 
                            ",\"data\":{\"sog\":" + to_string_with_precision(N2kIsNA(SOG) ? -273 : ReturnWithConversionCheckUnDef(SOG)) + 
                            ",\"cog\":" + to_string_with_precision(N2kIsNA(COG) ? -273 : ReturnWithConversionCheckUnDef(COG,&RadToDeg), 0) + 
                            "}}";
            sendToWebQueue(text.c_str());
        }
    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
void GNSS(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    uint16_t DaysSince1970;
    double SecondsSinceMidnight; 
    double Latitude;
    double Longitude;
    double Altitude; 
    tN2kGNSStype GNSStype;
    tN2kGNSSmethod GNSSmethod;
    unsigned char nSatellites;
    double HDOP;
    double PDOP;
    double GeoidalSeparation;
    unsigned char nReferenceStations;
    tN2kGNSStype ReferenceStationType;
    uint16_t ReferenceSationID;
    double AgeOfCorrection;

    digitalWrite(LED_N2K, HIGH);
    gpsKeepAlive = millis();
    if (ParseN2kGNSS(N2kMsg,SID,DaysSince1970,SecondsSinceMidnight,
                Latitude,Longitude,Altitude,
                GNSStype,GNSSmethod,
                nSatellites,HDOP,PDOP,GeoidalSeparation,
                nReferenceStations,ReferenceStationType,ReferenceSationID,
                AgeOfCorrection) ) {
        #ifdef DEBUG_EN
                        OutputStream->println("GNSS info:");
        PrintLabelValWithConversionCheckUnDef("  SID: ",SID,0,true);
        PrintLabelValWithConversionCheckUnDef("  days since 1.1.1970: ",DaysSince1970,0,true);
        PrintLabelValWithConversionCheckUnDef("  seconds since midnight: ",SecondsSinceMidnight,0,true);
        PrintLabelValWithConversionCheckUnDef("  latitude: ",Latitude,0,true,9);
        PrintLabelValWithConversionCheckUnDef("  longitude: ",Longitude,0,true,9);
        PrintLabelValWithConversionCheckUnDef("  altitude: (m): ",Altitude,0,true);
                        OutputStream->print("  GNSS type: "); PrintN2kEnumType(GNSStype,OutputStream);
                        OutputStream->print("  GNSS method: "); PrintN2kEnumType(GNSSmethod,OutputStream);
        PrintLabelValWithConversionCheckUnDef("  satellite count: ",nSatellites,0,true);
        PrintLabelValWithConversionCheckUnDef("  HDOP: ",HDOP,0,true);
        PrintLabelValWithConversionCheckUnDef("  PDOP: ",PDOP,0,true);
        PrintLabelValWithConversionCheckUnDef("  geoidal separation: ",GeoidalSeparation,0,true);
        PrintLabelValWithConversionCheckUnDef("  reference stations: ",nReferenceStations,0,true);
        #endif

        uint64_t unixTime;
        nmeaData[15] = String(N2kIsNA(Latitude) ? -273 : Latitude, 6);
        nmeaData[16] = String(N2kIsNA(Longitude) ? -273 : Longitude, 6);
        unixTime = ((DaysSince1970*86400)+SecondsSinceMidnight);
        nmeaData[18] = String(unixTime);
        struct timeval tv;
        tv.tv_sec = unixTime;
        settimeofday(&tv, NULL);    // set ESP32 time to GPS time

        std::string text = "{\"messageType\":\"129029\",\"instanceID\":" + std::to_string(SID) + 
                            ",\"data\":{\"unixTime\":" + std::to_string(unixTime) + 
                            ",\"lat\":" + to_string_with_precision(N2kIsNA(Latitude) ? -273 : Latitude, 6) + 
                            ",\"lon\":" + to_string_with_precision(N2kIsNA(Longitude) ? -273 : Longitude, 6) + 
                            "}}";
        sendToWebQueue(text.c_str());

    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
void Temperature(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    unsigned char TempInstance;
    tN2kTempSource TempSource;
    double ActualTemperature;
    double SetTemperature;
    
    digitalWrite(LED_N2K, HIGH);
    gpsKeepAlive = millis();
    if (ParseN2kTemperature(N2kMsg,SID,TempInstance,TempSource,ActualTemperature,SetTemperature) ) {
        #ifdef DEBUG_EN
                        OutputStream->print("Temperature source: "); PrintN2kEnumType(TempSource,OutputStream,false);
        PrintLabelValWithConversionCheckUnDef(", actual temperature: ",ActualTemperature,&KelvinToC);
        PrintLabelValWithConversionCheckUnDef(", set temperature: ",SetTemperature,&KelvinToC,true);
        #endif
        if (TempSource == N2kts_SeaTemperature) {
            nmeaData[11] = String(N2kIsNA(ActualTemperature) ? -273 : ActualTemperature, 2);
        }

        std::string text = "{\"messageType\":\"130312\",\"instanceID\":" + std::to_string(SID) + 
                            ",\"data\":{\"tempInstance\":" + std::to_string(TempInstance) + 
                            ",\"tempSource\":" + std::to_string(TempSource) + 
                            ",\"actualTemp\":" + to_string_with_precision(N2kIsNA(ActualTemperature) ? -273 : ActualTemperature) + 
                            ",\"setTemp\":" + to_string_with_precision(N2kIsNA(SetTemperature) ? -273 : SetTemperature) + 
                            "}}";
        sendToWebQueue(text.c_str());

    } else {OutputStream->print("Failed to parse PGN: ");  OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
void WaterDepth(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    double DepthBelowTransducer;
    double Offset;

    digitalWrite(LED_N2K, HIGH);
    gpsKeepAlive = millis();
    if (ParseN2kWaterDepth(N2kMsg,SID,DepthBelowTransducer,Offset)) {
        
        std::string text = "{\"messageType\":\"128267\",\"instanceID\":" + std::to_string(SID) + 
                            ",\"data\":{\"depth\":" + to_string_with_precision(N2kIsNA(DepthBelowTransducer) ? -273 : DepthBelowTransducer) + 
                            ",\"offset\":" + to_string_with_precision(N2kIsNA(Offset) ? -273 : Offset) + 
                            "}}";
        sendToWebQueue(text.c_str());

        if (N2kIsNA(Offset) || Offset == 0) {
            #ifdef DEBUG_EN
            PrintLabelValWithConversionCheckUnDef("Depth below transducer ",DepthBelowTransducer);
            // n2kdepth = DepthBelowTransducer;
            if (N2kIsNA(Offset)) {
                OutputStream->println(", offset not available");
            } else {
                OutputStream->println(", offset=0");
            }
            #endif
        } else {
            #ifdef DEBUG_EN
            if (Offset>0) {
                OutputStream->print("Water depth:");
            } else {
                OutputStream->print("Depth below keel:");
            }
            #endif
            if (!N2kIsNA(DepthBelowTransducer)) {
                #ifdef DEBUG_EN
                OutputStream->println(DepthBelowTransducer+Offset);
                #endif
                nmeaData[10] = String(DepthBelowTransducer+Offset, 2);
                depthKeepAlive = millis();
            } else {
                #ifdef DEBUG_EN
                OutputStream->println(" not available");
                #endif
                nmeaData[10] = "-273";
            }
        }
    }
}

//*****************************************************************************
// void printLLNumber(Stream *OutputStream, unsigned long long n, uint8_t base=10)
// {
//     unsigned char buf[16 * sizeof(long)]; // Assumes 8-bit chars.
//     unsigned long long i = 0;
//
//     if (n == 0) {
//         OutputStream->print('0');
//         return;
//     }
//
//     while (n > 0) {
//         buf[i++] = n % base;
//         n /= base;
//     }
//
//     for (; i > 0; i--)
//         OutputStream->print((char) (buf[i - 1] < 10 ?
//         '0' + buf[i - 1] :
//         'A' + buf[i - 1] - 10));
// }

//*****************************************************************************
void FluidLevel(const tN2kMsg &N2kMsg) {
    unsigned char Instance;
    tN2kFluidType FluidType;
    double Level=0;
    double Capacity=0;

    digitalWrite(LED_N2K, HIGH);
    evcKeepAlive = millis();
    if (ParseN2kFluidLevel(N2kMsg,Instance,FluidType,Level,Capacity) ) {
        #ifdef DEBUG_EN
        switch (FluidType) {
            case N2kft_Fuel:
                OutputStream->print("Fuel level :");
                break;
            case N2kft_Water:
                OutputStream->print("Water level :");
                break;
            case N2kft_GrayWater:
                OutputStream->print("Gray water level :");
                break;
            case N2kft_LiveWell:
                OutputStream->print("Live well level :");
                break;
            case N2kft_Oil:
                OutputStream->print("Oil level :");
                break;
            case N2kft_BlackWater:
                OutputStream->print("Black water level :");
                break;
            case N2kft_FuelGasoline:
                OutputStream->print("Gasoline level :");
                break;
            case N2kft_Error:
                OutputStream->print("Error level :");
                break;
            case N2kft_Unavailable:
                OutputStream->print("Unknown level :");
                break;
        }
        OutputStream->print(Level); OutputStream->print("%"); 
        OutputStream->print(" ("); OutputStream->print(Capacity*Level/100); OutputStream->print(")L");
        OutputStream->print(" capacity :"); OutputStream->println(Capacity);
        #endif
        nmeaData[5] = String((!N2kIsNA(Level) && FluidType == N2kft_Fuel) ? Level : -273, 1);
        std::string text = "{\"messageType\":\"127505\",\"instanceID\":" + std::to_string(Instance) + 
                            ",\"data\":{\"fluidType\":" + to_string_with_precision(FluidType) + 
                            ",\"level\":" + to_string_with_precision((!N2kIsNA(Level) && FluidType == N2kft_Fuel) ? Level : -273) + 
                            ",\"capacity\":" + to_string_with_precision(N2kIsNA(Capacity) ? -273 : Capacity) + 
                            "}}";
        sendToWebQueue(text.c_str());
    }
}

//*****************************************************************************
void MagneticVariation(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    tN2kMagneticVariation Source;
    uint16_t AgeOfService;
    double Variation;
    
    digitalWrite(LED_N2K, HIGH);
    gpsKeepAlive = millis();
    if (ParseN2kMagneticVariation(N2kMsg,SID, Source, AgeOfService, Variation) ) {
        #ifdef DEBUG_EN
                        OutputStream->println("Magnetic Variation:");
        PrintLabelValWithConversionCheckUnDef("  SID: ",SID,0,true);
                        OutputStream->print("  Variation Source: "); PrintN2kEnumType(Source,OutputStream,true);
        PrintLabelValWithConversionCheckUnDef("  Variation ",Variation,&RadToDeg,true);
        #endif
        nmeaData[17] = String(ReturnWithConversionCheckUnDef(Variation, &RadToDeg), 2);

        std::string text = "{\"messageType\":\"127258\",\"instanceID\":" + std::to_string(SID) + 
                            ",\"data\":{\"magVar\":" + to_string_with_precision(ReturnWithConversionCheckUnDef(Variation, &RadToDeg), 2) + 
                            "}}";
        sendToWebQueue(text.c_str());

    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

//*****************************************************************************
// void NavigationInfo(const tN2kMsg &N2kMsg) {
//     unsigned char SID;
//     double DistanceToWaypoint;
//     tN2kHeadingReference BearingReference;
//     bool PerpendicularCrossed;
//     bool ArrivalCircleEntered;
//     tN2kDistanceCalculationType CalculationType;
//     double ETATime;
//     int16_t ETADate;
//     double BearingOriginToDestinationWaypoint;
//     double BearingPositionToDestinationWaypoint;
//     uint32_t OriginWaypointNumber;
//     uint32_t DestinationWaypointNumber;
//     double DestinationLatitude;
//     double DestinationLongitude;
//     double WaypointClosingVelocity;
//     String info;
//
//     // digitalWrite(LED_N2K, HIGH);
//     // gpsKeepAlive = millis();
//
//     if (ParseN2kNavigationInfo(N2kMsg, SID, 
//             DistanceToWaypoint, 
//             BearingReference, PerpendicularCrossed, 
//             ArrivalCircleEntered, CalculationType, 
//             ETATime, ETADate, BearingOriginToDestinationWaypoint, 
//             BearingPositionToDestinationWaypoint, OriginWaypointNumber, 
//             DestinationWaypointNumber, DestinationLatitude, 
//             DestinationLongitude, WaypointClosingVelocity)) {
//      
//         // if (!N2kIsNA(DistanceToWaypoint)) {
//         //     info.concat(DistanceToWaypoint);
//         //     info.concat("m to dest,");
//         // }
//         // // info.concat(BearingReference ? "Magnetic" : "True");
//         // // info.concat(PerpendicularCrossed);
//         // // info.concat(ArrivalCircleEntered);
//         // // info.concat(CalculationType ? "Rhumb Line" : "Great Circle");
//         // if (!N2kIsNA(ETATime)) {
//         //     info.concat("Arr Time:");
//         //     info.concat(ETATime);
//         // }
//         // if (!N2kIsNA(ETADate)) {
//         //     info.concat("Arr Date:");
//         //     info.concat(ETADate);
//         // }
//         // // info.concat(BearingOriginToDestinationWaypoint);
//         // // info.concat(BearingPositionToDestinationWaypoint);
//         // // info.concat(OriginWaypointNumber);
//         // // info.concat(DestinationWaypointNumber);
//         // // info.concat(DestinationLatitude);
//         // // info.concat(DestinationLongitude);
//         // if (!N2kIsNA(WaypointClosingVelocity)) {
//         //     info.concat("Closing Velocity:");
//         //     info.concat(WaypointClosingVelocity);
//         // }
//         // // info.concat(WaypointClosingVelocity);
//         // nmeaTraxGenericMsg = info;
//     } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
// }

//*****************************************************************************
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {
    int iHandler;
    
    // Find handler
    #ifdef DEBUG_EN
    OutputStream->print("In Main Handler: "); OutputStream->println(N2kMsg.PGN);
    #endif
    for (iHandler=0; NMEA2000Handlers[iHandler].PGN!=0 && !(N2kMsg.PGN==NMEA2000Handlers[iHandler].PGN); iHandler++);
    
    if (NMEA2000Handlers[iHandler].PGN!=0) {
        NMEA2000Handlers[iHandler].Handler(N2kMsg); 
    }
}

//*****************************************************************************
void NMEAloop() {
    NMEA2000.ParseMessages();
    String nValid = "-273";
    if (evcKeepAlive + 1000 < millis()) {
        if (evcKeepAlive + 2000 > millis()) {
            for (int i = 0; i <= 8; ++i) {
                nmeaData[i] = nValid;
            }
            nmeaData[12] = nValid;
            nmeaData[13] = "0";
            nmeaData[14] = "-";
            nmeaData[19] = "-";
        }  
    }
    if (gpsKeepAlive + 1000 < millis()) {
        if (gpsKeepAlive + 2000 > millis()) {
            nmeaData[8] = nValid;
            nmeaData[9] = nValid;
            nmeaData[10] = nValid;
            nmeaData[11] = nValid;
            nmeaData[15] = nValid;
            nmeaData[16] = nValid;
            nmeaData[17] = nValid;
            nmeaData[18] = "0";
        }
    }
    if (depthKeepAlive + 5000 < millis()) {
        if (depthKeepAlive + 6000 > millis()) {
            nmeaData[10] = nValid;
        }
    }
    
    if (evcKeepAlive + 1000 < millis() && gpsKeepAlive + 1000 < millis() && depthKeepAlive + 1000 < millis()) {
        digitalWrite(LED_N2K, LOW);
        NMEAsleep = true;
        vTaskSuspend(NULL);
    }
}
