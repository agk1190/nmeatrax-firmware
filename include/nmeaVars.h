#ifndef NMEAVARS_H
#define NMEAVARS_H

#include <Arduino.h>

#define LED_PWR GPIO_NUM_32
#define LED_N2K GPIO_NUM_25
#define LED_SD GPIO_NUM_14
#define SD_Detect GPIO_NUM_34
#define N2K_STBY GPIO_NUM_4

#define FW_VERSION "12.0.0"
// #define UI_VERSION1 "3.1.0"

extern TaskHandle_t webTaskHandle;
extern TaskHandle_t loggingTaskHandle;
extern TaskHandle_t bgTaskHandle;
extern TaskHandle_t nmeaTaskHandle;

#endif // NMEAVARS_H