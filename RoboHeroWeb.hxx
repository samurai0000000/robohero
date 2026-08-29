/*
 * RoboHeroWeb.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_WEB_HXX
#define ROBOHERO_WEB_HXX

#include "RoboHeroConfig.hxx"
#include <ESP8266WebServer.h>
#include "RoboHeroServo.hxx"

class RoboHeroApp;
class RoboHeroEeprom;

class RoboHeroWeb {

public:

    RoboHeroWeb(RoboHeroServo &servo, RoboHeroEeprom &eeprom, RoboHeroApp &app);

    void begin();
    void handleClient();

private:

    void handleIndex();
    void handleCalibrate();

    ESP8266WebServer _server;
    RoboHeroServo &_servo;
    RoboHeroEeprom &_eeprom;
    RoboHeroApp &_app;

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
