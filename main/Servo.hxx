/*
 * Servo.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SERVO_HXX
#define ROBOHERO_SERVO_HXX

#include <stdbool.h>
#include <stdint.h>

#include "Config.hxx"

typedef bool (*MotionYieldFn)(void *ctx);

class Servo
{
  public:
    static Servo &instance();

    void init();
    void setYield(MotionYieldFn fn, void *ctx);

    void setPwm(int servo, int val);
    void writeGpio12(int val);

    void programZero();
    void programCenter();
    void programRelax();
    bool programRun(const int matrix[][ALLMATRIX], int steps);

    void reloadPwmFreq();
    void setPwmFrequency(int freq);
    int getPwmFrequency();

    void reloadVoltage();
    void setVoltageValue(int volt);
    int getVoltageValue();

    int getRunningPos(int index);
    void setRunningPos(int index, int val);

    void applyTrim(int key, int8_t val);

  private:
    Servo();
    Servo(const Servo &);
    Servo &operator=(const Servo &);
    static Servo _self;

    void applyPose(const int *pose);
    void maybePublish(int chan, int pos);

    int _basePos[ALLMATRIX];
    int _runningPos[ALLMATRIX];
    int _lastPwm[ALLSERVOS];
    int _pwmFreq;
    int _voltage;
    MotionYieldFn _yieldFn;
    void *_yieldCtx;
    uint32_t _headDuty;
    uint32_t _headPin;
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
