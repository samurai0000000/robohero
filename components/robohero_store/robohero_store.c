/*
 * robohero_store.c
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "robohero_store.h"

static const char *TAG = "store";
static const char *NVS_NS = "robohero";

static int8_t s_trims[STORE_PARAM_COUNT];
static uint8_t s_wifi_mode;
static uint8_t s_dhcp;
static uint8_t s_ap_channel;
static char s_sta_ssid[32];
static char s_sta_pass[64];
static char s_ap_ssid[32];
static char s_ap_pass[64];
static uint32_t s_static_ip;
static uint32_t s_static_mask;
static uint32_t s_static_gw;
static uint32_t s_static_dns;
static bool s_loaded;

static void apply_factory_ram(void)
{
    memset(s_trims, 0, sizeof(s_trims));
    s_wifi_mode = ROBOHERO_WIFI_STA;
    s_dhcp = 1;
    s_ap_channel = 1;
    memset(s_sta_ssid, 0, sizeof(s_sta_ssid));
    memset(s_sta_pass, 0, sizeof(s_sta_pass));
    memset(s_ap_ssid, 0, sizeof(s_ap_ssid));
    memset(s_ap_pass, 0, sizeof(s_ap_pass));
    strncpy(s_sta_ssid, CLIENT_SSID, sizeof(s_sta_ssid) - 1);
    strncpy(s_sta_pass, CLIENT_PASSWORD, sizeof(s_sta_pass) - 1);
    strncpy(s_ap_pass, AP_PASSWORD, sizeof(s_ap_pass) - 1);
    s_static_ip = 0;
    s_static_mask = 0;
    s_static_gw = 0;
    s_static_dns = 0;
}

static esp_err_t open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    return nvs_open(NVS_NS, mode, handle);
}

void store_init(void)
{
    apply_factory_ram();
    if (!store_load()) {
        ESP_LOGW(TAG, "NVS empty or invalid; writing factory defaults");
        store_factory_reset(true);
    }
}

bool store_load(void)
{
    nvs_handle_t h;
    esp_err_t err = open_nvs(&h, NVS_READONLY);
    if (err != ESP_OK) {
        s_loaded = false;
        return false;
    }

    size_t len = sizeof(s_trims);
    err = nvs_get_blob(h, "trims", s_trims, &len);
    if (err != ESP_OK || len != sizeof(s_trims)) {
        nvs_close(h);
        s_loaded = false;
        return false;
    }

    nvs_get_u8(h, "wifi_mode", &s_wifi_mode);
    nvs_get_u8(h, "dhcp", &s_dhcp);
    nvs_get_u8(h, "ap_ch", &s_ap_channel);

    size_t slen = sizeof(s_sta_ssid);
    if (nvs_get_str(h, "sta_ssid", s_sta_ssid, &slen) != ESP_OK) {
        s_sta_ssid[0] = '\0';
    }
    slen = sizeof(s_sta_pass);
    if (nvs_get_str(h, "sta_pass", s_sta_pass, &slen) != ESP_OK) {
        s_sta_pass[0] = '\0';
    }
    slen = sizeof(s_ap_ssid);
    if (nvs_get_str(h, "ap_ssid", s_ap_ssid, &slen) != ESP_OK) {
        s_ap_ssid[0] = '\0';
    }
    slen = sizeof(s_ap_pass);
    if (nvs_get_str(h, "ap_pass", s_ap_pass, &slen) != ESP_OK) {
        s_ap_pass[0] = '\0';
    }

    nvs_get_u32(h, "sip", &s_static_ip);
    nvs_get_u32(h, "smask", &s_static_mask);
    nvs_get_u32(h, "sgw", &s_static_gw);
    nvs_get_u32(h, "sdns", &s_static_dns);

    nvs_close(h);
    s_loaded = true;
    return true;
}

bool store_save(void)
{
    nvs_handle_t h;
    esp_err_t err = open_nvs(&h, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(h, "trims", s_trims, sizeof(s_trims));
    err |= nvs_set_u8(h, "wifi_mode", s_wifi_mode);
    err |= nvs_set_u8(h, "dhcp", s_dhcp);
    err |= nvs_set_u8(h, "ap_ch", s_ap_channel);
    err |= nvs_set_str(h, "sta_ssid", s_sta_ssid);
    err |= nvs_set_str(h, "sta_pass", s_sta_pass);
    err |= nvs_set_str(h, "ap_ssid", s_ap_ssid);
    err |= nvs_set_str(h, "ap_pass", s_ap_pass);
    err |= nvs_set_u32(h, "sip", s_static_ip);
    err |= nvs_set_u32(h, "smask", s_static_mask);
    err |= nvs_set_u32(h, "sgw", s_static_gw);
    err |= nvs_set_u32(h, "sdns", s_static_dns);
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

void store_factory_reset(bool auto_save)
{
    apply_factory_ram();
    if (auto_save) {
        store_save();
    }
}

bool store_reset_motion_trims(bool auto_save)
{
    for (int i = 0; i <= STORE_KEY_DELAY_TIME; i++) {
        s_trims[i] = 0;
    }
    return auto_save ? store_save() : true;
}

int8_t store_read_key(int key)
{
    if (key < 0 || key >= STORE_PARAM_COUNT) {
        return 0;
    }
    return s_trims[key];
}

bool store_write_key(int key, int8_t value, bool auto_save)
{
    if (key < 0 || key >= STORE_PARAM_COUNT) {
        return false;
    }
    s_trims[key] = value;
    return auto_save ? store_save() : true;
}

int8_t store_get_servo_trim(int servo_index)
{
    if (servo_index < 0 || servo_index > 16) {
        return 0;
    }
    return s_trims[servo_index];
}

int8_t store_get_delay_trim(void)
{
    return s_trims[STORE_KEY_DELAY_TIME];
}

int8_t store_get_matrix_trim(int index)
{
    if (index < 0 || index >= ALLMATRIX) {
        return 0;
    }
    return s_trims[index];
}

int8_t store_get_pwm_freq_trim(void)
{
    return s_trims[STORE_KEY_PWM_FREQ];
}

int8_t store_get_voltage_trim(void)
{
    return s_trims[STORE_KEY_VOLTAGE_CAL];
}

bool store_set_servo_trim(int servo_index, int8_t trim, bool auto_save)
{
    if (servo_index < 0 || servo_index > 16) {
        return false;
    }
    return store_write_key(servo_index, trim, auto_save);
}

bool store_set_delay_trim(int8_t trim, bool auto_save)
{
    return store_write_key(STORE_KEY_DELAY_TIME, trim, auto_save);
}

bool store_set_matrix_trim(int index, int8_t trim, bool auto_save)
{
    if (index < 0 || index >= ALLMATRIX) {
        return false;
    }
    return store_write_key(index, trim, auto_save);
}

bool store_set_pwm_freq_trim(int8_t trim, bool auto_save)
{
    return store_write_key(STORE_KEY_PWM_FREQ, trim, auto_save);
}

bool store_set_voltage_trim(int8_t trim, bool auto_save)
{
    return store_write_key(STORE_KEY_VOLTAGE_CAL, trim, auto_save);
}

uint8_t store_get_wifi_mode(void)
{
    return s_wifi_mode;
}

bool store_set_wifi_mode(uint8_t mode, bool auto_save)
{
    s_wifi_mode = mode;
    return auto_save ? store_save() : true;
}

const char *store_get_sta_ssid(void)
{
    return s_sta_ssid;
}

const char *store_get_sta_password(void)
{
    return s_sta_pass;
}

const char *store_get_ap_ssid(void)
{
    return s_ap_ssid;
}

const char *store_get_ap_password(void)
{
    return s_ap_pass;
}

uint8_t store_get_ap_channel(void)
{
    return (s_ap_channel >= 1 && s_ap_channel <= 14) ? s_ap_channel : 1;
}

bool store_set_sta_ssid(const char *ssid, bool auto_save)
{
    if (ssid == NULL) {
        return false;
    }
    strncpy(s_sta_ssid, ssid, sizeof(s_sta_ssid) - 1);
    s_sta_ssid[sizeof(s_sta_ssid) - 1] = '\0';
    return auto_save ? store_save() : true;
}

bool store_set_sta_password(const char *pass, bool auto_save)
{
    if (pass == NULL) {
        return false;
    }
    strncpy(s_sta_pass, pass, sizeof(s_sta_pass) - 1);
    s_sta_pass[sizeof(s_sta_pass) - 1] = '\0';
    return auto_save ? store_save() : true;
}

bool store_set_ap_ssid(const char *ssid, bool auto_save)
{
    if (ssid == NULL) {
        s_ap_ssid[0] = '\0';
    } else {
        strncpy(s_ap_ssid, ssid, sizeof(s_ap_ssid) - 1);
        s_ap_ssid[sizeof(s_ap_ssid) - 1] = '\0';
    }
    return auto_save ? store_save() : true;
}

bool store_set_ap_password(const char *pass, bool auto_save)
{
    if (pass == NULL) {
        return false;
    }
    strncpy(s_ap_pass, pass, sizeof(s_ap_pass) - 1);
    s_ap_pass[sizeof(s_ap_pass) - 1] = '\0';
    return auto_save ? store_save() : true;
}

bool store_set_ap_channel(uint8_t channel, bool auto_save)
{
    s_ap_channel = channel;
    return auto_save ? store_save() : true;
}

bool store_is_dhcp_enabled(void)
{
    return s_dhcp != 0;
}

bool store_set_dhcp_enabled(bool dhcp, bool auto_save)
{
    s_dhcp = dhcp ? 1 : 0;
    return auto_save ? store_save() : true;
}

uint32_t store_get_static_ip(void)
{
    return s_static_ip;
}

uint32_t store_get_static_netmask(void)
{
    return s_static_mask;
}

uint32_t store_get_static_gateway(void)
{
    return s_static_gw;
}

uint32_t store_get_static_dns(void)
{
    return s_static_dns;
}

bool store_set_static_ip(uint32_t ip, bool auto_save)
{
    s_static_ip = ip;
    return auto_save ? store_save() : true;
}

bool store_set_static_netmask(uint32_t mask, bool auto_save)
{
    s_static_mask = mask;
    return auto_save ? store_save() : true;
}

bool store_set_static_gateway(uint32_t gw, bool auto_save)
{
    s_static_gw = gw;
    return auto_save ? store_save() : true;
}

bool store_set_static_dns(uint32_t dns, bool auto_save)
{
    s_static_dns = dns;
    return auto_save ? store_save() : true;
}

const char *store_key_name(int key)
{
    static const char *const names[STORE_PARAM_COUNT] = {
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

    if (key >= 0 && key < STORE_PARAM_COUNT) {
        return names[key];
    }
    return "Reserved";
}

const char *store_wifi_mode_name(uint8_t mode)
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
