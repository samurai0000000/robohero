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

bool Mqtt::publishStatus(const char *payload)
{
    if (!_connected || !_client || payload == NULL) {
        return false;
    }
    int msgId =
        esp_mqtt_client_publish(_client, _statusTopic, payload, 0, 0, 0);
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
    char buf[MQTT_STATUS_BUF];
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

        unsigned ts = (unsigned) (esp_timer_get_time() / 1000);
        int n = snprintf(buf, sizeof(buf), "t=%u", ts);
        if (n < 0 || n >= (int) sizeof(buf)) {
            continue;
        }
        if (haveVolt) {
            int w = snprintf(buf + n, sizeof(buf) - (size_t) n,
                             ",v=%d", (int) volt);
            if (w < 0 || n + w >= (int) sizeof(buf)) {
                continue;
            }
            n += w;
        }
        for (int i = 0; i < ALLSERVOS; i++) {
            if (!havePwm[i]) {
                continue;
            }
            int w = snprintf(buf + n, sizeof(buf) - (size_t) n,
                             ",c%d=%d", i, (int) pwm[i]);
            if (w < 0 || n + w >= (int) sizeof(buf)) {
                break;
            }
            n += w;
        }
        m.publishStatus(buf);
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

void Mqtt::onControl(const char *data, int len)
{
    char buf[128];
    if (data == NULL || len <= 0) {
        return;
    }
    if (len >= (int) sizeof(buf)) {
        len = (int) sizeof(buf) - 1;
    }
    memcpy(buf, data, (size_t) len);
    buf[len] = '\0';
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' ||
                       buf[len - 1] == ' ')) {
        buf[--len] = '\0';
    }

    RoboHero &rh = RoboHero::instance();
    if (strcmp(buf, "stop") == 0) {
        rh.requestStop();
        return;
    }
    if (strcmp(buf, "center") == 0) {
        rh.submitCenter();
        return;
    }
    if (strcmp(buf, "zero") == 0) {
        rh.submitZero();
        return;
    }
    if (strcmp(buf, "relax") == 0) {
        rh.submitRelax();
        return;
    }

    int id = 0;
    if (sscanf(buf, "pm=%d", &id) == 1 || sscanf(buf, "pm %d", &id) == 1) {
        rh.submitPm(id);
        return;
    }
    if (sscanf(buf, "pms=%d", &id) == 1 || sscanf(buf, "pms %d", &id) == 1) {
        rh.submitPms(id);
        return;
    }

    int chan = 0;
    int pos = 0;
    if (sscanf(buf, "chan=%d pos=%d", &chan, &pos) == 2) {
        setPwmPos(chan, pos);
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
