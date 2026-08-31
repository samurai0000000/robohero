/*
 * robohero_servo.c
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/pwm.h"
#include "esp_log.h"

#include "pca9685.h"
#include "robohero_motions.h"
#include "robohero_servo.h"
#include "robohero_store.h"

static const char *TAG = "servo";

static int s_base_pos[ALLMATRIX];
static int s_running_pos[ALLMATRIX];
static int s_pwm_freq = PWM_FREQUENCY;
static int s_voltage = INPUT_VOLTAGE;
static motion_yield_fn s_yield_fn;
static void *s_yield_ctx;
static uint32_t s_head_duty;
static const uint32_t s_head_pin = GPIO12_PIN;

static int map_int(int x, int in_min, int in_max, int out_min, int out_max)
{
    if (in_max == in_min) {
        return out_min;
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static int clamp_trim(int trim)
{
    if (trim < -125 || trim > 125) {
        return 0;
    }
    return trim;
}

void servo_write_gpio12(int val)
{
    if (val < 0) {
        val = 0;
    }
    if (val > 180) {
        val = 180;
    }
    s_head_duty = (uint32_t) map_int(val, 0, 180, HEAD_SERVO_MIN_US, HEAD_SERVO_MAX_US);
    pwm_set_duty(0, s_head_duty);
    pwm_start();
}

void servo_set_pwm(int servo, int val)
{
    int pwmval = map_int(val, PWMRES_MIN, PWMRES_MAX, SERVOMIN, SERVOMAX);

    if (servo >= 16) {
        servo_write_gpio12(val);
    } else {
        pca9685_set_pwm(servo, 0, (uint16_t) pwmval);
    }
}

void servo_init(void)
{
    s_head_duty = HEAD_SERVO_MIN_US +
        (90 * (HEAD_SERVO_MAX_US - HEAD_SERVO_MIN_US)) / 180;
    pwm_init(HEAD_PWM_PERIOD_US, &s_head_duty, 1, &s_head_pin);
    pwm_start();

    if (pca9685_init() != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 init failed");
    }

    servo_reload_pwm_freq();
    if (pca9685_set_pwm_freq(s_pwm_freq) != ESP_OK) {
        ESP_LOGW(TAG, "PCA9685 set freq failed");
    }
    servo_reload_voltage();

    for (int i = 0; i < ALLMATRIX; i++) {
        s_base_pos[i] = Servo_Act_1[i];
        s_running_pos[i] = s_base_pos[i] + clamp_trim(store_get_matrix_trim(i));
    }
}

void servo_set_yield(motion_yield_fn fn, void *ctx)
{
    s_yield_fn = fn;
    s_yield_ctx = ctx;
}

static void apply_pose(const int *pose)
{
    for (int i = 0; i < ALLMATRIX; i++) {
        s_base_pos[i] = pose[i];
        s_running_pos[i] = s_base_pos[i] + clamp_trim(store_get_matrix_trim(i));
    }
    for (int i = 0; i < ALLSERVOS; i++) {
        servo_set_pwm(i, s_running_pos[i]);
        vTaskDelay(pdMS_TO_TICKS(BASEDELAYTIME));
    }
}

void servo_program_zero(void)
{
    apply_pose(Servo_Act_0);
}

void servo_program_center(void)
{
    apply_pose(Servo_Act_1);
}

bool servo_program_run(const int matrix[][ALLMATRIX], int steps)
{
    for (int main_i = 0; main_i < steps; main_i++) {
        int delay_trim = store_get_delay_trim();
        if (delay_trim < -125 || delay_trim > 125) {
            delay_trim = 0;
        }
        int total = matrix[main_i][ALLMATRIX - 1] + delay_trim;
        if (total < BASEDELAYTIME) {
            total = BASEDELAYTIME;
        }
        int nsteps = total / BASEDELAYTIME;
        if (nsteps < 1) {
            nsteps = 1;
        }

        for (int step = 0; step < nsteps; step++) {
            for (int s = 0; s < ALLSERVOS; s++) {
                int from = s_running_pos[s];
                int to = matrix[main_i][s] + clamp_trim(store_get_servo_trim(s));
                if (from == to) {
                    continue;
                }
                int delta = map_int(BASEDELAYTIME * step, 0, total, 0,
                                    (from > to) ? (from - to) : (to - from));
                if (from > to) {
                    if (from - delta >= to) {
                        servo_set_pwm(s, from - delta);
                    }
                } else if (from + delta <= to) {
                    servo_set_pwm(s, from + delta);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(BASEDELAYTIME));
            if (s_yield_fn && s_yield_fn(s_yield_ctx)) {
                return false;
            }
        }

        for (int i = 0; i < ALLMATRIX; i++) {
            s_running_pos[i] = matrix[main_i][i] + clamp_trim(store_get_matrix_trim(i));
        }
    }

    return true;
}

void servo_reload_pwm_freq(void)
{
    int trim = store_get_pwm_freq_trim();
    if (trim < -24 || trim > 100) {
        trim = 0;
    }
    s_pwm_freq = PWM_FREQUENCY + trim;
    if (s_pwm_freq < 30 || s_pwm_freq > 200) {
        s_pwm_freq = PWM_FREQUENCY;
    }
}

void servo_set_pwm_frequency(int freq)
{
    if (freq >= 30 && freq <= 200) {
        s_pwm_freq = freq;
        pca9685_set_pwm_freq(s_pwm_freq);
    }
}

int servo_get_pwm_frequency(void)
{
    return s_pwm_freq;
}

void servo_reload_voltage(void)
{
    int trim = store_get_voltage_trim();
    if (trim < -200 || trim > 200) {
        trim = 0;
    }
    s_voltage = INPUT_VOLTAGE + trim;
    if (s_voltage <= 0) {
        s_voltage = INPUT_VOLTAGE;
    }
}

void servo_set_voltage_value(int volt)
{
    s_voltage = volt;
}

int servo_get_voltage_value(void)
{
    return s_voltage;
}

int servo_get_running_pos(int index)
{
    if (index >= 0 && index < ALLMATRIX) {
        return s_running_pos[index];
    }
    return 0;
}

void servo_set_running_pos(int index, int val)
{
    if (index >= 0 && index < ALLMATRIX) {
        s_running_pos[index] = val;
    }
}

void servo_apply_trim(int key, int8_t val)
{
    if (key < 0 || key > 19) {
        return;
    }

    store_write_key(key, val, false);

    if (key >= 0 && key <= 15) {
        int target = s_base_pos[key] + val;
        s_running_pos[key] = target;
        servo_set_pwm(key, target);
    } else if (key == STORE_KEY_SERVO_GPIO12) {
        int target = s_base_pos[16] + val;
        s_running_pos[16] = target;
        servo_write_gpio12(target);
    } else if (key == STORE_KEY_PWM_FREQ) {
        servo_set_pwm_frequency(PWM_FREQUENCY + val);
    } else if (key == STORE_KEY_VOLTAGE_CAL) {
        servo_set_voltage_value(INPUT_VOLTAGE + val);
    }
}
