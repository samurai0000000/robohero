/*
 * RoboHeroEeprom.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_EEPROM_HXX
#define ROBOHERO_EEPROM_HXX

#include <stdint.h>
#include <stddef.h>
#include "RoboHeroConfig.hxx"
#include <EEPROM.h>

#define ROBOHERO_EEPROM_SIZE 256
#define ROBOHERO_EEPROM_PARAM_COUNT 20
#define ROBOHERO_EEPROM_MAGIC 0x524f4248   // "ROBH"
#define ROBOHERO_EEPROM_VERSION 1

#define EEPROM_OFFSET_SERVO_TRIMS   0    // 0..15: 16 bytes
#define EEPROM_OFFSET_SERVO_GPIO12 16    // 1 byte
#define EEPROM_OFFSET_DELAY_TIME   17    // 1 byte
#define EEPROM_OFFSET_PWM_FREQ     18    // 1 byte
#define EEPROM_OFFSET_VOLTAGE_CAL  19    // 1 byte

#define EEPROM_OFFSET_HEADER       24    // 8 bytes
#define EEPROM_OFFSET_STA_SSID     32    // 32 bytes
#define EEPROM_OFFSET_STA_PASS     64    // 64 bytes
#define EEPROM_OFFSET_AP_SSID     128    // 32 bytes
#define EEPROM_OFFSET_AP_PASS     160    // 64 bytes
#define EEPROM_OFFSET_NET_CONFIG  224    // 32 bytes

enum RoboHeroEepromKey {
    EEPROM_KEY_SERVO_0      = 0,
    EEPROM_KEY_SERVO_1      = 1,
    EEPROM_KEY_SERVO_2      = 2,
    EEPROM_KEY_SERVO_3      = 3,
    EEPROM_KEY_SERVO_4      = 4,
    EEPROM_KEY_SERVO_5      = 5,
    EEPROM_KEY_SERVO_6      = 6,
    EEPROM_KEY_SERVO_7      = 7,
    EEPROM_KEY_SERVO_8      = 8,
    EEPROM_KEY_SERVO_9      = 9,
    EEPROM_KEY_SERVO_10     = 10,
    EEPROM_KEY_SERVO_11     = 11,
    EEPROM_KEY_SERVO_12     = 12,
    EEPROM_KEY_SERVO_13     = 13,
    EEPROM_KEY_SERVO_14     = 14,
    EEPROM_KEY_SERVO_15     = 15,
    EEPROM_KEY_SERVO_GPIO12 = 16,
    EEPROM_KEY_DELAY_TIME   = 17,
    EEPROM_KEY_PWM_FREQ     = 18,
    EEPROM_KEY_VOLTAGE_CAL  = 19,
};

enum RoboHeroWifiMode {
    ROBOHERO_WIFI_STA     = 0,
    ROBOHERO_WIFI_AP      = 1,
    ROBOHERO_WIFI_AP_STA  = 2,
    ROBOHERO_WIFI_OFF     = 3,
};

class RoboHeroEeprom {

public:

    RoboHeroEeprom();
    ~RoboHeroEeprom();

    void begin(size_t size = ROBOHERO_EEPROM_SIZE);
    bool load(void);
    bool save(void);

    bool isValid(void) const;
    void factoryReset(bool autoSave = true);

    // Legacy calibration and servo trim accessors (0..19)
    int8_t readKeyValue(int8_t key) const;
    bool writeKeyValue(int8_t key, int8_t value, bool autoSave = true);

    int8_t getServoTrim(int servoIndex) const;
    bool setServoTrim(int servoIndex, int8_t trim, bool autoSave = true);

    int8_t getDelayTrim(void) const;
    bool setDelayTrim(int8_t trim, bool autoSave = true);

    int8_t getMatrixTrim(int index) const;
    bool setMatrixTrim(int index, int8_t trim, bool autoSave = true);

    int8_t getPwmFreqTrim(void) const;
    bool setPwmFreqTrim(int8_t trim, bool autoSave = true);

    int8_t getVoltageTrim(void) const;
    bool setVoltageTrim(int8_t trim, bool autoSave = true);

    bool resetMotionTrims(bool autoSave = true);
    bool resetAll(bool autoSave = true);

    // Wi-Fi and Network parameter accessors
    uint8_t getWifiMode(void) const;
    bool setWifiMode(uint8_t mode, bool autoSave = true);

    const char *getStaSSID(void) const;
    bool setStaSSID(const char *ssid, bool autoSave = true);

    const char *getStaPassword(void) const;
    bool setStaPassword(const char *pass, bool autoSave = true);

    const char *getApSSID(void) const;
    bool setApSSID(const char *ssid, bool autoSave = true);

    const char *getApPassword(void) const;
    bool setApPassword(const char *pass, bool autoSave = true);

    uint8_t getApChannel(void) const;
    bool setApChannel(uint8_t channel, bool autoSave = true);

    bool isDhcpEnabled(void) const;
    bool setDhcpEnabled(bool dhcp, bool autoSave = true);

    uint32_t getStaticIP(void) const;
    bool setStaticIP(uint32_t ip, bool autoSave = true);

    uint32_t getStaticNetmask(void) const;
    bool setStaticNetmask(uint32_t mask, bool autoSave = true);

    uint32_t getStaticGateway(void) const;
    bool setStaticGateway(uint32_t gw, bool autoSave = true);

    uint32_t getStaticDNS(void) const;
    bool setStaticDNS(uint32_t dns, bool autoSave = true);

    static const char *getKeyName(int8_t key);
    static const char *getWifiModeName(uint8_t mode);

    inline const int8_t *getData(void) const {
        return _data;
    }

    inline size_t getSize(void) const {
        return ROBOHERO_EEPROM_SIZE;
    }

    inline size_t getParamCount(void) const {
        return ROBOHERO_EEPROM_PARAM_COUNT;
    }

    inline bool isLoaded(void) const {
        return _loaded;
    }

private:

    int8_t _data[ROBOHERO_EEPROM_SIZE];
    bool _loaded;

    uint32_t readUint32(size_t offset) const;
    void writeUint32(size_t offset, uint32_t value);

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
