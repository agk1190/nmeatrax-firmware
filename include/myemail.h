/**
 * NMEATrax
 * 
 * @authors Alex Klouda
 * 
 * NMEATrax email header file.
 */

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

void sendEmail(void *pvParameters);