/*
 * RoboHeroApp.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_APP_HXX
#define ROBOHERO_APP_HXX

#include "RoboHeroShell.hxx"
#include "RoboHeroEeprom.hxx"
#include <ESP8266WiFi.h>
#include "RoboHeroConfig.hxx"
#include "RoboHeroMotions.hxx"
#include "RoboHeroServo.hxx"
#include "RoboHeroWeb.hxx"

class RoboHeroApp {

public:

    RoboHeroApp();

    void setup();
    void loop();

    int getServoProgram() const { return _servoProgram; }
    void setServoProgram(int prog) { _servoProgram = prog; }

    int getServoProgramStack() const { return _servoProgramStack; }
    void setServoProgramStack(int prog) { _servoProgramStack = prog; }

    int isLowVoltage() const { return _inputVoltageLow; }
    void setLowVoltage(int low) { _inputVoltageLow = low; }
    void resetLowVoltage() { _inputVoltageLow = 0; }

    int getVoltage() const { return _voltage; }
    void setVoltage(int volt) { _voltage = volt; }

    int getEngineeringModel() const { return _engineeringModel; }
    int getVoltageCab() const { return _voltageCab; }
    void setVoltageCab(int cab) { _voltageCab = cab; }

    RoboHeroEeprom &getEeprom() { return _eeprom; }
    RoboHeroServo &getServo() { return _servo; }
    RoboHeroWeb &getWeb() { return _web; }
    RoboHeroShell &getShell() { return _shell; }

    void requestCancel() { _cancelRequested = true; }
    bool isCancelRequested() const { return _cancelRequested; }
    bool isMotionBusy() const { return _motionBusy; }
    void pollDuringMotion();

private:

    bool setupWiFi();
    bool checkUartEscape();
    void checkVoltage();
    void executeProgram();
    void executeProgramStack();
    bool interruptibleDelay(int ms);
    bool runThenCenter(const int iMatrix[][ALLMATRIX], int iSteps);

    RoboHeroEeprom _eeprom;
    RoboHeroServo _servo;
    RoboHeroWeb _web;
    RoboHeroShell _shell;

    int _servoProgram;
    int _servoProgramStack;
    int _gpioId;
    int _gpio12Pwm;

    int _engineeringModel;
    int _voltageCounter;
    int _voltageCab;
    int _inputVoltageLow;
    int _voltage;

    bool _motionBusy;
    bool _cancelRequested;

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
