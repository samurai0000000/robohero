/*
 * Mqtt.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <cstdio>
#include <cstring>
#include <cctype>
#include <sstream>

#include "Mqtt.hxx"

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
    string payload(static_cast<const char *>(msg->payload), msg->payloadlen);
    self->parseStatusPayload(topic, payload);
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

void Mqtt::parseStatusPayload(const string &topic, const string &payload)
{
    TelemetryData t;
    t.rawPayload = payload;

    // Topic format: robot/robohero/<robotId>/status
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

    // Payload tokens separated by commas: e.g. t=1234,v=740,c0=135,c1=200
    stringstream ss(payload);
    string token;
    while (getline(ss, token, ',')) {
        while (!token.empty() && isspace(token.front())) token.erase(token.begin());
        while (!token.empty() && isspace(token.back())) token.pop_back();

        if (token.rfind("t=", 0) == 0) {
            try {
                t.timestampMs = stoul(token.substr(2));
            } catch (...) {}
        } else if (token.rfind("v=", 0) == 0) {
            try {
                t.voltage = stoi(token.substr(2));
                t.hasVoltage = true;
            } catch (...) {}
        } else if (!token.empty() && token[0] == 'c') {
            size_t eq = token.find('=');
            if (eq != string::npos && eq > 1) {
                try {
                    int chan = stoi(token.substr(1, eq - 1));
                    int pos = stoi(token.substr(eq + 1));
                    if (chan >= 0 && chan < ALLSERVOS) {
                        t.servoPositions[chan] = pos;
                        t.servoValid[chan] = true;
                    }
                } catch (...) {}
            }
        }
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

bool Mqtt::sendCommand(const string &cmd)
{
    if (!_mosq || !_connected.load()) {
        return false;
    }

    int rc = mosquitto_publish(_mosq, nullptr, _controlTopic.c_str(),
                               static_cast<int>(cmd.length()), cmd.c_str(),
                               0, false);
    return (rc == MOSQ_ERR_SUCCESS);
}

bool Mqtt::sendStop()
{
    return sendCommand("stop");
}

bool Mqtt::sendCenter()
{
    return sendCommand("center");
}

bool Mqtt::sendZero()
{
    return sendCommand("zero");
}

bool Mqtt::sendRelax()
{
    return sendCommand("relax");
}

bool Mqtt::sendPm(int id)
{
    return sendCommand("pm=" + to_string(id));
}

bool Mqtt::sendPms(int id)
{
    return sendCommand("pms=" + to_string(id));
}

bool Mqtt::sendPwm(int chan, int pos)
{
    return sendCommand("chan=" + to_string(chan) + " pos=" + to_string(pos));
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
