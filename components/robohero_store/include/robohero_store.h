/*
 * robohero_store.h
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_STORE_H
#define ROBOHERO_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "robohero_config.h"

void store_init(void);
bool store_load(void);
bool store_save(void);
void store_factory_reset(bool auto_save);
bool store_reset_motion_trims(bool auto_save);

int8_t store_read_key(int key);
bool store_write_key(int key, int8_t value, bool auto_save);

int8_t store_get_servo_trim(int servo_index);
int8_t store_get_delay_trim(void);
int8_t store_get_matrix_trim(int index);
int8_t store_get_pwm_freq_trim(void);
int8_t store_get_voltage_trim(void);

bool store_set_servo_trim(int servo_index, int8_t trim, bool auto_save);
bool store_set_delay_trim(int8_t trim, bool auto_save);
bool store_set_matrix_trim(int index, int8_t trim, bool auto_save);
bool store_set_pwm_freq_trim(int8_t trim, bool auto_save);
bool store_set_voltage_trim(int8_t trim, bool auto_save);

uint8_t store_get_wifi_mode(void);
bool store_set_wifi_mode(uint8_t mode, bool auto_save);

const char *store_get_sta_ssid(void);
const char *store_get_sta_password(void);
const char *store_get_ap_ssid(void);
const char *store_get_ap_password(void);
uint8_t store_get_ap_channel(void);

bool store_set_sta_ssid(const char *ssid, bool auto_save);
bool store_set_sta_password(const char *pass, bool auto_save);
bool store_set_ap_ssid(const char *ssid, bool auto_save);
bool store_set_ap_password(const char *pass, bool auto_save);
bool store_set_ap_channel(uint8_t channel, bool auto_save);

bool store_is_dhcp_enabled(void);
bool store_set_dhcp_enabled(bool dhcp, bool auto_save);

uint32_t store_get_static_ip(void);
uint32_t store_get_static_netmask(void);
uint32_t store_get_static_gateway(void);
uint32_t store_get_static_dns(void);

bool store_set_static_ip(uint32_t ip, bool auto_save);
bool store_set_static_netmask(uint32_t mask, bool auto_save);
bool store_set_static_gateway(uint32_t gw, bool auto_save);
bool store_set_static_dns(uint32_t dns, bool auto_save);

const char *store_key_name(int key);
const char *store_wifi_mode_name(uint8_t mode);

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
