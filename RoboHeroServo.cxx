/*
 * RoboHeroServo.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "RoboHeroServo.hxx"

RoboHeroServo::RoboHeroServo()
    : _pwm(0x40)
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
        _runningServoPos[index] = Servo_Act_1[index] + (int8_t) EEPROM.read(index);
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
        _runningServoPos[i] = Servo_Act_0[i] + (int8_t) EEPROM.read(i);
    }

    for (int i = 0; i < ALLSERVOS; i++) {
        setPWMtoServo(i, _runningServoPos[i]);
        delay(10);
    }
}

void RoboHeroServo::programCenter()
{
    for (int i = 0; i < ALLMATRIX; i++) {
        _runningServoPos[i] = Servo_Act_1[i] + (int8_t) EEPROM.read(i);
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
        int InterTotalTime = iMatrix[MainLoopIndex][ALLMATRIX - 1] +
            (int8_t) EEPROM.read(ALLMATRIX - 1);
        int InterDelayCounter = InterTotalTime / BASEDELAYTIME;
        for (int InterStepLoop = 0; InterStepLoop < InterDelayCounter;
             InterStepLoop++) {
            for (int ServoIndex = 0; ServoIndex < ALLSERVOS; ServoIndex++) {
                INT_TEMP_A = _runningServoPos[ServoIndex];
                INT_TEMP_B = iMatrix[MainLoopIndex][ServoIndex] +
                    (int8_t) EEPROM.read(ServoIndex);
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
            _runningServoPos[Index] = iMatrix[MainLoopIndex][Index] +
                (int8_t) EEPROM.read(Index);
        }
    }
}

void RoboHeroServo::getPWMFrequency()
{
    _setPWMFreq = PWM_Frequency + (int8_t) EEPROM.read(ALLSERVOS + 1);
}

void RoboHeroServo::setPWMFrequency(int freq)
{
    _setPWMFreq = freq;
    _pwm.setPWMFreq(_setPWMFreq);
}

int RoboHeroServo::getPWMFrequencySetting() const
{
    return _setPWMFreq;
}

void RoboHeroServo::getVoltageValue()
{
    _setVoltage = Input_Voltage + (int8_t) EEPROM.read(ALLSERVOS + 2);
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
    EEPROM.write(key, value);
    EEPROM.commit();
}

int8_t RoboHeroServo::readKeyValue(int8_t key)
{
    return (int8_t) EEPROM.read(key);
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
