/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax NMEA 0183 decoding header file.
 */

extern double n183lat;
extern double n183lon;
extern String n183time;
extern String n183date;
extern String n183timeString;
extern double n183speed;
extern int n183heading;
extern double n183wtemp;
extern double n183depth;
extern double n183mag_var;

/**
 * @brief Set up NMEA0183
 * @returns True if succeeded
*/
bool setupN183();

/**
 * Run NMEA0183 decoding
*/
void loopN183();