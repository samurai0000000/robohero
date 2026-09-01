/*
 * pca9685.h
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef PCA9685_H
#define PCA9685_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t pca9685_init(void);
esp_err_t pca9685_set_pwm_freq(int freq_hz);
esp_err_t pca9685_set_pwm(int channel, uint16_t on, uint16_t off);

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
