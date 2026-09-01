/*
 * Servo.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/pwm.h"
#include "esp_log.h"

#include "Motions.hxx"
#include "Mqtt.hxx"
#include "Pca9685.hxx"
#include "Servo.hxx"
#include "Store.hxx"

static const char *TAG = "servo";

static int mapInt(int x, int inMin, int inMax, int outMin, int outMax)
{
    if (inMax == inMin) {
        return outMin;
    }
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

static int clampTrim(int trim)
{
    if (trim < -125 || trim > 125) {
        return 0;
    }
    return trim;
}

Servo Servo::_self;

Servo &Servo::instance()
{
    return _self;
}

Servo::Servo()
    : _pwmFreq(PWM_FREQUENCY), _voltage(INPUT_VOLTAGE), _yieldFn(NULL),
      _yieldCtx(NULL), _headDuty(0), _headPin(GPIO12_PIN)
{
    for (int i = 0; i < ALLMATRIX; i++) {
        _basePos[i] = 0;
        _runningPos[i] = 0;
    }
    for (int i = 0; i < ALLSERVOS; i++) {
        _lastPwm[i] = -1;
    }
}

void Servo::maybePublish(int chan, int pos)
{
    if (chan < 0 || chan >= ALLSERVOS) {
        return;
    }
    if (_lastPwm[chan] == pos) {
        return;
    }
    _lastPwm[chan] = pos;
    Mqtt::instance().sendPwmPos(chan, pos);
}

void Servo::writeGpio12(int val)
{
    if (val < 0) {
        val = 0;
    }
    if (val > 180) {
        val = 180;
    }
    _headDuty =
        (uint32_t) mapInt(val, 0, 180, HEAD_SERVO_MIN_US, HEAD_SERVO_MAX_US);
    pwm_set_duty(0, _headDuty);
    pwm_start();
    maybePublish(16, val);
}

void Servo::setPwm(int servo, int val)
{
    int pwmval = mapInt(val, PWMRES_MIN, PWMRES_MAX, SERVOMIN, SERVOMAX);

    if (servo >= 16) {
        writeGpio12(val);
    } else {
        Pca9685::instance().setPwm(servo, 0, (uint16_t) pwmval);
        maybePublish(servo, val);
    }
}

void Servo::init()
{
    _headDuty = HEAD_SERVO_MIN_US +
                (90 * (HEAD_SERVO_MAX_US - HEAD_SERVO_MIN_US)) / 180;
    pwm_init(HEAD_PWM_PERIOD_US, &_headDuty, 1, &_headPin);
    /* SDK leaves channel phase uninitialized; 0 is in-range. */
    pwm_set_phase(0, 0);
    pwm_start();

    if (Pca9685::instance().init() != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 init failed");
    }

    reloadPwmFreq();
    if (Pca9685::instance().setPwmFreq(_pwmFreq) != ESP_OK) {
        ESP_LOGW(TAG, "PCA9685 set freq failed");
    }
    reloadVoltage();

    Store &st = Store::instance();
    for (int i = 0; i < ALLMATRIX; i++) {
        _basePos[i] = Servo_Act_1[i];
        _runningPos[i] = _basePos[i] + clampTrim(st.getMatrixTrim(i));
    }
}

void Servo::setYield(MotionYieldFn fn, void *ctx)
{
    _yieldFn = fn;
    _yieldCtx = ctx;
}

void Servo::applyPose(const int *pose)
{
    Store &st = Store::instance();
    for (int i = 0; i < ALLMATRIX; i++) {
        _basePos[i] = pose[i];
        _runningPos[i] = _basePos[i] + clampTrim(st.getMatrixTrim(i));
    }
    for (int i = 0; i < ALLSERVOS; i++) {
        setPwm(i, _runningPos[i]);
        vTaskDelay(pdMS_TO_TICKS(BASEDELAYTIME));
    }
}

