/*
 * Store.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "Store.hxx"

static const char *TAG = "store";
static const char *NVS_NS = "robohero";

static esp_err_t openNvs(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    return nvs_open(NVS_NS, mode, handle);
}

Store Store::_self;

Store &Store::instance()
{
    return _self;
}

Store::Store()
    : _wifiMode(ROBOHERO_WIFI_STA), _dhcp(1), _apChannel(1), _staticIp(0),
      _staticMask(0), _staticGw(0), _staticDns(0), _mqttEn(0), _mqttPort(1883),
      _loaded(false)
{
    applyFactoryRam();
}

void Store::applyFactoryRam()
{
    memset(_trims, 0, sizeof(_trims));
    _wifiMode = ROBOHERO_WIFI_STA;
    _dhcp = 1;
    _apChannel = 1;
    memset(_staSsid, 0, sizeof(_staSsid));
    memset(_staPass, 0, sizeof(_staPass));
    memset(_apSsid, 0, sizeof(_apSsid));
    memset(_apPass, 0, sizeof(_apPass));
    strncpy(_staSsid, CLIENT_SSID, sizeof(_staSsid) - 1);
    strncpy(_staPass, CLIENT_PASSWORD, sizeof(_staPass) - 1);
    strncpy(_apPass, AP_PASSWORD, sizeof(_apPass) - 1);
    _staticIp = 0;
    _staticMask = 0;
    _staticGw = 0;
    _staticDns = 0;
    _mqttEn = 0;
    _mqttPort = 1883;
    memset(_mqttHost, 0, sizeof(_mqttHost));
    memset(_mqttUser, 0, sizeof(_mqttUser));
    memset(_mqttPass, 0, sizeof(_mqttPass));
    memset(_mqttCid, 0, sizeof(_mqttCid));
}

void Store::init()
{
    applyFactoryRam();
    if (!load()) {
        ESP_LOGW(TAG, "NVS empty or invalid; writing factory defaults");
        factoryReset(true);
    }
}

bool Store::load()
{
    nvs_handle_t h;
    esp_err_t err = openNvs(&h, NVS_READONLY);
    if (err != ESP_OK) {
        _loaded = false;
        return false;
    }

    size_t len = sizeof(_trims);
    err = nvs_get_blob(h, "trims", _trims, &len);
    if (err != ESP_OK || len != sizeof(_trims)) {
        nvs_close(h);
        _loaded = false;
        return false;
    }

    nvs_get_u8(h, "wifi_mode", &_wifiMode);
    nvs_get_u8(h, "dhcp", &_dhcp);
    nvs_get_u8(h, "ap_ch", &_apChannel);

    size_t slen = sizeof(_staSsid);
    if (nvs_get_str(h, "sta_ssid", _staSsid, &slen) != ESP_OK) {
        _staSsid[0] = '\0';
    }
    slen = sizeof(_staPass);
    if (nvs_get_str(h, "sta_pass", _staPass, &slen) != ESP_OK) {
        _staPass[0] = '\0';
    }
    slen = sizeof(_apSsid);
    if (nvs_get_str(h, "ap_ssid", _apSsid, &slen) != ESP_OK) {
        _apSsid[0] = '\0';
    }
    slen = sizeof(_apPass);
    if (nvs_get_str(h, "ap_pass", _apPass, &slen) != ESP_OK) {
        _apPass[0] = '\0';
    }

    nvs_get_u32(h, "sip", &_staticIp);
    nvs_get_u32(h, "smask", &_staticMask);
    nvs_get_u32(h, "sgw", &_staticGw);
    nvs_get_u32(h, "sdns", &_staticDns);

    nvs_get_u8(h, "mqtt_en", &_mqttEn);
    nvs_get_u16(h, "mqtt_port", &_mqttPort);
    if (_mqttPort == 0) {
        _mqttPort = 1883;
    }

    slen = sizeof(_mqttHost);
    if (nvs_get_str(h, "mqtt_host", _mqttHost, &slen) != ESP_OK) {
        _mqttHost[0] = '\0';
    }
    slen = sizeof(_mqttUser);
    if (nvs_get_str(h, "mqtt_user", _mqttUser, &slen) != ESP_OK) {
        _mqttUser[0] = '\0';
    }
    slen = sizeof(_mqttPass);
    if (nvs_get_str(h, "mqtt_pass", _mqttPass, &slen) != ESP_OK) {
        _mqttPass[0] = '\0';
    }
    slen = sizeof(_mqttCid);
    if (nvs_get_str(h, "mqtt_cid", _mqttCid, &slen) != ESP_OK) {
        _mqttCid[0] = '\0';
    }

    nvs_close(h);
    _loaded = true;
    return true;
}

bool Store::save()
{
    nvs_handle_t h;
    esp_err_t err = openNvs(&h, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(h, "trims", _trims, sizeof(_trims));
    err |= nvs_set_u8(h, "wifi_mode", _wifiMode);
    err |= nvs_set_u8(h, "dhcp", _dhcp);
    err |= nvs_set_u8(h, "ap_ch", _apChannel);
    err |= nvs_set_str(h, "sta_ssid", _staSsid);
    err |= nvs_set_str(h, "sta_pass", _staPass);
    err |= nvs_set_str(h, "ap_ssid", _apSsid);
    err |= nvs_set_str(h, "ap_pass", _apPass);
    err |= nvs_set_u32(h, "sip", _staticIp);
    err |= nvs_set_u32(h, "smask", _staticMask);
    err |= nvs_set_u32(h, "sgw", _staticGw);
    err |= nvs_set_u32(h, "sdns", _staticDns);
    err |= nvs_set_u8(h, "mqtt_en", _mqttEn);
    err |= nvs_set_u16(h, "mqtt_port", _mqttPort);
    err |= nvs_set_str(h, "mqtt_host", _mqttHost);
    err |= nvs_set_str(h, "mqtt_user", _mqttUser);
    err |= nvs_set_str(h, "mqtt_pass", _mqttPass);
    err |= nvs_set_str(h, "mqtt_cid", _mqttCid);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void Store::factoryReset(bool autoSave)
{
    applyFactoryRam();
    if (autoSave) {
        save();
    }
}

bool Store::resetMotionTrims(bool autoSave)
{
    for (int i = 0; i <= STORE_KEY_DELAY_TIME; i++) {
        _trims[i] = 0;
    }
    return autoSave ? save() : true;
}

int8_t Store::readKey(int key)
{
    if (key < 0 || key >= STORE_PARAM_COUNT) {
        return 0;
    }
    return _trims[key];
}

bool Store::writeKey(int key, int8_t value, bool autoSave)
{
    if (key < 0 || key >= STORE_PARAM_COUNT) {
        return false;
    }
    _trims[key] = value;
    return autoSave ? save() : true;
}

int8_t Store::getServoTrim(int servoIndex)
{
    if (servoIndex < 0 || servoIndex > 16) {
        return 0;
    }
    return _trims[servoIndex];
}

int8_t Store::getDelayTrim()
{
    return _trims[STORE_KEY_DELAY_TIME];
}

int8_t Store::getMatrixTrim(int index)
{
    if (index < 0 || index >= ALLMATRIX) {
        return 0;
    }
    return _trims[index];
}

int8_t Store::getPwmFreqTrim()
{
    return _trims[STORE_KEY_PWM_FREQ];
}

int8_t Store::getVoltageTrim()
{
    return _trims[STORE_KEY_VOLTAGE_CAL];
}

bool Store::setServoTrim(int servoIndex, int8_t trim, bool autoSave)
{
    if (servoIndex < 0 || servoIndex > 16) {
        return false;
    }
    return writeKey(servoIndex, trim, autoSave);
}

bool Store::setDelayTrim(int8_t trim, bool autoSave)
{
    return writeKey(STORE_KEY_DELAY_TIME, trim, autoSave);
}

bool Store::setMatrixTrim(int index, int8_t trim, bool autoSave)
{
    if (index < 0 || index >= ALLMATRIX) {
        return false;
    }
    return writeKey(index, trim, autoSave);
}

bool Store::setPwmFreqTrim(int8_t trim, bool autoSave)
{
    return writeKey(STORE_KEY_PWM_FREQ, trim, autoSave);
}

bool Store::setVoltageTrim(int8_t trim, bool autoSave)
{
    return writeKey(STORE_KEY_VOLTAGE_CAL, trim, autoSave);
}

uint8_t Store::getWifiMode()
{
    return _wifiMode;
}

bool Store::setWifiMode(uint8_t mode, bool autoSave)
{
    _wifiMode = mode;
    return autoSave ? save() : true;
}

const char *Store::getStaSsid()
{
    return _staSsid;
}

const char *Store::getStaPassword()
{
    return _staPass;
}

const char *Store::getApSsid()
{
    return _apSsid;
}

const char *Store::getApPassword()
{
    return _apPass;
}

uint8_t Store::getApChannel()
{
    return (_apChannel >= 1 && _apChannel <= 14) ? _apChannel : 1;
}

bool Store::setStaSsid(const char *ssid, bool autoSave)
{
    if (ssid == NULL) {
        return false;
    }
    strncpy(_staSsid, ssid, sizeof(_staSsid) - 1);
    _staSsid[sizeof(_staSsid) - 1] = '\0';
    return autoSave ? save() : true;
}

bool Store::setStaPassword(const char *pass, bool autoSave)
{
    if (pass == NULL) {
        return false;
    }
    strncpy(_staPass, pass, sizeof(_staPass) - 1);
    _staPass[sizeof(_staPass) - 1] = '\0';
    return autoSave ? save() : true;
}

bool Store::setApSsid(const char *ssid, bool autoSave)
{
    if (ssid == NULL) {
        _apSsid[0] = '\0';
    } else {
        strncpy(_apSsid, ssid, sizeof(_apSsid) - 1);
        _apSsid[sizeof(_apSsid) - 1] = '\0';
    }
    return autoSave ? save() : true;
}

bool Store::setApPassword(const char *pass, bool autoSave)
{
    if (pass == NULL) {
        return false;
    }
    strncpy(_apPass, pass, sizeof(_apPass) - 1);
    _apPass[sizeof(_apPass) - 1] = '\0';
    return autoSave ? save() : true;
}

bool Store::setApChannel(uint8_t channel, bool autoSave)
{
    _apChannel = channel;
    return autoSave ? save() : true;
}

bool Store::isDhcpEnabled()
{
    return _dhcp != 0;
}

bool Store::setDhcpEnabled(bool dhcp, bool autoSave)
{
    _dhcp = dhcp ? 1 : 0;
    return autoSave ? save() : true;
}

uint32_t Store::getStaticIp()
{
    return _staticIp;
}

uint32_t Store::getStaticNetmask()
{
    return _staticMask;
}

uint32_t Store::getStaticGateway()
{
    return _staticGw;
}

uint32_t Store::getStaticDns()
{
    return _staticDns;
}

bool Store::setStaticIp(uint32_t ip, bool autoSave)
{
    _staticIp = ip;
    return autoSave ? save() : true;
}

bool Store::setStaticNetmask(uint32_t mask, bool autoSave)
{
    _staticMask = mask;
    return autoSave ? save() : true;
}

bool Store::setStaticGateway(uint32_t gw, bool autoSave)
{
    _staticGw = gw;
    return autoSave ? save() : true;
}

bool Store::setStaticDns(uint32_t dns, bool autoSave)
{
    _staticDns = dns;
    return autoSave ? save() : true;
}

bool Store::mqttEnabled()
{
    return _mqttEn != 0;
}

bool Store::setMqttEnabled(bool enabled, bool autoSave)
{
    _mqttEn = enabled ? 1 : 0;
    return autoSave ? save() : true;
}

const char *Store::getMqttHost()
{
    return _mqttHost;
}

uint16_t Store::getMqttPort()
{
    return _mqttPort ? _mqttPort : 1883;
}

const char *Store::getMqttUser()
{
    return _mqttUser;
}

const char *Store::getMqttPass()
{
    return _mqttPass;
}

const char *Store::getMqttClientId()
{
    return _mqttCid;
}

bool Store::setMqttHost(const char *host, bool autoSave)
{
    if (host == NULL) {
        _mqttHost[0] = '\0';
    } else {
        strncpy(_mqttHost, host, sizeof(_mqttHost) - 1);
        _mqttHost[sizeof(_mqttHost) - 1] = '\0';
    }
    return autoSave ? save() : true;
}

bool Store::setMqttPort(uint16_t port, bool autoSave)
{
    _mqttPort = port ? port : 1883;
    return autoSave ? save() : true;
}

bool Store::setMqttUser(const char *user, bool autoSave)
{
    if (user == NULL) {
        _mqttUser[0] = '\0';
    } else {
        strncpy(_mqttUser, user, sizeof(_mqttUser) - 1);
        _mqttUser[sizeof(_mqttUser) - 1] = '\0';
    }
    return autoSave ? save() : true;
}

bool Store::setMqttPass(const char *pass, bool autoSave)
{
    if (pass == NULL) {
        _mqttPass[0] = '\0';
    } else {
        strncpy(_mqttPass, pass, sizeof(_mqttPass) - 1);
        _mqttPass[sizeof(_mqttPass) - 1] = '\0';
    }
    return autoSave ? save() : true;
}

bool Store::setMqttClientId(const char *cid, bool autoSave)
{
    if (cid == NULL) {
        _mqttCid[0] = '\0';
    } else {
        strncpy(_mqttCid, cid, sizeof(_mqttCid) - 1);
        _mqttCid[sizeof(_mqttCid) - 1] = '\0';
    }
    return autoSave ? save() : true;
}

const char *Store::keyName(int key)
{
    static const char *const names[STORE_PARAM_COUNT] = {
        /* clang-format off */
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
        /* clang-format on */
    };

    if (key >= 0 && key < STORE_PARAM_COUNT) {
        return names[key];
    }
    return "Reserved";
}

const char *Store::wifiModeName(uint8_t mode)
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
