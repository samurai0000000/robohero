/*
 * Mqtt.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <cstdio>
#include <cstring>

#include "Mqtt.hxx"
#include "robohero/msg.h"

using namespace std;

Mqtt Mqtt::_self;

Mqtt &Mqtt::instance()
{
    return _self;
}

Mqtt::Mqtt()
    : _mosq(nullptr), _running(false), _connected(false),
      _host("localhost"), _port(1883), _keepalive(60)
{
    mosquitto_lib_init();
}

Mqtt::~Mqtt()
{
    stop();
    mosquitto_lib_cleanup();
}

void Mqtt::rebuildTopics()
{
    string rid = _robotId.empty() ? "+" : _robotId;
    _statusTopic = "robot/robohero/" + rid + "/status";
    if (_robotId.empty() || _robotId == "+") {
        _controlTopic = "robot/robohero/robohero/control";
    } else {
        _controlTopic = "robot/robohero/" + _robotId + "/control";
    }
}

string Mqtt::getBrokerUri() const
{
    return _host + ":" + to_string(_port);
}

string Mqtt::getControlTopic() const
{
    return _controlTopic;
}

string Mqtt::getStatusTopic() const
{
    return _statusTopic;
}

bool Mqtt::isConnected() const
{
    return _connected.load();
}

bool Mqtt::isRunning() const
{
    return _running.load();
}

bool Mqtt::start(const string &host, int port)
{
    stop();

    Configuration &cfg = Configuration::instance();
    _host = host.empty() ? cfg.getMqttHost() : host;
    _port = (port <= 0) ? cfg.getMqttPort() : port;
    _username = cfg.getMqttUsername();
    _password = cfg.getMqttPassword();
    _clientId = cfg.getMqttClientId();
    _robotId = cfg.getMqttRobotId();
    _keepalive = cfg.getMqttKeepalive();

    rebuildTopics();

    _mosq = mosquitto_new(_clientId.empty() ? nullptr : _clientId.c_str(),
                          true, this);
    if (!_mosq) {
        if (_logCb) {
            _logCb("Failed to create mosquitto instance");
        }
        return false;
    }

    if (!_username.empty()) {
        mosquitto_username_pw_set(_mosq, _username.c_str(),
                                  _password.empty() ? nullptr : _password.c_str());
    }

    mosquitto_connect_callback_set(_mosq, onConnectCallback);
    mosquitto_disconnect_callback_set(_mosq, onDisconnectCallback);
    mosquitto_message_callback_set(_mosq, onMessageCallback);
    mosquitto_log_callback_set(_mosq, onLogCallback);

    int rc = mosquitto_connect_async(_mosq, _host.c_str(), _port, _keepalive);
    if (rc != MOSQ_ERR_SUCCESS) {
        if (_logCb) {
            _logCb("MQTT connect error: " + string(mosquitto_strerror(rc)));
        }
        mosquitto_destroy(_mosq);
        _mosq = nullptr;
        return false;
    }

    rc = mosquitto_loop_start(_mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        if (_logCb) {
            _logCb("MQTT loop start error: " + string(mosquitto_strerror(rc)));
        }
        mosquitto_disconnect(_mosq);
        mosquitto_destroy(_mosq);
        _mosq = nullptr;
        return false;
    }

    _running.store(true);
    return true;
}

void Mqtt::stop()
{
    if (_running.load() && _mosq) {
        _running.store(false);
        mosquitto_disconnect(_mosq);
        mosquitto_loop_stop(_mosq, true);
        mosquitto_destroy(_mosq);
        _mosq = nullptr;
        _connected.store(false);
    }
}

void Mqtt::onConnectCallback(struct mosquitto *mosq, void *obj, int rc)
{
    Mqtt *self = static_cast<Mqtt *>(obj);
    if (!self) {
        return;
    }

    if (rc == 0) {
        self->_connected.store(true);
        mosquitto_subscribe(mosq, nullptr, self->_statusTopic.c_str(), 0);
        if (self->_statusTopic != "robot/robohero/+/status") {
            mosquitto_subscribe(mosq, nullptr, "robot/robohero/+/status", 0);
        }
        if (self->_connectionCb) {
            self->_connectionCb(true, "Connected to " + self->getBrokerUri());
        }
    } else {
        self->_connected.store(false);
        if (self->_connectionCb) {
            self->_connectionCb(false, "Connection failed: " +
                                string(mosquitto_connack_string(rc)));
        }
    }
}

void Mqtt::onDisconnectCallback(struct mosquitto *, void *obj, int rc)
{
    Mqtt *self = static_cast<Mqtt *>(obj);
    if (!self) {
        return;
    }

    self->_connected.store(false);
    if (self->_connectionCb) {
        self->_connectionCb(false, (rc == 0) ? "Disconnected cleanly"
                                             : "Disconnected unexpectedly");
    }
}

void Mqtt::onMessageCallback(struct mosquitto *, void *obj,
                             const struct mosquitto_message *msg)
{
    Mqtt *self = static_cast<Mqtt *>(obj);
    if (!self || !msg || !msg->payload) {
        return;
    }

    string topic(msg->topic ? msg->topic : "");
    self->parseStatusPayload(topic,
                             static_cast<const uint8_t *>(msg->payload),
                             msg->payloadlen);
}

void Mqtt::onLogCallback(struct mosquitto *, void *obj, int level,
                         const char *str)
{
    (void) level;
    Mqtt *self = static_cast<Mqtt *>(obj);
    if (self && self->_logCb && str) {
        // self->_logCb(str);
    }
}

static string hexDump(const uint8_t *p, int n)
{
    string s;

    if (p == NULL || n <= 0) {
        return s;
    }
    s.reserve((size_t) n * 3);
    for (int i = 0; i < n; i++) {
        char b[4];
        snprintf(b, sizeof(b), "%02x", p[i]);
        if (i > 0) {
            s += ' ';
        }
        s += b;
    }
    return s;
}

void Mqtt::parseStatusPayload(const string &topic,
                              const uint8_t *data,
                              int len)
{
    rh_msg_view view;
    TelemetryData t;
    int off;

    if (rh_msg_parse(data, len, &view) != 0) {
        return;
    }
    if (view.msg_type != RH_MSG_STATUS) {
        return;
    }

    t.rawPayload = hexDump(data, len);

    size_t prefixLen = sizeof("robot/robohero/") - 1;
    if (topic.length() > prefixLen) {
        string rem = topic.substr(prefixLen);
        size_t slash = rem.find('/');
        if (slash != string::npos) {
            t.robotId = rem.substr(0, slash);
        } else {
            t.robotId = rem;
        }
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
        if (type == RH_TLV_TIME && n == sizeof(uint32_t)) {
            memcpy(&t.timestampMs, val, sizeof(t.timestampMs));
        } else if (type == RH_TLV_VOLTAGE && n == sizeof(int16_t)) {
            int16_t v;
            memcpy(&v, val, sizeof(v));
            t.voltage = v;
            t.hasVoltage = true;
        } else if (type == RH_TLV_SERVO && n == sizeof(rh_tlv_servo)) {
            rh_tlv_servo s;
            memcpy(&s, val, sizeof(s));
            if (s.chan < ALLSERVOS) {
                t.servoPositions[s.chan] = s.pos;
                t.servoValid[s.chan] = true;
            }
        }
        off = next;
    }

    {
        lock_guard<mutex> lock(_telemetryMutex);
        if (t.hasVoltage) {
            _latestTelemetry.voltage = t.voltage;
            _latestTelemetry.hasVoltage = true;
        }
        if (t.timestampMs > 0) {
            _latestTelemetry.timestampMs = t.timestampMs;
        }
        if (!t.robotId.empty()) {
            _latestTelemetry.robotId = t.robotId;
        }
        _latestTelemetry.rawPayload = t.rawPayload;
        for (int i = 0; i < ALLSERVOS; i++) {
            if (t.servoValid[i]) {
                _latestTelemetry.servoPositions[i] = t.servoPositions[i];
                _latestTelemetry.servoValid[i] = true;
            }
        }
    }

    if (_telemetryCb) {
        _telemetryCb(t);
    }
}

bool Mqtt::sendBytes(const uint8_t *data, int len)
{
    if (!_mosq || !_connected.load() || data == NULL || len <= 0) {
        return false;
    }

    int rc = mosquitto_publish(_mosq,
                               nullptr,
                               _controlTopic.c_str(),
                               len,
                               data,
                               0,
                               false);
    return (rc == MOSQ_ERR_SUCCESS);
}

bool Mqtt::sendStop()
{
    uint8_t buf[sizeof(rh_msg_hdr)];
    int n = rh_msg_empty(buf, sizeof(buf), RH_MSG_STOP);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

bool Mqtt::sendCenter()
{
    uint8_t buf[sizeof(rh_msg_hdr)];
    int n = rh_msg_empty(buf, sizeof(buf), RH_MSG_CENTER);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

bool Mqtt::sendZero()
{
    uint8_t buf[sizeof(rh_msg_hdr)];
    int n = rh_msg_empty(buf, sizeof(buf), RH_MSG_ZERO);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

bool Mqtt::sendRelax()
{
    uint8_t buf[sizeof(rh_msg_hdr)];
    int n = rh_msg_empty(buf, sizeof(buf), RH_MSG_RELAX);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

bool Mqtt::sendPm(int id)
{
    uint8_t buf[16];
    int16_t id16 = (int16_t) id;
    int n = rh_msg_begin(buf, sizeof(buf), RH_MSG_PM);
    if (n < 0) {
        return false;
    }
    n = rh_tlv_put(buf, sizeof(buf), n, RH_TLV_PROG, &id16, sizeof(id16));
    if (n < 0) {
        return false;
    }
    n = rh_msg_finish(buf, n);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

bool Mqtt::sendPms(int id)
{
    uint8_t buf[16];
    int16_t id16 = (int16_t) id;
    int n = rh_msg_begin(buf, sizeof(buf), RH_MSG_PMS);
    if (n < 0) {
        return false;
    }
    n = rh_tlv_put(buf, sizeof(buf), n, RH_TLV_PROG, &id16, sizeof(id16));
    if (n < 0) {
        return false;
    }
    n = rh_msg_finish(buf, n);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

bool Mqtt::sendPwm(int chan, int pos)
{
    uint8_t buf[16];
    rh_tlv_servo s;
    int n;

    if (chan < 0 || chan >= ALLSERVOS) {
        return false;
    }
    s.chan = (uint8_t) chan;
    {
        int16_t pos16 = (int16_t) pos;
        memcpy(&s.pos, &pos16, sizeof(s.pos));
    }
    n = rh_msg_begin(buf, sizeof(buf), RH_MSG_SET_PWM);
    if (n < 0) {
        return false;
    }
    n = rh_tlv_put(buf, sizeof(buf), n, RH_TLV_SERVO, &s, sizeof(s));
    if (n < 0) {
        return false;
    }
    n = rh_msg_finish(buf, n);
    if (n < 0) {
        return false;
    }
    return sendBytes(buf, n);
}

TelemetryData Mqtt::getLatestTelemetry() const
{
    lock_guard<mutex> lock(_telemetryMutex);
    return _latestTelemetry;
}

int Mqtt::getVoltage() const
{
    lock_guard<mutex> lock(_telemetryMutex);
    return _latestTelemetry.voltage;
}

int Mqtt::getServoPosition(int chan) const
{
    if (chan < 0 || chan >= ALLSERVOS) {
        return -1;
    }
    lock_guard<mutex> lock(_telemetryMutex);
    return _latestTelemetry.servoPositions[chan];
}

bool Mqtt::isServoValid(int chan) const
{
    if (chan < 0 || chan >= ALLSERVOS) {
        return false;
    }
    lock_guard<mutex> lock(_telemetryMutex);
    return _latestTelemetry.servoValid[chan];
}

void Mqtt::onTelemetry(TelemetryCallback cb)
{
    _telemetryCb = cb;
}

void Mqtt::onConnection(ConnectionCallback cb)
{
    _connectionCb = cb;
}

void Mqtt::onLog(LogCallback cb)
{
    _logCb = cb;
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
