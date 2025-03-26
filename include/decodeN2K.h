/**
 * NMEATrax
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * NMEATrax NMEA 2000 decoding header file.
 */

/**
 * @brief Set up NMEA2000
 * @returns True if succeeded
*/
bool NMEAsetup();

/**
 * @brief Run NMEA2000 decoding
*/
void NMEAloop();
