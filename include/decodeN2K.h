/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax NMEA 2000 decoding header file.
 */

extern int rpm;
extern double depth;
extern double speed;
extern int heading;
extern double etemp;
extern double otemp;
extern double wtemp;
extern double lat;
extern double lon;
extern double mag_var;
extern int leg_tilt;
extern double opres;
extern double battV;
extern double fuel_rate;
extern uint32_t ehours;
extern String gear;
extern double flevel;
extern double lpkm;
extern uint64_t unixTime;
extern String evcErrorMsg;
extern String nmeaTraxGenericMsg;

// ***** For testing *****
// #include "NMEA2000StdTypes.h"
// std::string getEngineStatus1(const tN2kDD206& status);

/**
 * @brief Set up NMEA2000
 * @returns True if succeeded
*/
bool NMEAsetup();

/**
 * @brief Run NMEA2000 decoding
*/
void NMEAloop();
