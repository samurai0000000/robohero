/*
 * robohero_servo.h
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SERVO_H
#define ROBOHERO_SERVO_H

#include <stdbool.h>
#include <stdint.h>

#include "robohero_config.h"

typedef bool (*motion_yield_fn)(void *ctx);

void servo_init(void);
void servo_set_yield(motion_yield_fn fn, void *ctx);

void servo_set_pwm(int servo, int val);
void servo_write_gpio12(int val);

void servo_program_zero(void);
void servo_program_center(void);
bool servo_program_run(const int matrix[][ALLMATRIX], int steps);

void servo_reload_pwm_freq(void);
void servo_set_pwm_frequency(int freq);
int servo_get_pwm_frequency(void);

void servo_reload_voltage(void);
void servo_set_voltage_value(int volt);
int servo_get_voltage_value(void);

int servo_get_running_pos(int index);
void servo_set_running_pos(int index, int val);

void servo_apply_trim(int key, int8_t val);

#endif
