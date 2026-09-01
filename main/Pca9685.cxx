/*
 * Pca9685.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "Config.hxx"
#include "Pca9685.hxx"

#define MODE1         0x00
#define PRESCALE      0xFE
#define LED0_ON_L     0x06
#define MODE1_SLEEP   0x10
#define MODE1_AI      0x20
#define MODE1_RESTART 0x80

static const char *TAG = "pca9685";

Pca9685 Pca9685::_self;

Pca9685 &Pca9685::instance()
{
    return _self;
}

Pca9685::Pca9685() : _i2cMutex(NULL) {}

esp_err_t Pca9685::i2cWriteBytes(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t *) data, len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

esp_err_t Pca9685::i2cWriteReg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2cWriteBytes(buf, sizeof(buf));
}

esp_err_t Pca9685::i2cReadReg(uint8_t reg, uint8_t *val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

void Pca9685::lockI2c()
{
    if (_i2cMutex) {
        xSemaphoreTake((SemaphoreHandle_t) _i2cMutex, portMAX_DELAY);
    }
}

void Pca9685::unlockI2c()
{
    if (_i2cMutex) {
        xSemaphoreGive((SemaphoreHandle_t) _i2cMutex);
    }
}

esp_err_t Pca9685::init()
{
    if (_i2cMutex == NULL) {
        _i2cMutex = xSemaphoreCreateMutex();
    }

    i2c_config_t conf;
    memset(&conf, 0, sizeof(conf));
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t) I2C_SDA_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t) I2C_SCL_PIN;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.clk_stretch_tick = 300;

    esp_err_t err = i2c_driver_install(I2C_NUM_0, conf.mode);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_driver_install: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config: %s", esp_err_to_name(err));
        return err;
    }

    lockI2c();
    err = i2cWriteReg(MODE1, MODE1_RESTART);
    unlockI2c();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 reset failed: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t Pca9685::setPwmFreq(int freqHz)
{
    if (freqHz < 30) {
        freqHz = 30;
    }
    if (freqHz > 200) {
        freqHz = 200;
    }

    float prescaleval = 25000000.0f / (4096.0f * (float) freqHz) - 1.0f;
    uint8_t prescale = (uint8_t) (prescaleval + 0.5f);

    lockI2c();
    uint8_t oldmode = 0;
    esp_err_t err = i2cReadReg(MODE1, &oldmode);
    if (err == ESP_OK) {
        uint8_t sleep = (oldmode & 0x7f) | MODE1_SLEEP;
        err = i2cWriteReg(MODE1, sleep);
        if (err == ESP_OK) {
            err = i2cWriteReg(PRESCALE, prescale);
        }
        if (err == ESP_OK) {
            err = i2cWriteReg(MODE1, oldmode);
        }
    }
    unlockI2c();
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(5));

    lockI2c();
    err = i2cWriteReg(MODE1, oldmode | MODE1_RESTART | MODE1_AI);
    unlockI2c();
    return err;
}

esp_err_t Pca9685::setPwm(int channel, uint16_t on, uint16_t off)
{
    if (channel < 0 || channel > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[5] = {
        (uint8_t) (LED0_ON_L + 4 * channel),
        (uint8_t) (on & 0xff),
        (uint8_t) (on >> 8),
        (uint8_t) (off & 0xff),
        (uint8_t) (off >> 8),
    };

    lockI2c();
    esp_err_t err = i2cWriteBytes(buf, sizeof(buf));
    unlockI2c();
    return err;
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
