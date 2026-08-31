/*
 * RoboHeroConfig.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_CONFIG_HXX
#define ROBOHERO_CONFIG_HXX

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

#undef min
#undef max

#define FW_VERSION_STRING "RoboHero Firmware v4.0 (Charles Chiou) " \
    __DATE__ " " __TIME__

#define AP_MODE 0
#ifndef AP_PASSWORD
#define AP_PASSWORD "12345678"
#endif
#ifndef CLIENT_SSID
#define CLIENT_SSID "undefined"
#endif
#ifndef CLIENT_PASSWORD
#define CLIENT_PASSWORD "undefined"
#endif

// LED Pin
static const int LedPin = 13;

// GPIO 12 Servo Pin
static const int GPIO12Pin = 12;

/*
 * Servos Matrix
 *
 * 0-15: PWM Servos
 * 16:   GPIO12 Servo
 * 17:   Delay Time
 * 18:   PWM Frequency Calibration + Voltage Calibration
 */
static const int ALLMATRIX = 18;

/*
 * 0-15: PWM Servos
 * 16:   GPIO12 Servo
 */
static const int ALLSERVOS = 17;

// ES08MDII Pulse Travelling 270 degrees
static const int PWM_Frequency = 54;   // PWM frequency 50Hz
static const int PWMRES_Min = 1;       // PWM Resolution 1
static const int PWMRES_Max = 270;     // PWM Resolution 270
static const int SERVOMIN = 104;
static const int SERVOMAX = 512;

// Servo Delay Base Time
static const int BASEDELAYTIME = 10;   // 10ms

// Voltage Detection
static const int Input_Voltage = 785;        // Approximately 6.00V
static const int Input_MinVoltage = 590;     // Low voltage protection ~5.90V
static const int Input_RecoverVoltage = 620; // Clear latch ~6.20V (hysteresis)

// Frame Buffer
#define FRAME_BUFFER_MAX 50

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
