/**
 * NMEATrax
 * 
 * @authors Alex Klouda
 * 
 * NMEATrax email header file.
 */

#include <Arduino.h>
#include "secrets.h"

//---Contents of secrets.h---
/** 
 * #define AUTHOR_EMAIL ***
 * #define AUTHOR_PASSWORD ***
 * #define RECIPIENT_EMAIL ***
 * #define SENDER_NAME ***
 * #define RECIPIENT_NAME ***
 */

#define SMTP_HOST "smtp.gmail.com"

/** The smtp port e.g. 
 * 25  or esp_mail_smtp_port_25
 * 465 or esp_mail_smtp_port_465
 * 587 or esp_mail_smtp_port_587
*/
#define SMTP_PORT 465

#define ENABLE_SMTP  // Allows SMTP class and data
// #define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#define ENABLE_FS // Allow filesystem integration

/**
 * @brief Sets the wifi mode as it should be and deletes the task
 */
void quitAndDelete();

/**
 * @brief Main function to send an email
 */
void sendEmail(void *pvParameters);

/**
 * @brief Connects to the WiFi network based on stored credentials
 * 
 * This function scans for available WiFi networks, matches them with stored credentials,
 * and connects to the chosen network. If no matching network is found or connection fails,
 * it sends an email notification and deletes the task.
 */
void connectToWifi();

/**
 * @brief Send email progress
 * @param text The progress message to send
*/
void sendEmailData(String text);
