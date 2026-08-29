/*
 * RoboHeroApp.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "RoboHeroApp.hxx"

extern "C" {
#include "user_interface.h"
}

RoboHeroApp::RoboHeroApp()
    : _eeprom()
    , _servo(_eeprom)
    , _web(_servo, _eeprom, *this)
    , _shell(this)
    , _servoProgram(0)
    , _servoProgramStack(0)
    , _gpioId(0)
    , _gpio12Pwm(0)
    , _engineeringModel(0)
    , _voltageCounter(0)
    , _voltageCab(0)
    , _inputVoltageLow(0)
    , _voltage(Input_Voltage)
{
}

void RoboHeroApp::setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    system_set_os_print(0);
    Serial.flush();
    while (Serial.available()) {
        Serial.read();
    }

    // Initialize EEPROM first and load parameters
    _eeprom.begin();
    delay(20);

    // GPIO PIN
    pinMode(LedPin, OUTPUT);
    digitalWrite(LedPin, LOW);

    // Engineering Model check
    pinMode(GPIO12Pin, OUTPUT);
    digitalWrite(GPIO12Pin, LOW);
    delay(100);
    pinMode(GPIO12Pin, INPUT);
    delay(100);
    _engineeringModel = digitalRead(GPIO12Pin);
    if (_engineeringModel == 1) {
        _servoProgram = 100;
    }

    // Initialize Servos & I2C
    _servo.begin();

    if (checkUartEscape()) {
        Serial.println("\nDetected escape key, the  normal booting process is bypassed...");
        _shell.showWelcome();
        return;
    }

    // Setup Wi-Fi
    if (!setupWiFi()) {
        _shell.showWelcome();
        return;
    }

    // Clear low voltage indicator
    _inputVoltageLow = 0;

    // Start Web Server
    _web.begin();

    // Show shell welcome banner & prompt
    _shell.showWelcome();
}

bool RoboHeroApp::checkUartEscape()
{
    bool escaped = false;

    while (Serial.available() > 0) {
        int c = Serial.read();
        if ((c == '\r') || (c == '\n') || (c == 0x1b) || (c == 0x03)) {
            escaped = true;
        }
    }

    return escaped;
}

bool RoboHeroApp::setupWiFi()
{
    if (checkUartEscape()) {
        Serial.println("\nDetected escape key, the  normal booting process is bypassed...");
        return false;
    }

    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    char macIDBuf[8];
    snprintf(macIDBuf, sizeof(macIDBuf), "%02x%02x",
             mac[WL_MAC_ADDR_LENGTH - 2],
             mac[WL_MAC_ADDR_LENGTH - 1]);

    const char *apSsid = _eeprom.getApSSID();
    String apSsidStr;
    if ((apSsid == NULL) || (strlen(apSsid) == 0)) {
        apSsidStr = "TTR-" + String(macIDBuf);
    } else {
        apSsidStr = String(apSsid);
    }

    const char *apPass = _eeprom.getApPassword();
    const char *staSsid = _eeprom.getStaSSID();
    const char *staPass = _eeprom.getStaPassword();
    uint8_t wifiMode = _eeprom.getWifiMode();
    uint8_t apChannel = _eeprom.getApChannel();

    // Static IP configuration if DHCP is disabled
    if (!_eeprom.isDhcpEnabled()) {
        IPAddress localIP(_eeprom.getStaticIP());
        IPAddress gateway(_eeprom.getStaticGateway());
        IPAddress subnet(_eeprom.getStaticNetmask());
        IPAddress dns(_eeprom.getStaticDNS());
        if (localIP != IPAddress(0, 0, 0, 0)) {
            WiFi.config(localIP, gateway, subnet, dns);
        }
    }

    if (wifiMode == ROBOHERO_WIFI_OFF) {
        Serial.println("Wi-Fi is disabled in EEPROM");
        WiFi.mode(WIFI_OFF);
        return true;
    }

    if (wifiMode == ROBOHERO_WIFI_AP) {
        Serial.println("Using AP Mode (from EEPROM)");
        WiFi.mode(WIFI_AP);
        if (strlen(apPass) > 0) {
            WiFi.softAP(apSsidStr.c_str(), apPass, apChannel);
        } else {
            WiFi.softAP(apSsidStr.c_str());
        }
        Serial.print("AP SSID:       ");
        Serial.println(apSsidStr);
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Using Station Mode (from EEPROM)");
        WiFi.mode(WIFI_STA);
        Serial.print("Connecting to '");
        Serial.print(staSsid);
        Serial.println("'...");

        WiFi.begin(staSsid, staPass);

        int timeout = 10;
        while ((WiFi.status() != WL_CONNECTED) && (timeout > 0)) {
            if (checkUartEscape()) {
                Serial.println("\nDetected escape key, the  normal booting process is bypassed...");
                WiFi.disconnect();
                return false;
            }
            delay(500);
            Serial.print(".");
            timeout--;
        }

        if (checkUartEscape()) {
            Serial.println("\nDetected escape key, the  normal booting process is bypassed...");
            WiFi.disconnect();
            return false;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nWiFi station connection timed out; enabling AP fallback mode");
            WiFi.mode(WIFI_AP_STA);
            if (strlen(apPass) > 0) {
                WiFi.softAP(apSsidStr.c_str(), apPass, apChannel);
            } else {
                WiFi.softAP(apSsidStr.c_str());
            }
            Serial.print("AP SSID:       ");
            Serial.println(apSsidStr);
            Serial.print("AP IP address: ");
            Serial.println(WiFi.softAPIP());
        }
    }

    return true;
}

void RoboHeroApp::loop()
{
    _shell.process();
    _web.handleClient();
    executeProgram();
    executeProgramStack();
    checkVoltage();
}

void RoboHeroApp::executeProgram()
{
    if (_servoProgram >= 1 && _inputVoltageLow == 0) {
        digitalWrite(LedPin, HIGH);

        Serial.print("Servo_PROGRAM = ");
        Serial.println(_servoProgram);

        switch (_servoProgram) {
        case 1:  // Forward
            _servo.programRun(Servo_Prg_10, Servo_Prg_10_Step);
            _servo.programCenter();
            break;
        case 2:  // Backward
            _servo.programRun(Servo_Prg_11, Servo_Prg_11_Step);
            _servo.programCenter();
            break;
        case 3:  // Turn Left
            _servo.programRun(Servo_Prg_12, Servo_Prg_12_Step);
            _servo.programCenter();
            break;
        case 4:  // Turn Right
            _servo.programRun(Servo_Prg_13, Servo_Prg_13_Step);
            _servo.programCenter();
            break;
        case 5:  // Move Left
            _servo.programRun(Servo_Prg_14, Servo_Prg_14_Step);
            _servo.programCenter();
            break;
        case 6:  // Move Right
            _servo.programRun(Servo_Prg_15, Servo_Prg_15_Step);
            _servo.programCenter();
            break;
        case 11:  // Face Up Get Up
            _servo.programRun(Servo_Prg_20, Servo_Prg_20_Step);
            _servo.programCenter();
            break;
        case 12:  // Face Down Get Up
            _servo.programRun(Servo_Prg_21, Servo_Prg_21_Step);
            _servo.programCenter();
            break;
        case 99:  // Standby
            _servo.programCenter();
            delay(300);
            break;
        case 100: // Zero
            _servo.programZero();
            delay(300);
            break;
        }

        _servoProgram = 0;
    }
}

void RoboHeroApp::executeProgramStack()
{
    if (_servoProgramStack >= 1 && _inputVoltageLow == 0) {
        digitalWrite(LedPin, HIGH);

        Serial.print("Servo_PROGRAM_Stack = ");
        Serial.println(_servoProgramStack);

        switch (_servoProgramStack) {
        case 1:  // Bow
            _servo.programRun(Servo_Prg_1, Servo_Prg_1_Step);
            _servo.programCenter();
            break;
        case 2:  // Wave
            _servo.programRun(Servo_Prg_2, Servo_Prg_2_Step);
            _servo.programCenter();
            break;
        case 3:  // Iron Man
            _servo.programRun(Servo_Prg_3, Servo_Prg_3_Step);
            _servo.programCenter();
            break;
        case 4:  // Apache
            _servo.programRun(Servo_Prg_4, Servo_Prg_4_Step);
            _servo.programCenter();
            break;
        case 5:  // Balance
            _servo.programRun(Servo_Prg_5, Servo_Prg_5_Step);
            _servo.programCenter();
            break;
        case 6:  // Warm-up
            _servo.programRun(Servo_Prg_6, Servo_Prg_6_Step);
            _servo.programCenter();
            break;
        case 7:  // Clap Hands
            _servo.programRun(Servo_Prg_7, Servo_Prg_7_Step);
            _servo.programCenter();
            break;
        case 8:  // Glico
            _servo.programRun(Servo_Prg_8, Servo_Prg_8_Step);
            _servo.programCenter();
            break;
        case 9:  // Dance
            _servo.programRun(Servo_Prg_9, Servo_Prg_9_Step);
            _servo.programCenter();
            break;
        case 99:  // Auto Demo
            _servo.programRun(Servo_Prg_1, Servo_Prg_1_Step);
            _servo.programCenter();
            delay(1000);
            _servo.programRun(Servo_Prg_2, Servo_Prg_2_Step);
            _servo.programCenter();
            delay(1000);
            _servo.programRun(Servo_Prg_6, Servo_Prg_6_Step);
            _servo.programCenter();
            delay(1000);
            _servo.programRun(Servo_Prg_3, Servo_Prg_3_Step);
            _servo.programCenter();
            delay(1000);
            _servo.programRun(Servo_Prg_4, Servo_Prg_4_Step);
            _servo.programCenter();
            delay(1000);
            _servo.programRun(Servo_Prg_5, Servo_Prg_5_Step);
            _servo.programCenter();
            delay(1000);
            _servo.programRun(Servo_Prg_1, Servo_Prg_1_Step);
            _servo.programCenter();
            break;
        }

        _servoProgramStack = 0;
    }
}

void RoboHeroApp::checkVoltage()
{
    if (_voltageCounter >= 200000) {
        _voltage = (_voltage + map(analogRead(A0), 0, 1025, 0, _servo.getVoltageValueSetting())) / 2;

        if (_engineeringModel == 1) {
            if (_voltage > 601) {
                _servo.writeGPIO12(120);
                _voltageCab--;
                _eeprom.setVoltageTrim(_voltageCab);
                _servo.setVoltageValue(Input_Voltage + _voltageCab);
            } else if (_voltage < 599) {
                _servo.writeGPIO12(60);
                _voltageCab++;
                _eeprom.setVoltageTrim(_voltageCab);
                _servo.setVoltageValue(Input_Voltage + _voltageCab);
            } else {
                _servo.writeGPIO12(90);
            }
        } else {
            if (_voltage <= Input_MinVoltage) {
                _inputVoltageLow = 1;
            }
        }

        if (_inputVoltageLow == 1) {
            if (digitalRead(LedPin) == 1) {
                digitalWrite(LedPin, LOW);
            } else {
                digitalWrite(LedPin, HIGH);
            }
        } else {
            digitalWrite(LedPin, HIGH);
        }

        _voltageCounter = 0;
    } else {
        _voltageCounter++;
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
