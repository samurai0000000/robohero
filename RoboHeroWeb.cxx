/*
 * RoboHeroWeb.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "RoboHeroWeb.hxx"
#include "RoboHeroApp.hxx"

RoboHeroWeb::RoboHeroWeb(RoboHeroServo &servo, RoboHeroEeprom &eeprom, RoboHeroApp &app)
    : _server(80)
    , _servo(servo)
    , _eeprom(eeprom)
    , _app(app)
{
}

void RoboHeroWeb::begin()
{
    HTTPMethod getMethod = HTTP_GET;

    _server.on("/controller", getMethod, [this]() { handleController(); });
    _server.on("/save", getMethod, [this]() { handleSave(); });

    _server.on("/", getMethod, [this]() { handleIndex(); });
    _server.on("/editor", getMethod, [this]() { handleEditor(); });
    _server.on("/zero", getMethod, [this]() { handleZero(); });
    _server.on("/setting", getMethod, [this]() { handleSetting(); });
    _server.on("/eeprom", getMethod, [this]() { handleGetEEPROM(); });

    _server.on("/online", getMethod, [this]() { handleOnLine(); });
    _server.on("/online_new", getMethod, [this]() { handleOnLineNew(); });

    _server.on("/info", getMethod, [this]() { handleInfo(); });
    _server.on("/reset", getMethod, [this]() { handleReset(); });
    _server.on("/restart", getMethod, [this]() { handleRestart(); });

    _server.begin();
}

void RoboHeroWeb::handleClient()
{
    _server.handleClient();
}

void RoboHeroWeb::handleSave()
{
    String key = _server.arg("key");
    String value = _server.arg("value");

    int8_t keyInt = key.toInt();
    int8_t valueInt = value.toInt();

    if (keyInt == 100) {
        _eeprom.resetMotionTrims();
    } else {
        if (valueInt >= -125 && valueInt <= 125) {
            _eeprom.writeKeyValue(keyInt, valueInt);

            if (keyInt == 18) {
                _servo.setPWMFrequency(PWM_Frequency + valueInt);
            }

            if (keyInt == 19) {
                _servo.setVoltageValue(Input_Voltage + valueInt);
                _app.resetLowVoltage();
            }
        }
    }

    String content = "(key, value)=(" + key + "," + value +
        "), but response = ";
    content += String(_eeprom.readKeyValue(keyInt));

    _server.send(200, "text/html", content);
}

void RoboHeroWeb::handleController()
{
    String pm = _server.arg("pm");
    String pms = _server.arg("pms");
    String gpid = _server.arg("gpid");
    String gpio = _server.arg("gpio");
    String servo = _server.arg("servo");

    if (pm != "") {
        _app.setServoProgram(pm.toInt());
    }

    if (pms != "") {
        _app.setServoProgramStack(pms.toInt());
    }

    if (servo != "" && _app.isLowVoltage() == 0) {
        int Servo_ID = servo.toInt();
        String ival = _server.arg("value");
        int Servo_PWM = ival.toInt() + _eeprom.getServoTrim(Servo_ID);
        int pulselength = map(Servo_PWM, PWMRES_Min, PWMRES_Max, SERVOMIN, SERVOMAX);

        if (Servo_ID != 16) {
            _servo.getPWMServoDriver().setPWM(Servo_ID, 0, pulselength);
        } else if (Servo_ID == 16) {
            int gpio12Pwm = ival.toInt() + _eeprom.getServoTrim(16);
            _servo.writeGPIO12(gpio12Pwm);
        }
    }

    if (gpid != "" && gpio == "") {
        gpio = gpid;
    }

    if (gpio != "" && _app.isLowVoltage() == 0) {
        int gpioId = gpio.toInt();
        if (gpioId == 12) {
            String ival = _server.arg("value");
            int gpio12Pwm = ival.toInt() + _eeprom.getServoTrim(16);
            _servo.writeGPIO12(gpio12Pwm);
        }
    }

    _server.send(200, "text/html", "(pm, pms)=(" + pm + "," + pms + ")");
}

void RoboHeroWeb::handleGetEEPROM()
{
    String content = "";
    for (int i = 0; i <= 19; i++) {
        if (i > 0) {
            content += ",";
        }
        content += String(_eeprom.readKeyValue(i));
    }
    _server.send(200, "text/html", content);
}

void RoboHeroWeb::handleOnLineNew()
{
    int handleOnlineTemp[18];
    String m0 = _server.arg("m0");
    String m1 = _server.arg("m1");
    String m2 = _server.arg("m2");
    String m3 = _server.arg("m3");
    String m4 = _server.arg("m4");
    String m5 = _server.arg("m5");
    String m6 = _server.arg("m6");
    String m7 = _server.arg("m7");
    String m8 = _server.arg("m8");
    String m9 = _server.arg("m9");
    String m10 = _server.arg("m10");
    String m11 = _server.arg("m11");
    String m12 = _server.arg("m12");
    String m13 = _server.arg("m13");
    String m14 = _server.arg("m14");
    String m15 = _server.arg("m15");
    String m16 = _server.arg("m16");
    String t1 = _server.arg("t1");

    handleOnlineTemp[0] = m0.toInt();
    handleOnlineTemp[1] = m1.toInt();
    handleOnlineTemp[2] = m2.toInt();
    handleOnlineTemp[3] = m3.toInt();
    handleOnlineTemp[4] = m4.toInt();
    handleOnlineTemp[5] = m5.toInt();
    handleOnlineTemp[6] = m6.toInt();
    handleOnlineTemp[7] = m7.toInt();
    handleOnlineTemp[8] = m8.toInt();
    handleOnlineTemp[9] = m9.toInt();
    handleOnlineTemp[10] = m10.toInt();
    handleOnlineTemp[11] = m11.toInt();
    handleOnlineTemp[12] = m12.toInt();
    handleOnlineTemp[13] = m13.toInt();
    handleOnlineTemp[14] = m14.toInt();
    handleOnlineTemp[15] = m15.toInt();
    handleOnlineTemp[16] = m16.toInt();
    handleOnlineTemp[17] = t1.toInt();

    Serial.print("online [");
    for (int i = 0; i < 18; i++) {
        Serial.print(handleOnlineTemp[i]);
        Serial.print(",");
    }
    Serial.println("]");

    _servo.push(handleOnlineTemp);
    _server.send(200, "", "");
}

void RoboHeroWeb::handleOnLine()
{
    String m0 = _server.arg("m0");
    String m1 = _server.arg("m1");
    String m2 = _server.arg("m2");
    String m3 = _server.arg("m3");
    String m4 = _server.arg("m4");
    String m5 = _server.arg("m5");
    String m6 = _server.arg("m6");
    String m7 = _server.arg("m7");
    String m8 = _server.arg("m8");
    String m9 = _server.arg("m9");
    String m10 = _server.arg("m10");
    String m11 = _server.arg("m11");
    String m12 = _server.arg("m12");
    String m13 = _server.arg("m13");
    String m14 = _server.arg("m14");
    String m15 = _server.arg("m15");
    String m16 = _server.arg("m16");
    String t1 = _server.arg("t1");

    int Servo_Prg_tmp[][18] = {
        { m0.toInt(), m1.toInt(), m2.toInt(), m3.toInt(), m4.toInt(), m5.toInt(),
          m6.toInt(), m7.toInt(), m8.toInt(), m9.toInt(), m10.toInt(), m11.toInt(),
          m12.toInt(), m13.toInt(), m14.toInt(), m15.toInt(), m16.toInt(), t1.toInt(), }
    };

    _server.send(200);

    if (_app.isLowVoltage() == 0) {
        _servo.programRun(Servo_Prg_tmp, 1);
    }
}

void RoboHeroWeb::handleZero()
{
    String content = "";
    content += "<html><head><title>RoboHero Zero Check</title>";
    content += "<style type=\"text/css\">";
    content += "body { color: white; background-color: #000000; }";
    content += ".pm_btn { width: 160px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px 10px 20px; text-decoration: none; }";
    content += ".pm_text { width: 160px; border-radius: 5px; font-family: Arial; font-size: 24px; padding: 10px 20px 10px 20px; text-decoration: none; }";
    content += ".pm_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += ".pms_btn { width: 240px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px 10px 20px; text-decoration: none; }";
    content += ".pms_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += "</style></head><body>";
    content += "<table><tr><td></td><td><button class=\"pm_btn\" style=\"background: #6e6e6e;\" type=\"button\" onclick=\"controlGpid(12, 90)\">GPIO 12</button></td></tr></table><br>";
    content += "<table><tr><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(8,135)\">Servo 8</button></td><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(7,135)\">Servo 7</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(9,135)\">Servo 9</button></td><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(6,135)\">Servo 6</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(10,135)\">Servo 10</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(5, 135)\">Servo 5</button></td></tr></table><br>";
    content += "<table><tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(11,135)\">Servo 11</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(4,135)\">Servo 4</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(12,135)\">Servo 12</button></td><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(3,135)\">Servo 3</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(13,135)\">Servo 13</button></td><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(2,135)\">Servo 2</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(14,135)\">Servo 14</button></td><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(1,135)\">Servo 1</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(15,135)\">Servo 15</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(0,135)\">Servo 0</button></td></tr></table><br>";
    content += "<table><tr><td><button class=\"pm_btn\" style=\"background: #ed3db5;\" type=\"button\" onclick=\"controlPm(100)\">ALL</button></td></tr></table><br>";
    content += "</body><script>";
    content += "function controlServo(id, value) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?servo=\"+id+\"&value=\"+value, true); xhttp.send(); }";
    content += "function controlGpid(id, value) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?gpid=\"+id+\"&value=\"+value, true); xhttp.send(); }";
    content += "function controlPm(value) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?pm=\"+value, true); xhttp.send(); }";
    content += "</script></html>";

    _server.send(200, "text/html", content);
}

void RoboHeroWeb::handleEditor()
{
    String content = "";
    content += "<html><head><title>RoboHero Motion Editor</title>";
    content += "<style type=\"text/css\">";
    content += "body { color: white; background-color: #000000; }";
    content += ".pm_btn { width: 160px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px; text-decoration: none; }";
    content += ".pm_text { width: 160px; border-radius: 5px; font-family: Arial; font-size: 24px; padding: 10px 20px; text-decoration: none; }";
    content += ".pm_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += ".pms_btn { width: 240px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px; text-decoration: none; }";
    content += ".pms_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += "</style></head><body>";
    content += "<table><tr><td></td><td><button class=\"pm_btn\" style=\"background: #6e6e6e;\" type=\"button\" onclick=\"controlGpid(12, 90)\">GPIO 12</button></td></tr></table><br>";
    content += "<table><tr><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(8,135)\">Servo 8</button></td><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(7,135)\">Servo 7</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(9,135)\">Servo 9</button></td><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(6,135)\">Servo 6</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(10,135)\">Servo 10</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(5, 135)\">Servo 5</button></td></tr></table><br>";
    content += "<table><tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(11,135)\">Servo 11</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(4,135)\">Servo 4</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(12,135)\">Servo 12</button></td><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(3,135)\">Servo 3</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(13,135)\">Servo 13</button></td><td><button class=\"pm_btn\" style=\"background: #f5da81;\" type=\"button\" onclick=\"controlServo(2,135)\">Servo 2</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(14,135)\">Servo 14</button></td><td><button class=\"pm_btn\" style=\"background: #bdbdbd;\" type=\"button\" onclick=\"controlServo(1,135)\">Servo 1</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(15,135)\">Servo 15</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlServo(0,135)\">Servo 0</button></td></tr></table><br>";
    content += "</body><script>";
    content += "function controlServo(id, value) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?servo=\"+id+\"&value=\"+value, true); xhttp.send(); }";
    content += "function controlGpid(id, value) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?gpid=\"+id+\"&value=\"+value, true); xhttp.send(); }";
    content += "</script></html>";

    _server.send(200, "text/html", content);
}

void RoboHeroWeb::handleSetting()
{
    String content = "";
    content += "<html><head><title>RoboHero Setting</title>";
    content += "<style type=\"text/css\">";
    content += "body { color: white; background-color: #000000; }";
    content += ".pm_btn { width: 120px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px; text-decoration: none; }";
    content += ".pm_text { width: 80px; border-radius: 5px; font-family: Arial; font-size: 24px; padding: 10px 20px; text-decoration: none; }";
    content += ".pm_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += ".pms_btn { width: 160px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px; text-decoration: none; }";
    content += ".pms_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += "</style></head><body>";
    content += "<table><tr><td></td><td>GPIO 12<br/><input class=\"pm_text\" type=\"text\" id=\"servo_16\" value=\"" + String(_servo.readKeyValue(16)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(16,'servo_16')\">SET</button></td></tr>";
    content += "<tr><td>Servo 8<br/><input class=\"pm_text\" type=\"text\" id=\"servo_8\" value=\"" + String(_servo.readKeyValue(8)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(8,'servo_8')\">SET</button></td><td>Servo 7<br/><input class=\"pm_text\" type=\"text\" id=\"servo_7\" value=\"" + String(_servo.readKeyValue(7)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(7,'servo_7')\">SET</button></td></tr>";
    content += "<tr><td>Servo 9<br/><input class=\"pm_text\" type=\"text\" id=\"servo_9\" value=\"" + String(_servo.readKeyValue(9)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(9,'servo_9')\">SET</button></td><td>Servo 6<br/><input class=\"pm_text\" type=\"text\" id=\"servo_6\" value=\"" + String(_servo.readKeyValue(6)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(6,'servo_6')\">SET</button></td></tr>";
    content += "<tr><td>Servo 10<br/><input class=\"pm_text\" type=\"text\" id=\"servo_10\" value=\"" + String(_servo.readKeyValue(10)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(10,'servo_10')\">SET</button></td><td>Servo 5<br/><input class=\"pm_text\" type=\"text\" id=\"servo_5\" value=\"" + String(_servo.readKeyValue(5)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(5,'servo_5')\">SET</button></td></tr>";
    content += "<tr><td>Servo 11<br/><input class=\"pm_text\" type=\"text\" id=\"servo_11\" value=\"" + String(_servo.readKeyValue(11)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(11,'servo_11')\">SET</button></td><td>Servo 4<br/><input class=\"pm_text\" type=\"text\" id=\"servo_4\" value=\"" + String(_servo.readKeyValue(4)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(4,'servo_4')\">SET</button></td></tr>";
    content += "<tr><td>Servo 12<br/><input class=\"pm_text\" type=\"text\" id=\"servo_12\" value=\"" + String(_servo.readKeyValue(12)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(12,'servo_12')\">SET</button></td><td>Servo 3<br/><input class=\"pm_text\" type=\"text\" id=\"servo_3\" value=\"" + String(_servo.readKeyValue(3)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(3,'servo_3')\">SET</button></td></tr>";
    content += "<tr><td>Servo 13<br/><input class=\"pm_text\" type=\"text\" id=\"servo_13\" value=\"" + String(_servo.readKeyValue(13)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(13,'servo_13')\">SET</button></td><td>Servo 2<br/><input class=\"pm_text\" type=\"text\" id=\"servo_2\" value=\"" + String(_servo.readKeyValue(2)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(2,'servo_2')\">SET</button></td></tr>";
    content += "<tr><td>Servo 14<br/><input class=\"pm_text\" type=\"text\" id=\"servo_14\" value=\"" + String(_servo.readKeyValue(14)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(14,'servo_14')\">SET</button></td><td>Servo 1<br/><input class=\"pm_text\" type=\"text\" id=\"servo_1\" value=\"" + String(_servo.readKeyValue(1)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(1,'servo_1')\">SET</button></td></tr>";
    content += "<tr><td>Servo 15<br/><input class=\"pm_text\" type=\"text\" id=\"servo_15\" value=\"" + String(_servo.readKeyValue(15)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(15,'servo_15')\">SET</button></td><td>Servo 0<br/><input class=\"pm_text\" type=\"text\" id=\"servo_0\" value=\"" + String(_servo.readKeyValue(0)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(0,'servo_0')\">SET</button></td></tr></table><br>";
    content += "<table><tr><td>PWM Frequency Calibration<br/><input class=\"pm_text\" type=\"text\" id=\"servo_18\" value=\"" + String(_servo.readKeyValue(18)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(18,'servo_18')\">SET</button></td><td>Voltage Calibration<br/><input class=\"pm_text\" type=\"text\" id=\"servo_19\" value=\"" + String(_servo.readKeyValue(19)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(19,'servo_19')\">SET</button></td></tr>";
    content += "<tr><td>Delay Time<br/><input class=\"pm_text\" type=\"text\" id=\"servo_17\" value=\"" + String(_servo.readKeyValue(17)) + "\"><button class=\"pm_btn\" type=\"button\" onclick=\"saveServo(17,'servo_17')\">SET</button></td></tr></table><br><br>";
    content += "<table><tr><td><button class=\"pm_btn\" style=\"background: #ed3db5;\" type=\"button\" onclick=\"saveServo(100, 0)\">RESET</button></td></tr></table><br>";
    content += "</body><script>";
    content += "function saveServo(id, textId) { var xhttp = new XMLHttpRequest(); var value = \"0\"; if(id==100){ document.getElementById(\"servo_17\").value = \"0\"; document.getElementById(\"servo_16\").value = \"0\"; document.getElementById(\"servo_15\").value = \"0\"; document.getElementById(\"servo_14\").value = \"0\"; document.getElementById(\"servo_13\").value = \"0\"; document.getElementById(\"servo_12\").value = \"0\"; document.getElementById(\"servo_11\").value = \"0\"; document.getElementById(\"servo_10\").value = \"0\"; document.getElementById(\"servo_9\").value = \"0\"; document.getElementById(\"servo_8\").value = \"0\"; document.getElementById(\"servo_7\").value = \"0\"; document.getElementById(\"servo_6\").value = \"0\"; document.getElementById(\"servo_5\").value = \"0\"; document.getElementById(\"servo_4\").value = \"0\"; document.getElementById(\"servo_3\").value = \"0\"; document.getElementById(\"servo_2\").value = \"0\"; document.getElementById(\"servo_1\").value = \"0\"; document.getElementById(\"servo_0\").value = \"0\"; } else { value = document.getElementById(textId).value; } xhttp.open(\"GET\",\"save?key=\"+id+\"&value=\"+value, true); xhttp.send(); }";
    content += "</script></html>";

    _server.send(200, "text/html", content);
}

void RoboHeroWeb::handleIndex()
{
    String content = "";
    content += "<html><head><title>RoboHero Controller</title>";
    content += "<style type=\"text/css\">";
    content += "body { color: white; background-color: #000000; }";
    content += ".pm_btn { width: 160px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px; text-decoration: none; }";
    content += ".pm_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += ".pms_btn { width: 240px; border-radius: 5px; font-family: Arial; color: #ffffff; font-size: 24px; background: #3498db; padding: 10px 20px; text-decoration: none; }";
    content += ".pms_btn:hover { background: #3cb0fd; text-decoration: none; }";
    content += "</style></head><body>";
    content += "<table><tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlPm(3)\">TurnLeft</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlPm(1)\">Forward</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlPm(4)\">TurnRight</button></td></tr>";
    content += "<tr><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlPm(5)\">MoveLeft</button></td><td><button class=\"pm_btn\" style=\"background: #ed3db5;\" type=\"button\" onclick=\"controlPm(99)\">STANDBY</button></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlPm(6)\">MoveRight</button></td></tr>";
    content += "<tr><td></td><td><button class=\"pm_btn\" type=\"button\" onclick=\"controlPm(2)\">Backward</button></td><td></td></tr></table>";
    content += "<table><tr><td><button class=\"pms_btn\" type=\"button\" onclick=\"controlPm(11)\">Get Up</button></td><td><button class=\"pms_btn\" type=\"button\" onclick=\"controlPm(12)\">FaceDownGetUp</button></td></tr></table>";
    content += "<table><tr><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(1)\">Bow</button></td><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(4)\">Apache</button></td></tr>";
    content += "<tr><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(2)\">Waving</button></td><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(5)\">Balance</button></td></tr>";
    content += "<tr><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(3)\">Iron Man</button></td><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(6)\">Warm-Up</button></td></tr>";
    content += "<tr><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(7)\">Clap Hands</button></td><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(8)\">GOILC</button></td></tr>";
    content += "<tr><td><button class=\"pms_btn\" style=\"background: #ffbf00;\" type=\"button\" onclick=\"controlPms(9)\">Dance</button></td><td></td></tr>";
    content += "<tr><td colspan=\"2\"><center><button class=\"pms_btn\" style=\"background: #04b404;\" type=\"button\" onclick=\"controlPms(99)\">Auto</button></center></td></tr></table>";
    content += "<table><tr><td>\"" + String(FW_VERSION_STRING) + "\"</td></tr></table>";
    content += "</body><script>";
    content += "function controlPm(id) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?pm=\"+id, true); xhttp.send(); }";
    content += "function controlPms(id) { var xhttp = new XMLHttpRequest(); xhttp.open(\"GET\", \"controller?pms=\"+id, true); xhttp.send(); }";
    content += "</script></html>";

    _server.send(200, "text/html", content);
}

void RoboHeroWeb::handleInfo()
{
    String response = "{";
    response += "\"ver\": 2.29, ";
    response += "\"type\": \"robohero\", ";
    response += "\"low\": " + String(_app.isLowVoltage()) + ", ";
    response += "\"volt\": " + String(_app.getVoltage()) + ", ";
    response += "\"msg\": \"Robohero firmware\" ";
    response += "}";
    _server.send(200, "text/html", response);
}

void RoboHeroWeb::handleReset()
{
    _app.resetLowVoltage();
    _server.send(200, "text/html", "{\"ret\": \"ok\" }");
}

void RoboHeroWeb::handleRestart()
{
    ESP.restart();
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
