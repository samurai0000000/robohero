/*
 * Mqtt.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Config.hxx"
#include "Mqtt.hxx"
#include "RoboHero.hxx"
#include "Servo.hxx"
#include "Store.hxx"
#include "robohero/msg.h"

static const char *TAG = "mqtt";

#define MQTT_STATUS_BUF 256

Mqtt Mqtt::_self;

Mqtt &Mqtt::instance()
{
    return _self;
}

Mqtt::Mqtt()
    : _client(NULL), _running(false), _connected(false), _relaxed(false),
      _txCount(0), _rxCount(0), _dropped(0), _voltPending(-1), _pubTask(NULL)
{
    for (int i = 0; i < ALLSERVOS; i++) {
        _pwmPending[i] = -1;
    }
    _clientId[0] = '\0';
    _cleanId[0] = '\0';
    _uri[0] = '\0';
    _statusTopic[0] = '\0';
    _controlTopic[0] = '\0';
    _availTopic[0] = '\0';
    _stateTopic[0] = '\0';
    _cmdTopic[0] = '\0';
    _headTopic[0] = '\0';
}

Mqtt::~Mqtt()
{
    stop();
}

bool Mqtt::isRunning() const
{
    return _running;
}

bool Mqtt::isConnected() const
{
    return _connected;
}

unsigned Mqtt::txCount() const
{
    return _txCount;
}

unsigned Mqtt::rxCount() const
{
    return _rxCount;
}

unsigned Mqtt::droppedCount() const
{
    return _dropped;
}

int Mqtt::getLowCutoff() const
{
    return INPUT_MIN_VOLTAGE;
}

int Mqtt::getHighCutoff() const
{
    return INPUT_RECOVER_VOLTAGE;
}

void Mqtt::rebuildTopics()
{
    size_t j = 0;
    for (size_t i = 0; _clientId[i] != '\0' && j < sizeof(_cleanId) - 1; i++) {
        char c = _clientId[i];
        if (isalnum((unsigned char) c)) {
            _cleanId[j++] = (char) tolower((unsigned char) c);
        } else {
            _cleanId[j++] = '_';
        }
    }
    _cleanId[j] = '\0';
    if (_cleanId[0] == '\0') {
        strncpy(_cleanId, "robohero", sizeof(_cleanId) - 1);
        _cleanId[sizeof(_cleanId) - 1] = '\0';
    }

    snprintf(_statusTopic,
             sizeof(_statusTopic),
             "robot/robohero/%s/status",
             _clientId);
    snprintf(_controlTopic,
             sizeof(_controlTopic),
             "robot/robohero/%s/control",
             _clientId);
    snprintf(_availTopic,
             sizeof(_availTopic),
             "robot/robohero/%s/availability",
             _clientId);
    snprintf(_stateTopic,
             sizeof(_stateTopic),
             "robot/robohero/%s/state",
             _clientId);
    snprintf(_cmdTopic,
             sizeof(_cmdTopic),
             "robot/robohero/%s/cmd",
             _clientId);
    snprintf(_headTopic,
             sizeof(_headTopic),
             "robot/robohero/%s/head/set",
             _clientId);
}

bool Mqtt::publishStatus(const void *payload, int len)
{
    if (!_connected || !_client || payload == NULL || len <= 0) {
        return false;
    }
    int msgId = esp_mqtt_client_publish(_client,
                                        _statusTopic,
                                        (const char *) payload,
                                        len,
                                        0,
                                        0);
    if (msgId < 0) {
        return false;
    }
    _txCount++;
    return true;
}

void Mqtt::clearPending()
{
    for (int i = 0; i < ALLSERVOS; i++) {
        _pwmPending[i] = -1;
    }
    _voltPending = -1;
}

bool Mqtt::enqueuePending(int16_t *slot, int16_t v)
{
    if (!_running || slot == NULL) {
        return false;
    }
    taskENTER_CRITICAL();
    if (*slot != -1) {
        _dropped++;
    }
    *slot = v;
    taskEXIT_CRITICAL();
    if (_pubTask) {
        xTaskNotifyGive((TaskHandle_t) _pubTask);
    }
    return true;
}

bool Mqtt::sendPwmPos(int chan, int pos)
{
    if (chan < 0 || chan >= ALLSERVOS) {
        return false;
    }
    return enqueuePending(&_pwmPending[chan], (int16_t) pos);
}

bool Mqtt::sendVoltage(int voltage)
{
    return enqueuePending(&_voltPending, (int16_t) voltage);
}

bool Mqtt::publishJsonState(int volt, bool lowVoltage, bool moving)
{
    if (!_connected || !_client || !_stateTopic[0]) {
        return false;
    }
    if (volt < 0) {
        volt = RoboHero::instance().getVoltage();
    }
    if (volt < 0) {
        volt = 0;
    }
    int v_int = volt / 100;
    int v_dec = volt % 100;
    int pct = (volt - 590) * 100 / (840 - 590);
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }

    const char *motion = "idle";
    if (lowVoltage) {
        motion = "voltage_low";
    } else if (moving) {
        motion = "moving";
    } else if (_relaxed) {
        motion = "relaxed";
    }

    char buf[128];
    snprintf(buf,
             sizeof(buf),
             "{\"voltage\":%d.%02d,\"battery\":%d,\"low_voltage\":%s,\"motion\":\"%s\"}",
             v_int,
             v_dec,
             pct,
             lowVoltage ? "true" : "false",
             motion);

    int msgId = esp_mqtt_client_publish(_client,
                                        _stateTopic,
                                        buf,
                                        strlen(buf),
                                        0,
                                        1);
    return (msgId >= 0);
}

void Mqtt::publishHaEntity(const char *component,
                           const char *objectId,
                           const char *jsonConfig)
{
    if (!_connected || !_client || !component || !objectId || !jsonConfig) {
        return;
    }
    char topic[128];
    snprintf(topic,
             sizeof(topic),
             "homeassistant/%s/%s/%s/config",
             component,
             _cleanId,
             objectId);
    esp_mqtt_client_publish(_client,
                            topic,
                            jsonConfig,
                            strlen(jsonConfig),
                            0,
                            1);
}

struct HaBtnDef {
    const char *objId;
    const char *name;
    const char *cmd;
    const char *icon;
};

static const HaBtnDef HA_BUTTONS[] = {
    { "stop", "Emergency Stop", "stop", "mdi:stop-circle" },
    { "standby", "Standby", "standby", "mdi:human" },
    { "relax", "Relax", "relax", "mdi:power-sleep" },
    { "zero", "Zero Pose", "zero", "mdi:crosshairs-gps" },
    { "forward", "Forward", "forward", "mdi:arrow-up-bold" },
    { "backward", "Backward", "backward", "mdi:arrow-down-bold" },
    { "turn_left", "Turn Left", "turn_left", "mdi:rotate-left" },
    { "turn_right", "Turn Right", "turn_right", "mdi:rotate-right" },
    { "sidestep_left", "Move Left", "move_left", "mdi:arrow-left-bold" },
    { "sidestep_right", "Move Right", "move_right", "mdi:arrow-right-bold" },
    { "get_up_back", "Get Up (Back)", "get_up", "mdi:human-handsup" },
    { "get_up_front", "Face-Down Get Up", "get_up_face", "mdi:human-handsdown" },
    { "wave", "Wave", "wave", "mdi:hand-wave" },
    { "bow", "Bow", "bow", "mdi:human-greeting" },
    { "dance", "Dance", "dance", "mdi:music" },
    { "iron_man", "Iron Man", "iron_man", "mdi:shield-star" },
    { "clap", "Clap", "clap", "mdi:hand-clap" },
    { "warmup", "Warm-Up", "warmup", "mdi:run" },
    { "apache", "Apache", "apache", "mdi:karate" },
    { "balance", "Balance", "balance", "mdi:scale-balance" },
    { "goilc", "GOILC", "goilc", "mdi:robot-happy" },
    { "auto", "Auto Demo Loop", "auto", "mdi:play-circle-outline" },
};

void Mqtt::publishHaDiscovery()
{
    if (!_connected || !_client) {
        return;
    }

    char devJson[128];
    snprintf(devJson,
             sizeof(devJson),
             "{\"ids\":[\"robohero_%s\"],\"name\":\"RoboHero\",\"mf\":\"TT-Robotix\",\"mdl\":\"RoboHero 17DOF\"}",
             _cleanId);

    char payload[600];

    // Buttons
    for (size_t i = 0; i < sizeof(HA_BUTTONS) / sizeof(HA_BUTTONS[0]); i++) {
        const HaBtnDef &b = HA_BUTTONS[i];
        snprintf(payload,
                 sizeof(payload),
                 "{\"name\":\"%s\",\"obj_id\":\"%s\",\"cmd_t\":\"%s\","
                 "\"payload_press\":\"%s\",\"icon\":\"%s\","
                 "\"uniq_id\":\"robohero_%s_%s\",\"avty_t\":\"%s\",\"dev\":%s}",
                 b.name,
                 b.objId,
                 _cmdTopic,
                 b.cmd,
                 b.icon,
                 _cleanId,
                 b.objId,
                 _availTopic,
                 devJson);
        publishHaEntity("button", b.objId, payload);
    }

    // Battery Voltage Sensor
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Battery Voltage\",\"obj_id\":\"battery_voltage\","
             "\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.voltage }}\","
             "\"unit_of_meas\":\"V\",\"dev_cla\":\"voltage\",\"stat_cla\":\"measurement\","
             "\"uniq_id\":\"robohero_%s_battery_voltage\",\"avty_t\":\"%s\",\"dev\":%s}",
             _stateTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("sensor", "battery_voltage", payload);

    // Battery Level Sensor
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Battery Level\",\"obj_id\":\"battery_level\","
             "\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.battery }}\","
             "\"unit_of_meas\":\"%%\",\"dev_cla\":\"battery\",\"stat_cla\":\"measurement\","
             "\"uniq_id\":\"robohero_%s_battery_level\",\"avty_t\":\"%s\",\"dev\":%s}",
             _stateTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("sensor", "battery_level", payload);

    // Motion State Sensor
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Motion Status\",\"obj_id\":\"motion_state\","
             "\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.motion }}\","
             "\"icon\":\"mdi:robot\","
             "\"uniq_id\":\"robohero_%s_motion_state\",\"avty_t\":\"%s\",\"dev\":%s}",
             _stateTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("sensor", "motion_state", payload);

    // Low Voltage Binary Sensor
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Low Voltage Alert\",\"obj_id\":\"low_voltage\","
             "\"stat_t\":\"%s\",\"val_tpl\":\"{{ 'ON' if value_json.low_voltage else 'OFF' }}\","
             "\"dev_cla\":\"problem\","
             "\"uniq_id\":\"robohero_%s_low_voltage\",\"avty_t\":\"%s\",\"dev\":%s}",
             _stateTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("binary_sensor", "low_voltage", payload);

    // Connectivity Binary Sensor
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Connectivity\",\"obj_id\":\"connectivity\","
             "\"stat_t\":\"%s\",\"payload_on\":\"online\",\"payload_off\":\"offline\","
             "\"dev_cla\":\"connectivity\","
             "\"uniq_id\":\"robohero_%s_connectivity\",\"dev\":%s}",
             _availTopic,
             _cleanId,
             devJson);
    publishHaEntity("binary_sensor", "connectivity", payload);

    // Select Action Dropdown
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Select Motion Program\",\"obj_id\":\"action\","
             "\"cmd_t\":\"%s\","
             "\"options\":[\"standby\",\"relax\",\"zero\",\"wave\",\"bow\",\"dance\",\"iron_man\",\"clap\",\"warmup\",\"balance\",\"apache\",\"auto\"],"
             "\"icon\":\"mdi:animation-play\","
             "\"uniq_id\":\"robohero_%s_action\",\"avty_t\":\"%s\",\"dev\":%s}",
             _cmdTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("select", "action", payload);

    // Head Pan Slider
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Head Pan Angle\",\"obj_id\":\"head_pan\","
             "\"cmd_t\":\"%s\",\"min\":-90,\"max\":90,\"step\":5,"
             "\"unit_of_meas\":\"°\",\"icon\":\"mdi:head\","
             "\"uniq_id\":\"robohero_%s_head_pan\",\"avty_t\":\"%s\",\"dev\":%s}",
             _headTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("number", "head_pan", payload);

    // Power Switch (Exposes RoboHero as an on/off device to Home Assistant and Google Home)
    snprintf(payload,
             sizeof(payload),
             "{\"name\":\"Power\",\"obj_id\":\"power\","
             "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
             "\"val_tpl\":\"{{ 'OFF' if value_json.motion == 'relaxed' else 'ON' }}\","
             "\"payload_on\":\"standby\",\"payload_off\":\"relax\","
             "\"icon\":\"mdi:robot\","
             "\"uniq_id\":\"robohero_%s_power\",\"avty_t\":\"%s\",\"dev\":%s}",
             _cmdTopic,
             _stateTopic,
             _cleanId,
             _availTopic,
             devJson);
    publishHaEntity("switch", "power", payload);
}

void Mqtt::pubTask(void *arg)
{
    (void) arg;
    Mqtt &m = instance();
    uint8_t buf[MQTT_STATUS_BUF];
    int16_t pwm[ALLSERVOS];
    int16_t volt = -1;
    bool havePwm[ALLSERVOS];
    TickType_t lastJsonPub = 0;

    while (m._running) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        if (!m._running) {
            break;
        }

        taskENTER_CRITICAL();
        volt = m._voltPending;
        m._voltPending = -1;
        for (int i = 0; i < ALLSERVOS; i++) {
            pwm[i] = m._pwmPending[i];
            m._pwmPending[i] = -1;
        }
        taskEXIT_CRITICAL();

        bool haveVolt = (volt != -1);
        TickType_t now = xTaskGetTickCount();
        if (haveVolt || (now - lastJsonPub >= pdMS_TO_TICKS(5000))) {
            lastJsonPub = now;
            m.publishJsonState(volt,
                               RoboHero::instance().isLowVoltage(),
                               RoboHero::instance().isBusy());
        }

        bool any = haveVolt;
        for (int i = 0; i < ALLSERVOS; i++) {
            havePwm[i] = (pwm[i] != -1);
            if (havePwm[i]) {
                any = true;
            }
        }
        if (!any) {
            continue;
        }

        int n = rh_msg_begin(buf, sizeof(buf), RH_MSG_STATUS);
        if (n < 0) {
            continue;
        }

        uint32_t ts = (uint32_t) (esp_timer_get_time() / 1000);
        n = rh_tlv_put(buf, sizeof(buf), n, RH_TLV_TIME, &ts, sizeof(ts));
        if (n < 0) {
            continue;
        }
        if (haveVolt) {
            n = rh_tlv_put(buf,
                           sizeof(buf),
                           n,
                           RH_TLV_VOLTAGE,
                           &volt,
                           sizeof(volt));
            if (n < 0) {
                continue;
            }
        }
        for (int i = 0; i < ALLSERVOS; i++) {
            uint8_t raw[3];

            if (!havePwm[i]) {
                continue;
            }
            raw[0] = (uint8_t) i;
            memcpy(raw + 1, &pwm[i], sizeof(pwm[i]));
            n = rh_tlv_put(
                buf, sizeof(buf), n, RH_TLV_SERVO, raw, sizeof(raw));
            if (n < 0) {
                break;
            }
        }
        if (n < 0) {
            continue;
        }
        n = rh_msg_finish(buf, n);
        if (n < 0) {
            continue;
        }
        m.publishStatus(buf, n);
    }
    m._pubTask = NULL;
    vTaskDelete(NULL);
}

bool Mqtt::setPwmPos(int chan, int pos)
{
    Servo::instance().setPwm(chan, pos);
    Servo::instance().setRunningPos(chan, pos);
    return true;
}

void Mqtt::onHaCommand(const void *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    char cmd[64];
    if (len >= (int) sizeof(cmd)) {
        len = sizeof(cmd) - 1;
    }
    memcpy(cmd, data, len);
    cmd[len] = '\0';

    char *p = cmd;
    while (*p && isspace((unsigned char) *p)) p++;
    int end = strlen(p);
    while (end > 0 && isspace((unsigned char) p[end - 1])) {
        p[--end] = '\0';
    }

    RoboHero &rh = RoboHero::instance();

    if (strcmp(p, "forward") == 0) {
        _relaxed = false;
        rh.submitPm(1);
    } else if (strcmp(p, "backward") == 0) {
        _relaxed = false;
        rh.submitPm(2);
    } else if (strcmp(p, "turn_left") == 0) {
        _relaxed = false;
        rh.submitPm(3);
    } else if (strcmp(p, "turn_right") == 0) {
        _relaxed = false;
        rh.submitPm(4);
    } else if (strcmp(p, "move_left") == 0 || strcmp(p, "sidestep_left") == 0) {
        _relaxed = false;
        rh.submitPm(5);
    } else if (strcmp(p, "move_right") == 0 || strcmp(p, "sidestep_right") == 0) {
        _relaxed = false;
        rh.submitPm(6);
    } else if (strcmp(p, "get_up") == 0) {
        _relaxed = false;
        rh.submitPm(11);
    } else if (strcmp(p, "get_up_face") == 0) {
        _relaxed = false;
        rh.submitPm(12);
    } else if (strcmp(p, "standby") == 0 || strcmp(p, "center") == 0) {
        _relaxed = false;
        rh.submitCenter();
    } else if (strcmp(p, "stop") == 0) {
        rh.requestStop();
    } else if (strcmp(p, "relax") == 0) {
        _relaxed = true;
        rh.submitRelax();
    } else if (strcmp(p, "zero") == 0) {
        _relaxed = false;
        rh.submitZero();
    } else if (strcmp(p, "bow") == 0) {
        _relaxed = false;
        rh.submitPms(1);
    } else if (strcmp(p, "wave") == 0) {
        _relaxed = false;
        rh.submitPms(2);
    } else if (strcmp(p, "iron_man") == 0) {
        _relaxed = false;
        rh.submitPms(3);
    } else if (strcmp(p, "apache") == 0) {
        _relaxed = false;
        rh.submitPms(4);
    } else if (strcmp(p, "balance") == 0) {
        _relaxed = false;
        rh.submitPms(5);
    } else if (strcmp(p, "warmup") == 0) {
        _relaxed = false;
        rh.submitPms(6);
    } else if (strcmp(p, "clap") == 0) {
        _relaxed = false;
        rh.submitPms(7);
    } else if (strcmp(p, "goilc") == 0) {
        _relaxed = false;
        rh.submitPms(8);
    } else if (strcmp(p, "dance") == 0) {
        _relaxed = false;
        rh.submitPms(9);
    } else if (strcmp(p, "auto") == 0) {
        _relaxed = false;
        rh.submitPms(99);
    } else if (strncmp(p, "pm ", 3) == 0) {
        _relaxed = false;
        int id = atoi(p + 3);
        rh.submitPm(id);
    } else if (strncmp(p, "pms ", 4) == 0) {
        _relaxed = false;
        int id = atoi(p + 4);
        rh.submitPms(id);
    }

    publishJsonState(-1, rh.isLowVoltage(), rh.isBusy());
}

void Mqtt::onHaHead(const void *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    char valStr[32];
    if (len >= (int) sizeof(valStr)) {
        len = sizeof(valStr) - 1;
    }
    memcpy(valStr, data, len);
    valStr[len] = '\0';

    int angle = atoi(valStr);
    int deg = angle;
    if (angle >= -90 && angle <= 90) {
        deg = angle + 90;
    }
    if (deg < 0) {
        deg = 0;
    }
    if (deg > 180) {
        deg = 180;
    }
    Servo::instance().writeGpio12(deg);
}

void Mqtt::onControl(const void *data, int len)
{
    rh_msg_view view;
    RoboHero &rh = RoboHero::instance();
    int off;

    if (rh_msg_parse(data, len, &view) != 0) {
        return;
    }

    switch (view.msg_type) {
    case RH_MSG_STOP:
        rh.requestStop();
        return;
    case RH_MSG_CENTER:
        rh.submitCenter();
        return;
    case RH_MSG_ZERO:
        rh.submitZero();
        return;
    case RH_MSG_RELAX:
        rh.submitRelax();
        return;
    default:
        break;
    }

    off = 0;
    while (off < (int) view.payload_len) {
        uint8_t type;
        uint8_t n;
        const uint8_t *val;
        int next = rh_tlv_next(
            view.payload, view.payload_len, off, &type, &val, &n);
        if (next < 0) {
            return;
        }
        if (view.msg_type == RH_MSG_PM && type == RH_TLV_PROG &&
            n == sizeof(int16_t)) {
            int16_t id;
            memcpy(&id, val, sizeof(id));
            rh.submitPm((int) id);
            return;
        }
        if (view.msg_type == RH_MSG_PMS && type == RH_TLV_PROG &&
            n == sizeof(int16_t)) {
            int16_t id;
            memcpy(&id, val, sizeof(id));
            rh.submitPms((int) id);
            return;
        }
        if (view.msg_type == RH_MSG_SET_PWM && type == RH_TLV_SERVO &&
            n == sizeof(rh_tlv_servo)) {
            rh_tlv_servo s;
            memcpy(&s, val, sizeof(s));
            setPwmPos((int) s.chan, (int) s.pos);
            return;
        }
        off = next;
    }
}

esp_err_t Mqtt::eventHandler(esp_mqtt_event_handle_t event)
{
    Mqtt &m = instance();

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        m._connected = true;
        if (m._availTopic[0]) {
            esp_mqtt_client_publish(event->client,
                                    m._availTopic,
                                    "online",
                                    6,
                                    1,
                                    1);
        }
        if (m._controlTopic[0]) {
            esp_mqtt_client_subscribe(event->client, m._controlTopic, 0);
        }
        if (m._cmdTopic[0]) {
            esp_mqtt_client_subscribe(event->client, m._cmdTopic, 0);
        }
        if (m._headTopic[0]) {
            esp_mqtt_client_subscribe(event->client, m._headTopic, 0);
        }
        m.publishHaDiscovery();
        m.publishJsonState(RoboHero::instance().getVoltage(),
                           RoboHero::instance().isLowVoltage(),
                           RoboHero::instance().isBusy());
        ESP_LOGI(TAG, "connected, sub %s, %s, %s",
                 m._controlTopic, m._cmdTopic, m._headTopic);
        break;
    case MQTT_EVENT_DISCONNECTED:
        m._connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        m._rxCount++;
        if (event->data && event->data_len > 0 && event->topic && event->topic_len > 0) {
            if ((int) strlen(m._controlTopic) == event->topic_len &&
                strncmp(event->topic, m._controlTopic, event->topic_len) == 0) {
                m.onControl(event->data, event->data_len);
            } else if ((int) strlen(m._cmdTopic) == event->topic_len &&
                       strncmp(event->topic, m._cmdTopic, event->topic_len) == 0) {
                m.onHaCommand(event->data, event->data_len);
            } else if ((int) strlen(m._headTopic) == event->topic_len &&
                       strncmp(event->topic, m._headTopic, event->topic_len) == 0) {
                m.onHaHead(event->data, event->data_len);
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "error");
        break;
    default:
        break;
    }
    return ESP_OK;
}

bool Mqtt::start()
{
    stop();

    Store &st = Store::instance();
    const char *host = st.getMqttHost();
    if (host == NULL || host[0] == '\0') {
        ESP_LOGW(TAG, "MQTT host not set");
        return false;
    }

    _txCount = 0;
    _rxCount = 0;
    _dropped = 0;
    _connected = false;
    clearPending();

    strncpy(_clientId, st.getMqttClientId(), sizeof(_clientId) - 1);
    _clientId[sizeof(_clientId) - 1] = '\0';
    if (_clientId[0] == '\0') {
        uint8_t mac[6];
        memset(mac, 0, sizeof(mac));
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
        snprintf(_clientId, sizeof(_clientId), "TTR-%02x%02x", mac[4], mac[5]);
    }

    snprintf(
        _uri, sizeof(_uri), "mqtt://%s:%u", host, (unsigned) st.getMqttPort());
    rebuildTopics();

    esp_mqtt_client_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.uri = _uri;
    cfg.client_id = _clientId;
    cfg.event_handle = &Mqtt::eventHandler;
    cfg.transport = MQTT_TRANSPORT_OVER_TCP;
    cfg.task_stack = 4096;
    cfg.buffer_size = 1024;
    cfg.lwt_topic = _availTopic;
    cfg.lwt_msg = "offline";
    cfg.lwt_qos = 1;
    cfg.lwt_retain = 1;
    if (st.getMqttUser()[0] != '\0') {
        cfg.username = st.getMqttUser();
        cfg.password = st.getMqttPass();
    }

    _client = esp_mqtt_client_init(&cfg);
    if (_client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return false;
    }
    if (esp_mqtt_client_start(_client) != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start failed");
        esp_mqtt_client_destroy(_client);
        _client = NULL;
        return false;
    }

    _running = true;
    TaskHandle_t handle = NULL;
    if (xTaskCreate(pubTask, "mqttPub", 2048, NULL, 5, &handle) != pdPASS) {
        ESP_LOGE(TAG, "mqttPub task failed");
        _running = false;
        esp_mqtt_client_stop(_client);
        esp_mqtt_client_destroy(_client);
        _client = NULL;
        return false;
    }
    _pubTask = handle;

    ESP_LOGI(TAG, "started %s id %s", _uri, _clientId);
    return true;
}

void Mqtt::stop()
{
    _running = false;
    _connected = false;
    if (_pubTask) {
        xTaskNotifyGive((TaskHandle_t) _pubTask);
    }

    while (_pubTask) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    clearPending();

    if (_client == NULL) {
        return;
    }
    esp_mqtt_client_stop(_client);
    esp_mqtt_client_destroy(_client);
    _client = NULL;
    ESP_LOGI(TAG, "stopped");
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
