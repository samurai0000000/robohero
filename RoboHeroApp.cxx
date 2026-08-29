/*
 * RoboHeroApp.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "RoboHeroApp.hxx"

RoboHeroApp::RoboHeroApp()
    : _servo()
    , _web(_servo, *this)
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
    Serial.println("RoboHero Start");

    // EEPROM must be initialized first
    EEPROM.begin(64);
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

    // Setup Wi-Fi
    setupWiFi();

    // Clear low voltage indicator
    _inputVoltageLow = 0;

    // Start Web Server
    _web.begin();
}

void RoboHeroApp::setupWiFi()
{
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
        String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = "TTR-" + macID;

    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);

    for (int i = 0; i < AP_NameString.length(); i++) {
        AP_NameChar[i] = AP_NameString.charAt(i);
    }

    if (AP_MODE) {
        Serial.println("Using AP Mode");
        WiFi.softAP(AP_NameChar, AP_PASSWORD);
    } else {
        Serial.println("Using Client Mode");
        WiFi.begin(CLIENT_SSID, CLIENT_PASSWORD);

        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.print("Connecting ");
            Serial.print(CLIENT_SSID);
            Serial.println("...");
        }
        Serial.println("WiFi connected");
        delay(500);
    }

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void RoboHeroApp::loop()
{
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
                _servo.writeKeyValue(19, _voltageCab);
                _servo.setVoltageValue(Input_Voltage + _voltageCab);
            } else if (_voltage < 599) {
                _servo.writeGPIO12(60);
                _voltageCab++;
                _servo.writeKeyValue(19, _voltageCab);
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
