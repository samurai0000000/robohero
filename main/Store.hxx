/*
 * Store.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_STORE_HXX
#define ROBOHERO_STORE_HXX

#include <stdbool.h>
#include <stdint.h>

#include "Config.hxx"

class Store
{
  public:
    static Store &instance();

    void init();
    bool load();
    bool save();
    void factoryReset(bool autoSave);
    bool resetMotionTrims(bool autoSave);

    int8_t readKey(int key);
    bool writeKey(int key, int8_t value, bool autoSave);

    int8_t getServoTrim(int servoIndex);
    int8_t getDelayTrim();
    int8_t getMatrixTrim(int index);
    int8_t getPwmFreqTrim();
    int8_t getVoltageTrim();

    bool setServoTrim(int servoIndex, int8_t trim, bool autoSave);
    bool setDelayTrim(int8_t trim, bool autoSave);
    bool setMatrixTrim(int index, int8_t trim, bool autoSave);
    bool setPwmFreqTrim(int8_t trim, bool autoSave);
    bool setVoltageTrim(int8_t trim, bool autoSave);

    uint8_t getWifiMode();
    bool setWifiMode(uint8_t mode, bool autoSave);

    const char *getStaSsid();
    const char *getStaPassword();
    const char *getApSsid();
    const char *getApPassword();
    uint8_t getApChannel();

    bool setStaSsid(const char *ssid, bool autoSave);
    bool setStaPassword(const char *pass, bool autoSave);
    bool setApSsid(const char *ssid, bool autoSave);
    bool setApPassword(const char *pass, bool autoSave);
    bool setApChannel(uint8_t channel, bool autoSave);

    bool isDhcpEnabled();
    bool setDhcpEnabled(bool dhcp, bool autoSave);

    uint32_t getStaticIp();
    uint32_t getStaticNetmask();
    uint32_t getStaticGateway();
    uint32_t getStaticDns();

    bool setStaticIp(uint32_t ip, bool autoSave);
    bool setStaticNetmask(uint32_t mask, bool autoSave);
    bool setStaticGateway(uint32_t gw, bool autoSave);
    bool setStaticDns(uint32_t dns, bool autoSave);

    bool mqttEnabled();
    bool setMqttEnabled(bool enabled, bool autoSave);
    const char *getMqttHost();
    uint16_t getMqttPort();
    const char *getMqttUser();
    const char *getMqttPass();
    const char *getMqttClientId();

    bool setMqttHost(const char *host, bool autoSave);
    bool setMqttPort(uint16_t port, bool autoSave);
    bool setMqttUser(const char *user, bool autoSave);
    bool setMqttPass(const char *pass, bool autoSave);
    bool setMqttClientId(const char *cid, bool autoSave);

    const char *keyName(int key);
    const char *wifiModeName(uint8_t mode);

  private:
    Store();
    Store(const Store &);
    Store &operator=(const Store &);
    static Store _self;

    void applyFactoryRam();

    int8_t _trims[STORE_PARAM_COUNT];
    uint8_t _wifiMode;
    uint8_t _dhcp;
    uint8_t _apChannel;
    char _staSsid[32];
    char _staPass[64];
    char _apSsid[32];
    char _apPass[64];
    uint32_t _staticIp;
    uint32_t _staticMask;
    uint32_t _staticGw;
    uint32_t _staticDns;
    uint8_t _mqttEn;
    uint16_t _mqttPort;
    char _mqttHost[64];
    char _mqttUser[32];
    char _mqttPass[64];
    char _mqttCid[32];
    bool _loaded;
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
