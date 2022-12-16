/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax NMEA 2000 decoding header file.
 */

extern int rpm;
extern double n2kdepth;
extern double n2kspeed;
extern int n2kheading;
extern int etemp;
extern int otemp;
extern double n2kwtemp;
extern double n2klat;
extern double n2klon;
extern double n2krapidLat;
extern double n2krapidLon;
extern double n2kmag_var;
extern int leg_tilt;
extern int opres;
extern double battV;
extern double fuel_rate;
extern int ehours;
extern int fpres;
extern String gear;
extern double flevel;
extern String n2ktimeString;

/**
 * @brief Set up NMEA2000
 * @returns True if succeeded
*/
bool NMEAsetup();

/**
 * @brief Run NMEA2000 decoding
*/
void NMEAloop();
