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
#include "recording.h"

#include "HardwareManager.h"
#include "CommunicationManager.h"


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
    {0,0}
};

bool nmeaSleep = false;

uint32_t evcKeepAlive;
uint32_t gpsKeepAlive;
uint32_t depthKeepAlive;

Stream *OutputStream;

CommunicationManager& comm = CommunicationManager::getInstance();

void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);

bool NMEAsetup() {
    OutputStream = &Serial;

    // Set Product information
    NMEA2000.SetProductInformation(comm.getMacAddress().c_str(),  // Manufacturer's Model serial code
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

// std::string to_string_with_precision(double value, int precision = 2) {     // ChatGPT
//     std::ostringstream oss;
//     oss << std::fixed << std::setprecision(precision);
//     oss << value;
//     return oss.str();
// }

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
        // Calculate maximum possible length for text buffer
        // Format: {"messageType":"127488","instanceID":<uint>,"data":{"rpm":<double>,"legTilt":<double>}}
        // Max uint8_t: 3 digits, max double: up to 24 chars (including sign, decimal, exponent)
        // Conservative estimate: 64 (fixed) + 3 (EngineInstance) + 2*24 (EngineSpeed, EngineTiltTrim) + 1 (null) = 116
        // But to be safe, use 160
        char text[128];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"127488\",\"instanceID\":%u,\"data\":{\"rpm\":%.0f,\"legTilt\":%.0f}}",
            EngineInstance,
            !N2kIsNA(EngineSpeed) ? EngineSpeed : -273,
            !N2kIsNA(EngineTiltTrim) ? (double)EngineTiltTrim : -273
        );
        comm.sendData(text);
        nmeaData->rpm = !N2kIsNA(EngineSpeed) ? EngineSpeed : -273;
        nmeaData->legTilt = !N2kIsNA(EngineTiltTrim) ? EngineTiltTrim : -273;

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

        nmeaData->eTemp = N2kIsNA(EngineCoolantTemp) ? -273 : EngineCoolantTemp;
        nmeaData->oTemp = N2kIsNA(EngineOilTemp) ? -273 : EngineOilTemp;
        nmeaData->oPres = N2kIsNA(EngineOilPress) ? -273 : EngineOilPress / 1000;
        nmeaData->battV = N2kIsNA(AltenatorVoltage) ? -273 : AltenatorVoltage;
        nmeaData->fuelRate = N2kIsNA(FuelRate) ? -273 : FuelRate;
        nmeaData->eHours = N2kIsNA(EngineHours) ? -273 : EngineHours/3600;

        double lpkm;
        if (nmeaData->speed > 0) {
            double _fuel_rate = nmeaData->fuelRate == -273 ? 0 : nmeaData->fuelRate;
            lpkm = _fuel_rate / (nmeaData->speed*3.6);
        } else {
            lpkm = -273;
        }
        nmeaData->fEfficiency = lpkm;

        uint32_t errorBitsCollection = (Status1.Status << 16) | Status2.Status;
        nmeaData->errorBits = errorBitsCollection;

        char text[256];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"127489\",\"instanceID\":%u,\"data\":{\"eTemp\":%.2f,\"oTemp\":%.2f,\"oPres\":%.2f,\"battV\":%.2f,\"fuelRate\":%.2f,\"eHours\":%.0f,\"efficiency\":%.3f,\"status1\":%u,\"status2\":%u}}",
            EngineInstance,
            !N2kIsNA(EngineCoolantTemp) ? EngineCoolantTemp : -273,
            !N2kIsNA(EngineOilTemp) ? EngineOilTemp : -273,
            !N2kIsNA(EngineOilPress) ? EngineOilPress / 1000 : -273,
            !N2kIsNA(AltenatorVoltage) ? AltenatorVoltage : -273,
            !N2kIsNA(FuelRate) ? FuelRate : -273,
            !N2kIsNA(EngineHours) ? EngineHours / 3600 : -273,
            !N2kIsNA(lpkm) ? lpkm : -273,
            Status1.Status, 
            Status2.Status
        );
        comm.sendData(text);

        // char text2[256];
        // snprintf(text, sizeof(text),
        //     "{\"messageType\":\"161616\",\"instanceID\":%u,\"data\":{\"status1\":%lu,\"status2\":%lu}}",
        //     EngineInstance,
        //     Status1.Status,
        //     Status2.Status
        // );
        // comm.sendData(text2);

        // char errorBits[32];
        
        // snprintf(errorBits, sizeof(errorBits), "%u", errorBitsCollection);

        // strcpy(nmeaData->errorBits, errorBitsCollection);
        // nmeaData->errorBits = String(Status1.Status) + ";" + String(Status2.Status);

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
                // strcpy(nmeaData->gear, "F");
                nmeaData->gear = 'F';
                break;
            case N2kTG_Neutral:
                nmeaData->gear = 'N';
                break;
            case N2kTG_Reverse:
                nmeaData->gear = 'R';
                break;
            default:
                nmeaData->gear = '-';
                break;
        }

        char text[128];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"127493\",\"instanceID\":%u,\"data\":{\"gear\":\"%c\",\"oTemp\":%.2f,\"oPres\":%.2f}}",
            EngineInstance,
            nmeaData->gear,
            !N2kIsNA(OilTemperature) ? OilTemperature : -273,
            !N2kIsNA(OilPressure) ? OilPressure / 1000 : -273
        );
        comm.sendData(text);
        
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
            nmeaData->speed = ReturnWithConversionCheckUnDef(SOG);
            nmeaData->heading = ReturnWithConversionCheckUnDef(COG,&RadToDeg);

            char text[128];
            snprintf(text, sizeof(text),
                "{\"messageType\":\"129026\",\"instanceID\":%u,\"data\":{\"sog\":%.2f,\"cog\":%.2f}}",
                SID,
                !N2kIsNA(SOG) ? ReturnWithConversionCheckUnDef(SOG) : -273,
                !N2kIsNA(COG) ? ReturnWithConversionCheckUnDef(COG,&RadToDeg) : -273
            );
            comm.sendData(text);
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
        nmeaData->lat = N2kIsNA(Latitude) ? -273 : Latitude, 6;
        nmeaData->lon = N2kIsNA(Longitude) ? -273 : Longitude, 6;
        unixTime = ((DaysSince1970*86400)+SecondsSinceMidnight);
        nmeaData->unixTime = unixTime;
        struct timeval tv;
        tv.tv_sec = unixTime;
        settimeofday(&tv, NULL);    // set ESP32 time to GPS time

        char text[128];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"129029\",\"instanceID\":%u,\"data\":{\"unixTime\":%" PRIu64 ",\"lat\":%.6f,\"lon\":%.6f}}",
            SID,
            unixTime,
            !N2kIsNA(Latitude) ? Latitude : -273,
            !N2kIsNA(Longitude) ? Longitude : -273
        );
        comm.sendData(text);

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
            nmeaData->wTemp = N2kIsNA(ActualTemperature) ? -273 : ActualTemperature;
        }

        char text[160];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"130312\",\"instanceID\":%u,\"data\":{\"tempInstance\":%u,\"tempSource\":%u,\"actualTemp\":%.2f,\"setTemp\":%.2f}}",
            SID,
            TempInstance,
            TempSource,
            !N2kIsNA(ActualTemperature) ? ActualTemperature : -273,
            !N2kIsNA(SetTemperature) ? SetTemperature : -273
        );
        comm.sendData(text);

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

        char text[128];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"128267\",\"instanceID\":%u,\"data\":{\"depth\":%.2f,\"offset\":%.2f}}",
            SID,
            !N2kIsNA(DepthBelowTransducer) ? DepthBelowTransducer : -273,
            !N2kIsNA(Offset) ? Offset : -273
        );
        comm.sendData(text);

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
                nmeaData->depth = DepthBelowTransducer+Offset;
                depthKeepAlive = millis();
            } else {
                #ifdef DEBUG_EN
                OutputStream->println(" not available");
                #endif
                nmeaData->depth = -273;
            }
        }
    }
}

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
        nmeaData->fLevel = (!N2kIsNA(Level) && FluidType == N2kft_Fuel) ? Level : -273;

        char text[128];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"127505\",\"instanceID\":%u,\"data\":{\"fluidType\":%u,\"level\":%.1f,\"capacity\":%.1f}}",
            Instance,
            FluidType,
            (!N2kIsNA(Level) && FluidType == N2kft_Fuel) ? Level : -273,
            !N2kIsNA(Capacity) ? Capacity : -273
        );
        comm.sendData(text);
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
        nmeaData->magVar = ReturnWithConversionCheckUnDef(Variation, &RadToDeg);

        char text[128];
        snprintf(text, sizeof(text),
            "{\"messageType\":\"127258\",\"instanceID\":%u,\"data\":{\"magVar\":%.2f}}",
            SID,
            ReturnWithConversionCheckUnDef(Variation, &RadToDeg)
        );
        comm.sendData(text);

    } else {OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);}
}

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
    double nValid = -273;
    if (evcKeepAlive + 1000 < millis()) {
        if (evcKeepAlive + 2000 > millis()) {
            nmeaData->rpm = nValid;
            nmeaData->eTemp = nValid;
            nmeaData->oTemp = nValid;
            nmeaData->oPres = nValid;
            nmeaData->fuelRate = nValid;
            nmeaData->fLevel = nValid;
            nmeaData->fEfficiency = nValid;
            nmeaData->legTilt = nValid;
            nmeaData->battV = nValid;
            nmeaData->eHours = nValid;
            nmeaData->gear = '-';
            nmeaData->errorBits = 0;
        }  
    }
    if (gpsKeepAlive + 1000 < millis()) {
        if (gpsKeepAlive + 2000 > millis()) {
            nmeaData->lat = nValid;
            nmeaData->lon = nValid;
            nmeaData->speed = nValid;
            nmeaData->heading = nValid;
            nmeaData->magVar = nValid;
            nmeaData->unixTime = 0;
            nmeaData->depth = nValid;
            nmeaData->wTemp = nValid;
        }
    }
    if (depthKeepAlive + 5000 < millis()) {
        if (depthKeepAlive + 6000 > millis()) {
            nmeaData->depth = nValid;
        }
    }
    
    if (evcKeepAlive + 1000 < millis() && gpsKeepAlive + 1000 < millis() && depthKeepAlive + 1000 < millis()) {
        digitalWrite(LED_N2K, LOW);
        nmeaSleep = true;
        vTaskSuspend(NULL);
    }
}
