/*
 * Mqtt.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <stdio.h>
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
    : _client(NULL), _running(false), _connected(false), _txCount(0),
      _rxCount(0), _dropped(0), _voltPending(-1), _pubTask(NULL)
{
    for (int i = 0; i < ALLSERVOS; i++) {
        _pwmPending[i] = -1;
    }
    _clientId[0] = '\0';
    _uri[0] = '\0';
    _statusTopic[0] = '\0';
    _controlTopic[0] = '\0';
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
    snprintf(_statusTopic,
             sizeof(_statusTopic),
             "robot/robohero/%s/status",
             _clientId);
    snprintf(_controlTopic,
             sizeof(_controlTopic),
             "robot/robohero/%s/control",
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

void Mqtt::pubTask(void *arg)
{
    (void) arg;
    Mqtt &m = instance();
    uint8_t buf[MQTT_STATUS_BUF];
    int16_t pwm[ALLSERVOS];
    int16_t volt = -1;
    bool havePwm[ALLSERVOS];

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
        if (m._controlTopic[0]) {
            esp_mqtt_client_subscribe(event->client, m._controlTopic, 0);
        }
        ESP_LOGI(TAG, "connected, sub %s", m._controlTopic);
        break;
    case MQTT_EVENT_DISCONNECTED:
        m._connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        m._rxCount++;
        if (event->data && event->data_len > 0) {
            m.onControl(event->data, event->data_len);
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
    cfg.buffer_size = 512;
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
