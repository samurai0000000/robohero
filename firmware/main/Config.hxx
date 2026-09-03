/*
 * Config.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_CONFIG_HXX
#define ROBOHERO_CONFIG_HXX

#include <stddef.h>
#include <stdint.h>

/*
 * Undo ISO-2022 / attribute mess left by ESP8266 ROM boot prints
 * (often at 74880 baud) before the app speaks 115200.
 * ESC c = RIS, SI + G0/G1 ASCII, SGR reset.
 */
/* Adjacent "\x1b" "c" so the hex escape does not swallow 'c'. */
/* clang-format off */
#define TERM_RESET_SEQ "\r\n\x1b" "c\x0f\x1b(B\x1b)B\x1b[0m"
/* clang-format on */

#ifndef AP_PASSWORD
#define AP_PASSWORD "12345678"
#endif
#ifndef CLIENT_SSID
#define CLIENT_SSID "undefined"
#endif
#ifndef CLIENT_PASSWORD
#define CLIENT_PASSWORD "undefined"
#endif

#define LED_PIN      13
#define GPIO12_PIN   12
#define I2C_SDA_PIN  4
#define I2C_SCL_PIN  5
#define PCA9685_ADDR 0x40

#define ALLMATRIX 18
#define ALLSERVOS 17

#define PWM_FREQUENCY 54
#define PWMRES_MIN    1
#define PWMRES_MAX    270
#define SERVOMIN      104
#define SERVOMAX      512

#define BASEDELAYTIME 10

#define INPUT_VOLTAGE         785
#define INPUT_MIN_VOLTAGE     590
#define INPUT_RECOVER_VOLTAGE 620

#define FRAME_BUFFER_MAX 50

#define HEAD_PWM_PERIOD_US 20000
#define HEAD_SERVO_MIN_US  544
#define HEAD_SERVO_MAX_US  2400

#define STORE_PARAM_COUNT 20

#define STORE_KEY_SERVO_GPIO12 16
#define STORE_KEY_DELAY_TIME   17
#define STORE_KEY_PWM_FREQ     18
#define STORE_KEY_VOLTAGE_CAL  19

enum RoboHeroWifiMode {
    ROBOHERO_WIFI_STA = 0,
    ROBOHERO_WIFI_AP = 1,
    ROBOHERO_WIFI_AP_STA = 2,
    ROBOHERO_WIFI_OFF = 3,
};

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
