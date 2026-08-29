/*
 * RoboHeroServo.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "RoboHeroServo.hxx"

RoboHeroServo::RoboHeroServo(RoboHeroEeprom &eeprom)
    : _eeprom(eeprom)
    , _pwm(0x40)
    , _setPWMFreq(PWM_Frequency)
    , _setVoltage(Input_Voltage)
    , _head(0)
    , _tail(0)
{
    memset(_runningServoPos, 0, sizeof(_runningServoPos));
    memset(_frameBuffer, 0, sizeof(_frameBuffer));
    memset(_framePop, 0, sizeof(_framePop));
}

void RoboHeroServo::begin()
{
    // Software PWM PIN
    _gpio12Servo.attach(GPIO12Pin);

    // Initialize I2C
    Wire.begin(4, 5);

    // Initialize PCA9685 PWMServoDriver
    _pwm.begin();
    getPWMFrequency();
    _pwm.setPWMFreq(_setPWMFreq);

    // Load initial voltage threshold
    getVoltageValue();

    // Initialize running servo positions from Standby pose + EEPROM offsets
    for (int index = 0; index < ALLMATRIX; index++) {
        int trim = _eeprom.getMatrixTrim(index);
        if ((trim < -125) || (trim > 125)) {
            trim = 0;
        }
        _runningServoPos[index] = Servo_Act_1[index] + trim;
    }
}

void RoboHeroServo::setPWMtoServo(int servo, int val)
{
    int pwmval = map(val, PWMRES_Min, PWMRES_Max, SERVOMIN, SERVOMAX);

    if (servo >= 16) {
        _gpio12Servo.write(val);
    } else {
        _pwm.setPWM(servo, 0, pwmval);
    }
}

void RoboHeroServo::writeGPIO12(int val)
{
    _gpio12Servo.write(val);
}

void RoboHeroServo::programZero()
{
    for (int i = 0; i < ALLMATRIX; i++) {
        int trim = _eeprom.getMatrixTrim(i);
        if ((trim < -125) || (trim > 125)) {
            trim = 0;
        }
        _runningServoPos[i] = Servo_Act_0[i] + trim;
    }

    for (int i = 0; i < ALLSERVOS; i++) {
        setPWMtoServo(i, _runningServoPos[i]);
        delay(10);
    }
}

void RoboHeroServo::programCenter()
{
    for (int i = 0; i < ALLMATRIX; i++) {
        int trim = _eeprom.getMatrixTrim(i);
        if ((trim < -125) || (trim > 125)) {
            trim = 0;
        }
        _runningServoPos[i] = Servo_Act_1[i] + trim;
    }

    for (int i = 0; i < ALLSERVOS; i++) {
        setPWMtoServo(i, _runningServoPos[i]);
        delay(10);
    }
}

void RoboHeroServo::programRun(const int iMatrix[][ALLMATRIX], int iSteps)
{
    int INT_TEMP_A, INT_TEMP_B, INT_TEMP_C;

    for (int MainLoopIndex = 0; MainLoopIndex < iSteps; MainLoopIndex++) {
        int delayTrim = _eeprom.getDelayTrim();
        if ((delayTrim < -125) || (delayTrim > 125)) {
            delayTrim = 0;
        }
        int InterTotalTime = iMatrix[MainLoopIndex][ALLMATRIX - 1] + delayTrim;
        if (InterTotalTime < BASEDELAYTIME) {
            InterTotalTime = BASEDELAYTIME;
        }

        int InterDelayCounter = InterTotalTime / BASEDELAYTIME;
        if (InterDelayCounter < 1) {
            InterDelayCounter = 1;
        }

        for (int InterStepLoop = 0; InterStepLoop < InterDelayCounter;
             InterStepLoop++) {
            for (int ServoIndex = 0; ServoIndex < ALLSERVOS; ServoIndex++) {
                INT_TEMP_A = _runningServoPos[ServoIndex];
                int sTrim = _eeprom.getServoTrim(ServoIndex);
                if ((sTrim < -125) || (sTrim > 125)) {
                    sTrim = 0;
                }
                INT_TEMP_B = iMatrix[MainLoopIndex][ServoIndex] + sTrim;
                if (INT_TEMP_A == INT_TEMP_B) {
                    INT_TEMP_C = INT_TEMP_B;
                } else if (INT_TEMP_A > INT_TEMP_B) {
                    INT_TEMP_C = map(BASEDELAYTIME * InterStepLoop,
                                     0,
                                     InterTotalTime,
                                     0,
                                     INT_TEMP_A - INT_TEMP_B);
                    if (INT_TEMP_A - INT_TEMP_C >= INT_TEMP_B) {
                        setPWMtoServo(ServoIndex, INT_TEMP_A - INT_TEMP_C);
                    }
                } else if (INT_TEMP_A < INT_TEMP_B) {
                    INT_TEMP_C = map(BASEDELAYTIME * InterStepLoop,
                                     0,
                                     InterTotalTime,
                                     0,
                                     INT_TEMP_B - INT_TEMP_A);
                    if (INT_TEMP_A + INT_TEMP_C <= INT_TEMP_B) {
                        setPWMtoServo(ServoIndex, INT_TEMP_A + INT_TEMP_C);
                    }
                }
            }

            delay(BASEDELAYTIME);
        }

        for (int Index = 0; Index < ALLMATRIX; Index++) {
            int mTrim = _eeprom.getMatrixTrim(Index);
            if ((mTrim < -125) || (mTrim > 125)) {
                mTrim = 0;
            }
            _runningServoPos[Index] = iMatrix[MainLoopIndex][Index] + mTrim;
        }
    }
}

void RoboHeroServo::getPWMFrequency()
{
    int trim = _eeprom.getPwmFreqTrim();
    if ((trim < -24) || (trim > 100)) {
        trim = 0;
    }
    _setPWMFreq = PWM_Frequency + trim;
    if ((_setPWMFreq < 30) || (_setPWMFreq > 200)) {
        _setPWMFreq = PWM_Frequency;
    }
}

void RoboHeroServo::setPWMFrequency(int freq)
{
    if ((freq >= 30) && (freq <= 200)) {
        _setPWMFreq = freq;
        _pwm.setPWMFreq(_setPWMFreq);
    }
}

int RoboHeroServo::getPWMFrequencySetting() const
{
    return _setPWMFreq;
}

void RoboHeroServo::getVoltageValue()
{
    int trim = _eeprom.getVoltageTrim();
    if ((trim < -200) || (trim > 200)) {
        trim = 0;
    }
    _setVoltage = Input_Voltage + trim;
    if (_setVoltage <= 0) {
        _setVoltage = Input_Voltage;
    }
}

void RoboHeroServo::setVoltageValue(int volt)
{
    _setVoltage = volt;
}

int RoboHeroServo::getVoltageValueSetting() const
{
    return _setVoltage;
}

void RoboHeroServo::writeKeyValue(int8_t key, int8_t value)
{
    _eeprom.writeKeyValue(key, value);
}

int8_t RoboHeroServo::readKeyValue(int8_t key)
{
    return _eeprom.readKeyValue(key);
}

void RoboHeroServo::push(int frame[])
{
    noInterrupts();

    if ((_tail + 1 % FRAME_BUFFER_MAX) != _head) {
        for (int i = 0; i < ALLMATRIX; i++) {
            _frameBuffer[_tail][i] = frame[i];
        }
        _tail = (_tail + 1) % FRAME_BUFFER_MAX;
    } else {
        Serial.println("FrameBuffer Overflow");
    }

    interrupts();
}

void RoboHeroServo::pop(int frame[])
{
    noInterrupts();

    if (_head != _tail) {
        for (int i = 0; i < ALLMATRIX; i++) {
            frame[i] = _frameBuffer[_head][i];
        }

        _head = (_head + 1) % FRAME_BUFFER_MAX;
    } else {
        Serial.println("FrameBuffer No Data");
    }

    interrupts();
}

int RoboHeroServo::getBufferLength()
{
    int ret = 0;

    noInterrupts();

    if (_head == _tail) {
        ret = 0;
    } else {
        if (_tail > _head) {
            ret = _tail - _head;
        } else {
            ret = (_tail + FRAME_BUFFER_MAX - _head) % FRAME_BUFFER_MAX;
        }
    }

    if (ret < -1) {
        Serial.print("Ring Buffer Error, Tail:");
        Serial.print(_tail);
        Serial.print(" Head:");
        Serial.print(_head);
        Serial.print(" ret:");
        Serial.println(ret);
    }

    interrupts();

    return ret;
}

int RoboHeroServo::getRunningServoPos(int index) const
{
    if (index >= 0 && index < ALLMATRIX) {
        return _runningServoPos[index];
    }
    return 0;
}

void RoboHeroServo::setRunningServoPos(int index, int val)
{
    if (index >= 0 && index < ALLMATRIX) {
        _runningServoPos[index] = val;
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
