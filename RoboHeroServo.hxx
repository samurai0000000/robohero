/*
 * RoboHeroServo.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SERVO_HXX
#define ROBOHERO_SERVO_HXX

#include <Wire.h>
#include <Servo.h>
#include <Adafruit_PWMServoDriver.h>
#include <Ticker.h>
#include "RoboHeroConfig.hxx"
#include "RoboHeroEeprom.hxx"
#include "RoboHeroMotions.hxx"

class RoboHeroServo {

public:

    RoboHeroServo(RoboHeroEeprom &eeprom);

    void begin();

    void setPWMtoServo(int servo, int val);
    void writeGPIO12(int val);

    typedef bool (*MotionYieldFn)(void *ctx);

    void setMotionYield(MotionYieldFn fn, void *ctx);

    void programZero();
    void programCenter();
    bool programRun(const int iMatrix[][ALLMATRIX], int iSteps);

    void getPWMFrequency();
    void setPWMFrequency(int freq);
    int getPWMFrequencySetting() const;

    void getVoltageValue();
    void setVoltageValue(int volt);
    int getVoltageValueSetting() const;

    void writeKeyValue(int8_t key, int8_t value);
    int8_t readKeyValue(int8_t key);

    void push(int frame[]);
    void pop(int frame[]);
    int getBufferLength();

    int getRunningServoPos(int index) const;
    void setRunningServoPos(int index, int val);

    void applyTrim(int key, int8_t val);

    Servo &getGPIO12Servo() { return _gpio12Servo; }
    Adafruit_PWMServoDriver &getPWMServoDriver() { return _pwm; }
    RoboHeroEeprom &getEeprom() { return _eeprom; }

private:

    RoboHeroEeprom &_eeprom;
    Adafruit_PWMServoDriver _pwm;
    Servo _gpio12Servo;

    int _baseServoPos[ALLMATRIX];
    int _runningServoPos[ALLMATRIX];
    int _setPWMFreq;
    int _setVoltage;

    int _head;
    int _tail;
    int _frameBuffer[FRAME_BUFFER_MAX + 1][ALLMATRIX];
    int _framePop[FRAME_BUFFER_MAX + 1][ALLMATRIX];
    Ticker _servoTicker;

    MotionYieldFn _yieldFn;
    void *_yieldCtx;

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