void Servo::programZero()
{
    applyPose(Servo_Act_0);
}

void Servo::programCenter()
{
    applyPose(Servo_Act_1);
}

bool Servo::programRun(const int matrix[][ALLMATRIX], int steps)
{
    Store &st = Store::instance();

    for (int mainI = 0; mainI < steps; mainI++) {
        int delayTrim = st.getDelayTrim();
        if (delayTrim < -125 || delayTrim > 125) {
            delayTrim = 0;
        }
        int total = matrix[mainI][ALLMATRIX - 1] + delayTrim;
        if (total < BASEDELAYTIME) {
            total = BASEDELAYTIME;
        }
        int nsteps = total / BASEDELAYTIME;
        if (nsteps < 1) {
            nsteps = 1;
        }

        for (int step = 0; step < nsteps; step++) {
            for (int s = 0; s < ALLSERVOS; s++) {
                int from = _runningPos[s];
                int to = matrix[mainI][s] + clampTrim(st.getServoTrim(s));
                if (from == to) {
                    continue;
                }
                int delta = mapInt(BASEDELAYTIME * step,
                                   0,
                                   total,
                                   0,
                                   (from > to) ? (from - to) : (to - from));
                if (from > to) {
                    if (from - delta >= to) {
                        setPwm(s, from - delta);
                    }
                } else if (from + delta <= to) {
                    setPwm(s, from + delta);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(BASEDELAYTIME));
            if (_yieldFn && _yieldFn(_yieldCtx)) {
                return false;
            }
        }

        for (int i = 0; i < ALLMATRIX; i++) {
            _runningPos[i] = matrix[mainI][i] + clampTrim(st.getMatrixTrim(i));
        }
    }

    return true;
}

void Servo::reloadPwmFreq()
{
    int trim = Store::instance().getPwmFreqTrim();
    if (trim < -24 || trim > 100) {
        trim = 0;
    }
    _pwmFreq = PWM_FREQUENCY + trim;
    if (_pwmFreq < 30 || _pwmFreq > 200) {
        _pwmFreq = PWM_FREQUENCY;
    }
}

void Servo::setPwmFrequency(int freq)
{
    if (freq >= 30 && freq <= 200) {
        _pwmFreq = freq;
        Pca9685::instance().setPwmFreq(_pwmFreq);
    }
}

int Servo::getPwmFrequency()
{
    return _pwmFreq;
}

void Servo::reloadVoltage()
{
    int trim = Store::instance().getVoltageTrim();
    if (trim < -200 || trim > 200) {
        trim = 0;
    }
    _voltage = INPUT_VOLTAGE + trim;
    if (_voltage <= 0) {
        _voltage = INPUT_VOLTAGE;
    }
}

void Servo::setVoltageValue(int volt)
{
    _voltage = volt;
}

int Servo::getVoltageValue()
{
    return _voltage;
}

int Servo::getRunningPos(int index)
{
    if (index >= 0 && index < ALLMATRIX) {
        return _runningPos[index];
    }
    return 0;
}

void Servo::setRunningPos(int index, int val)
{
    if (index >= 0 && index < ALLMATRIX) {
        _runningPos[index] = val;
    }
}

void Servo::applyTrim(int key, int8_t val)
{
    if (key < 0 || key > 19) {
        return;
    }

    Store::instance().writeKey(key, val, false);

    if (key >= 0 && key <= 15) {
        int target = _basePos[key] + val;
        _runningPos[key] = target;
        setPwm(key, target);
    } else if (key == STORE_KEY_SERVO_GPIO12) {
        int target = _basePos[16] + val;
        _runningPos[16] = target;
        writeGpio12(target);
    } else if (key == STORE_KEY_PWM_FREQ) {
        setPwmFrequency(PWM_FREQUENCY + val);
    } else if (key == STORE_KEY_VOLTAGE_CAL) {
        setVoltageValue(INPUT_VOLTAGE + val);
    }
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
