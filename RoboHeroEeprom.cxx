/*
 * RoboHeroEeprom.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <string.h>
#include "RoboHeroEeprom.hxx"

RoboHeroEeprom::RoboHeroEeprom()
    : _loaded(false)
{
    memset(_data, 0, sizeof(_data));
}

RoboHeroEeprom::~RoboHeroEeprom()
{
}

void RoboHeroEeprom::begin(size_t size)
{
    EEPROM.begin(size);
    load();

    if (!isValid()) {
        factoryReset(true);
    }
}

bool RoboHeroEeprom::load(void)
{
    for (size_t i = 0; i < ROBOHERO_EEPROM_SIZE; i++) {
        _data[i] = (int8_t) EEPROM.read(i);
    }

    _loaded = true;
    return true;
}

bool RoboHeroEeprom::save(void)
{
    for (size_t i = 0; i < ROBOHERO_EEPROM_SIZE; i++) {
        EEPROM.write(i, (uint8_t) _data[i]);
    }

    return EEPROM.commit();
}

bool RoboHeroEeprom::isValid(void) const
{
    uint32_t magic = readUint32(EEPROM_OFFSET_HEADER);
    uint8_t ver = (uint8_t) _data[EEPROM_OFFSET_HEADER + 4];

    return (magic == ROBOHERO_EEPROM_MAGIC) && (ver == ROBOHERO_EEPROM_VERSION);
}

void RoboHeroEeprom::factoryReset(bool autoSave)
{
    memset(_data, 0, sizeof(_data));

    // Write header
    writeUint32(EEPROM_OFFSET_HEADER, ROBOHERO_EEPROM_MAGIC);
    _data[EEPROM_OFFSET_HEADER + 4] = ROBOHERO_EEPROM_VERSION;
    _data[EEPROM_OFFSET_HEADER + 5] = (uint8_t) AP_MODE;
    _data[EEPROM_OFFSET_HEADER + 6] = 1;  // DHCP enabled

    // Write default Wi-Fi Station credentials
    strncpy((char *) &_data[EEPROM_OFFSET_STA_SSID], CLIENT_SSID, 31);
    _data[EEPROM_OFFSET_STA_SSID + 31] = '\0';

    strncpy((char *) &_data[EEPROM_OFFSET_STA_PASS], CLIENT_PASSWORD, 63);
    _data[EEPROM_OFFSET_STA_PASS + 63] = '\0';

    // Write default Wi-Fi AP credentials
    _data[EEPROM_OFFSET_AP_SSID] = '\0';

    strncpy((char *) &_data[EEPROM_OFFSET_AP_PASS], AP_PASSWORD, 63);
    _data[EEPROM_OFFSET_AP_PASS + 63] = '\0';

    // Default AP Channel = 1
    _data[EEPROM_OFFSET_NET_CONFIG + 16] = 1;

    if (autoSave) {
        save();
    }
}

int8_t RoboHeroEeprom::readKeyValue(int key) const
{
    if ((key >= 0) && (key < (int) ROBOHERO_EEPROM_SIZE)) {
        return _data[key];
    }

    return 0;
}

bool RoboHeroEeprom::writeKeyValue(int key, int8_t value, bool autoSave)
{
    if ((key >= 0) && (key < (int) ROBOHERO_EEPROM_SIZE)) {
        _data[key] = value;
        if (autoSave) {
            return save();
        }
        return true;
    }

    return false;
}

int8_t RoboHeroEeprom::getServoTrim(int servoIndex) const
{
    if ((servoIndex >= 0) && (servoIndex <= 16)) {
        return _data[servoIndex];
    }

    return 0;
}

bool RoboHeroEeprom::setServoTrim(int servoIndex, int8_t trim, bool autoSave)
{
    if ((servoIndex >= 0) && (servoIndex <= 16)) {
        return writeKeyValue(servoIndex, trim, autoSave);
    }

    return false;
}

int8_t RoboHeroEeprom::getDelayTrim(void) const
{
    return _data[EEPROM_KEY_DELAY_TIME];
}

bool RoboHeroEeprom::setDelayTrim(int8_t trim, bool autoSave)
{
    return writeKeyValue((int) EEPROM_KEY_DELAY_TIME, trim, autoSave);
}

int8_t RoboHeroEeprom::getMatrixTrim(int index) const
{
    if ((index >= 0) && (index < ALLMATRIX)) {
        return _data[index];
    }

    return 0;
}

bool RoboHeroEeprom::setMatrixTrim(int index, int8_t trim, bool autoSave)
{
    if ((index >= 0) && (index < ALLMATRIX)) {
        return writeKeyValue(index, trim, autoSave);
    }

    return false;
}

int8_t RoboHeroEeprom::getPwmFreqTrim(void) const
{
    return _data[EEPROM_KEY_PWM_FREQ];
}

bool RoboHeroEeprom::setPwmFreqTrim(int8_t trim, bool autoSave)
{
    return writeKeyValue((int) EEPROM_KEY_PWM_FREQ, trim, autoSave);
}

int8_t RoboHeroEeprom::getVoltageTrim(void) const
{
    return _data[EEPROM_KEY_VOLTAGE_CAL];
}

bool RoboHeroEeprom::setVoltageTrim(int8_t trim, bool autoSave)
{
    return writeKeyValue((int) EEPROM_KEY_VOLTAGE_CAL, trim, autoSave);
}

bool RoboHeroEeprom::resetMotionTrims(bool autoSave)
{
    for (int i = 0; i <= 17; i++) {
        _data[i] = 0;
    }

    if (autoSave) {
        return save();
    }

    return true;
}

bool RoboHeroEeprom::resetAll(bool autoSave)
{
    factoryReset(autoSave);
    return true;
}

uint8_t RoboHeroEeprom::getWifiMode(void) const
{
    return (uint8_t) _data[EEPROM_OFFSET_HEADER + 5];
}

bool RoboHeroEeprom::setWifiMode(uint8_t mode, bool autoSave)
{
    _data[EEPROM_OFFSET_HEADER + 5] = (int8_t) mode;
    if (autoSave) {
        return save();
    }
    return true;
}

const char *RoboHeroEeprom::getStaSSID(void) const
{
    return (const char *) &_data[EEPROM_OFFSET_STA_SSID];
}

bool RoboHeroEeprom::setStaSSID(const char *ssid, bool autoSave)
{
    if (ssid == NULL) {
        return false;
    }

    strncpy((char *) &_data[EEPROM_OFFSET_STA_SSID], ssid, 31);
    _data[EEPROM_OFFSET_STA_SSID + 31] = '\0';

    if (autoSave) {
        return save();
    }
    return true;
}

const char *RoboHeroEeprom::getStaPassword(void) const
{
    return (const char *) &_data[EEPROM_OFFSET_STA_PASS];
}

bool RoboHeroEeprom::setStaPassword(const char *pass, bool autoSave)
{
    if (pass == NULL) {
        return false;
    }

    strncpy((char *) &_data[EEPROM_OFFSET_STA_PASS], pass, 63);
    _data[EEPROM_OFFSET_STA_PASS + 63] = '\0';

    if (autoSave) {
        return save();
    }
    return true;
}

const char *RoboHeroEeprom::getApSSID(void) const
{
    return (const char *) &_data[EEPROM_OFFSET_AP_SSID];
}

bool RoboHeroEeprom::setApSSID(const char *ssid, bool autoSave)
{
    if (ssid == NULL) {
        _data[EEPROM_OFFSET_AP_SSID] = '\0';
    } else {
        strncpy((char *) &_data[EEPROM_OFFSET_AP_SSID], ssid, 31);
        _data[EEPROM_OFFSET_AP_SSID + 31] = '\0';
    }

    if (autoSave) {
        return save();
    }
    return true;
}

const char *RoboHeroEeprom::getApPassword(void) const
{
    return (const char *) &_data[EEPROM_OFFSET_AP_PASS];
}

bool RoboHeroEeprom::setApPassword(const char *pass, bool autoSave)
{
    if (pass == NULL) {
        return false;
    }

    strncpy((char *) &_data[EEPROM_OFFSET_AP_PASS], pass, 63);
    _data[EEPROM_OFFSET_AP_PASS + 63] = '\0';

    if (autoSave) {
        return save();
    }
    return true;
}

uint8_t RoboHeroEeprom::getApChannel(void) const
{
    uint8_t ch = (uint8_t) _data[EEPROM_OFFSET_NET_CONFIG + 16];
    return (ch >= 1 && ch <= 14) ? ch : 1;
}

bool RoboHeroEeprom::setApChannel(uint8_t channel, bool autoSave)
{
    _data[EEPROM_OFFSET_NET_CONFIG + 16] = (int8_t) channel;
    if (autoSave) {
        return save();
    }
    return true;
}

bool RoboHeroEeprom::isDhcpEnabled(void) const
{
    return _data[EEPROM_OFFSET_HEADER + 6] != 0;
}

bool RoboHeroEeprom::setDhcpEnabled(bool dhcp, bool autoSave)
{
    _data[EEPROM_OFFSET_HEADER + 6] = dhcp ? 1 : 0;
    if (autoSave) {
        return save();
    }
    return true;
}

uint32_t RoboHeroEeprom::getStaticIP(void) const
{
    return readUint32(EEPROM_OFFSET_NET_CONFIG);
}

bool RoboHeroEeprom::setStaticIP(uint32_t ip, bool autoSave)
{
    writeUint32(EEPROM_OFFSET_NET_CONFIG, ip);
    if (autoSave) {
        return save();
    }
    return true;
}

uint32_t RoboHeroEeprom::getStaticNetmask(void) const
{
    return readUint32(EEPROM_OFFSET_NET_CONFIG + 4);
}

bool RoboHeroEeprom::setStaticNetmask(uint32_t mask, bool autoSave)
{
    writeUint32(EEPROM_OFFSET_NET_CONFIG + 4, mask);
    if (autoSave) {
        return save();
    }
    return true;
}

uint32_t RoboHeroEeprom::getStaticGateway(void) const
{
    return readUint32(EEPROM_OFFSET_NET_CONFIG + 8);
}

bool RoboHeroEeprom::setStaticGateway(uint32_t gw, bool autoSave)
{
    writeUint32(EEPROM_OFFSET_NET_CONFIG + 8, gw);
    if (autoSave) {
        return save();
    }
    return true;
}

uint32_t RoboHeroEeprom::getStaticDNS(void) const
{
    return readUint32(EEPROM_OFFSET_NET_CONFIG + 12);
}

bool RoboHeroEeprom::setStaticDNS(uint32_t dns, bool autoSave)
{
    writeUint32(EEPROM_OFFSET_NET_CONFIG + 12, dns);
    if (autoSave) {
        return save();
    }
    return true;
}

uint32_t RoboHeroEeprom::readUint32(size_t offset) const
{
    if (offset + 4 > ROBOHERO_EEPROM_SIZE) {
        return 0;
    }

    uint32_t val = 0;
    val |= ((uint32_t) (uint8_t) _data[offset]);
    val |= ((uint32_t) (uint8_t) _data[offset + 1]) << 8;
    val |= ((uint32_t) (uint8_t) _data[offset + 2]) << 16;
    val |= ((uint32_t) (uint8_t) _data[offset + 3]) << 24;

    return val;
}

void RoboHeroEeprom::writeUint32(size_t offset, uint32_t value)
{
    if (offset + 4 > ROBOHERO_EEPROM_SIZE) {
        return;
    }

    _data[offset]     = (int8_t) (value & 0xff);
    _data[offset + 1] = (int8_t) ((value >> 8) & 0xff);
    _data[offset + 2] = (int8_t) ((value >> 16) & 0xff);
    _data[offset + 3] = (int8_t) ((value >> 24) & 0xff);
}

const char *RoboHeroEeprom::getKeyName(int8_t key)
{
    static const char *const keyNames[ROBOHERO_EEPROM_PARAM_COUNT] = {
        "Servo 0 Trim",
        "Servo 1 Trim",
        "Servo 2 Trim",
        "Servo 3 Trim",
        "Servo 4 Trim",
        "Servo 5 Trim",
        "Servo 6 Trim",
        "Servo 7 Trim",
        "Servo 8 Trim",
        "Servo 9 Trim",
        "Servo 10 Trim",
        "Servo 11 Trim",
        "Servo 12 Trim",
        "Servo 13 Trim",
        "Servo 14 Trim",
        "Servo 15 Trim",
        "GPIO12 Servo Trim",
        "Delay Time Offset",
        "PWM Frequency Offset",
        "Voltage Cal Offset",
    };

    if ((key >= 0) && (key < ROBOHERO_EEPROM_PARAM_COUNT)) {
        return keyNames[key];
    }

    return "Reserved";
}

const char *RoboHeroEeprom::getWifiModeName(uint8_t mode)
{
    switch (mode) {
    case ROBOHERO_WIFI_STA:
        return "Station (STA)";
    case ROBOHERO_WIFI_AP:
        return "Access Point (AP)";
    case ROBOHERO_WIFI_AP_STA:
        return "AP + Station";
    case ROBOHERO_WIFI_OFF:
        return "OFF";
    default:
        break;
    }

    return "Unknown";
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
