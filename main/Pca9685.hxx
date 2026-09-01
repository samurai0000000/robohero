/*
 * Pca9685.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_PCA9685_HXX
#define ROBOHERO_PCA9685_HXX

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

class Pca9685
{
  public:
    static Pca9685 &instance();

    esp_err_t init();
    esp_err_t setPwmFreq(int freqHz);
    esp_err_t setPwm(int channel, uint16_t on, uint16_t off);
    esp_err_t setAllOff();

  private:
    Pca9685();
    Pca9685(const Pca9685 &);
    Pca9685 &operator=(const Pca9685 &);
    static Pca9685 _self;

    esp_err_t i2cWriteBytes(const uint8_t *data, size_t len);
    esp_err_t i2cWriteReg(uint8_t reg, uint8_t val);
    esp_err_t i2cReadReg(uint8_t reg, uint8_t *val);
    void lockI2c();
    void unlockI2c();

    void *_i2cMutex;
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
