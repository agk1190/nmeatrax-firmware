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
extern int etemp;
extern int otemp;
extern double wtemp;
extern double lat;
extern double lon;
extern double rapidLat;
extern double rapidLon;
extern double mag_var;
extern int leg_tilt;
extern int opres;
extern double battV;
extern double fuel_rate;
extern int ehours;
extern String gear;
extern double flevel;
extern String timeString;

/**
 * @brief Set up NMEA2000
 * @returns True if succeeded
*/
bool NMEAsetup();

/**
 * @brief Run NMEA2000 decoding
*/
void NMEAloop();
