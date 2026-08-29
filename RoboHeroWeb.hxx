/*
 * RoboHeroWeb.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_WEB_HXX
#define ROBOHERO_WEB_HXX

#include <ESP8266WebServer.h>
#include "RoboHeroConfig.hxx"
#include "RoboHeroServo.hxx"

class RoboHeroApp;

class RoboHeroWeb {

public:

    RoboHeroWeb(RoboHeroServo &servo, RoboHeroApp &app);

    void begin();
    void handleClient();

private:

    void handleIndex();
    void handleController();
    void handleSave();
    void handleGetEEPROM();
    void handleOnLine();
    void handleOnLineNew();
    void handleZero();
    void handleEditor();
    void handleSetting();
    void handleInfo();
    void handleReset();
    void handleRestart();

    ESP8266WebServer _server;
    RoboHeroServo &_servo;
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
