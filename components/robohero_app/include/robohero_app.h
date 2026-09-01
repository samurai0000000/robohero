/*
 * robohero_app.h
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_APP_H
#define ROBOHERO_APP_H

#include <stdbool.h>
#include <stdint.h>

enum rh_cmd_type {
    RH_CMD_PM = 1,
    RH_CMD_PMS,
    RH_CMD_STOP,
    RH_CMD_CENTER,
    RH_CMD_ZERO,
};

typedef void (*robohero_wifi_up_fn)(void);

void robohero_app_on_wifi_up(robohero_wifi_up_fn fn);
void robohero_console_begin(void);
void robohero_terminal_reset(void);
void robohero_app_start(void);
bool robohero_app_apply_netif(void);
bool robohero_app_wifi_ready(void);

bool robohero_app_is_busy(void);
bool robohero_app_is_low_voltage(void);
void robohero_app_reset_low_voltage(void);
int robohero_app_get_voltage(void);
int robohero_app_engineering_model(void);

bool robohero_app_submit_pm(int id);
bool robohero_app_submit_pms(int id);
bool robohero_app_request_stop(void);
void robohero_app_submit_center(void);
void robohero_app_submit_zero(void);

#endif
