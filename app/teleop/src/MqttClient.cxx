/*
 * MqttClient.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MqttClient.hxx"
#include <mosquitto.h>
#include <robohero/msg.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <cstring>

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , _mosq(nullptr)
    , _connected(false)
    , _port(1883)
    , _robotId("robohero")
{
    _lastSentPwm.fill(0);
    _lastSentValid.fill(false);
    mosquitto_lib_init();
    _mosq = mosquitto_new(nullptr, true, this);
    if (_mosq) {
        mosquitto_connect_callback_set(_mosq, onConnectCallback);
        mosquitto_disconnect_callback_set(_mosq, onDisconnectCallback);
        mosquitto_publish_callback_set(_mosq, onPublishCallback);
        mosquitto_message_callback_set(_mosq, onMessageCallback);
    }
}

MqttClient::~MqttClient()
{
    disconnectFromBroker();
    if (_mosq) {
        mosquitto_destroy(_mosq);
        _mosq = nullptr;
    }
    mosquitto_lib_cleanup();
}

bool MqttClient::connectToBroker(const std::string &host, int port,
                                 const std::string &username,
                                 const std::string &password,
                                 int keepaliveSec)
{
    if (!_mosq) {
        emit connectionError("Mosquitto instance not initialized");
        return false;
    }

    disconnectFromBroker();

    _host = host;
    _port = port;

    if (!username.empty()) {
        mosquitto_username_pw_set(_mosq, username.c_str(),
                                  password.empty() ? nullptr : password.c_str());
    } else {
        mosquitto_username_pw_set(_mosq, nullptr, nullptr);
    }

    int rc = mosquitto_connect_async(_mosq, host.c_str(), port, keepaliveSec);
    if (rc != MOSQ_ERR_SUCCESS) {
        QString err = QString("MQTT connection error: %1").arg(mosquitto_strerror(rc));
        emit connectionError(err);
        return false;
    }

    mosquitto_loop_start(_mosq);
    return true;
}

void MqttClient::disconnectFromBroker()
{
    if (_mosq) {
        mosquitto_disconnect(_mosq);
        mosquitto_loop_stop(_mosq, true);
    }
    _connected.store(false);
    resetPwmTxCache();
}

void MqttClient::resetPwmTxCache()
{
    _lastSentPwm.fill(0);
    _lastSentValid.fill(false);
}

bool MqttClient::isConnected() const
{
    return _connected.load();
}

void MqttClient::setRobotId(const std::string &robotId)
{
    std::string id = robotId;
    const std::string prefix = "robot/robohero/";
    if (id.rfind(prefix, 0) == 0) {
        id = id.substr(prefix.length());
    }
    while (!id.empty() && id.back() == '/') {
        id.pop_back();
    }
    if (id.size() >= 8 && id.compare(id.size() - 8, 8, "/control") == 0) {
        id = id.substr(0, id.size() - 8);
    } else if (id.size() >= 7 && id.compare(id.size() - 7, 7, "/status") == 0) {
        id = id.substr(0, id.size() - 7);
    }
    _robotId = id.empty() ? "robohero" : id;
    resetPwmTxCache();
    if (_connected.load()) {
        subscribeTopics();
    }
}

std::string MqttClient::sanitizedId() const
{
    return _robotId.empty() ? "robohero" : _robotId;
}

std::string MqttClient::controlTopic() const
{
    return "robot/robohero/" + sanitizedId() + "/control";
}

std::string MqttClient::statusTopic() const
{
    return "robot/robohero/" + sanitizedId() + "/status";
}

QString MqttClient::lastError() const
{
    return _lastError;
}

bool MqttClient::sendPwm(const std::array<int, 17> &pwmValues, const std::string &topic)
{
    if (!_mosq || !_connected.load()) {
        _lastError = "MQTT not connected";
        return false;
    }

    int published = 0;
    int totalBytes = 0;

    for (uint8_t ch = 0; ch < 17; ++ch) {
        int pos = pwmValues[ch];
        if (pos < 1) {
            pos = 1;
        } else if (pos > 270) {
            pos = 270;
        }

        if (_lastSentValid[ch] && _lastSentPwm[ch] == pos) {
            continue;
        }

        uint8_t buffer[32];
        rh_tlv_servo servo{};
        int16_t pos16 = static_cast<int16_t>(pos);
        servo.chan = ch;
        std::memcpy(&servo.pos, &pos16, sizeof(pos16));

        int used = rh_msg_begin(buffer, sizeof(buffer), RH_MSG_SET_PWM);
        if (used < 0) {
            _lastError = "Failed to encode SET_PWM header";
            return false;
        }
        used = rh_tlv_put(buffer, sizeof(buffer), used, RH_TLV_SERVO, &servo,
                          sizeof(servo));
        if (used < 0) {
            _lastError = "Failed to encode SET_PWM servo TLV";
            return false;
        }
        used = rh_msg_finish(buffer, used);
        if (used < 0) {
            _lastError = "Failed to finish SET_PWM message";
            return false;
        }

        int mid = 0;
        int rc = mosquitto_publish(_mosq, &mid, topic.c_str(), used, buffer, 0, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            _lastError = QString("MQTT publish failed: %1").arg(mosquitto_strerror(rc));
            return false;
        }

        _lastSentPwm[ch] = pos;
        _lastSentValid[ch] = true;
        published++;
        totalBytes += used;
    }

    _lastError.clear();
    if (published > 0) {
        emit messageSent(totalBytes);
    }
    return true;
}

bool MqttClient::sendCenter(const std::string &topic)
{
    if (!_mosq || !_connected.load()) {
        _lastError = "MQTT not connected";
        return false;
    }

    uint8_t buffer[32];
    int used = rh_msg_empty(buffer, sizeof(buffer), RH_MSG_CENTER);
    if (used < 0) {
        return false;
    }

    int mid = 0;
    return mosquitto_publish(_mosq, &mid, topic.c_str(), used, buffer, 0, false) == MOSQ_ERR_SUCCESS;
}

bool MqttClient::sendRelax(const std::string &topic)
{
    if (!_mosq || !_connected.load()) {
        _lastError = "MQTT not connected";
        return false;
    }

    uint8_t buffer[32];
    int used = rh_msg_empty(buffer, sizeof(buffer), RH_MSG_RELAX);
    if (used < 0) {
        return false;
    }

    int mid = 0;
    return mosquitto_publish(_mosq, &mid, topic.c_str(), used, buffer, 0, false) == MOSQ_ERR_SUCCESS;
}

bool MqttClient::sendStop(const std::string &topic)
{
    if (!_mosq || !_connected.load()) {
        _lastError = "MQTT not connected";
        return false;
    }

    uint8_t buffer[32];
    int used = rh_msg_empty(buffer, sizeof(buffer), RH_MSG_STOP);
    if (used < 0) {
        return false;
    }

    int mid = 0;
    return mosquitto_publish(_mosq, &mid, topic.c_str(), used, buffer, 0, false) == MOSQ_ERR_SUCCESS;
}

void MqttClient::subscribeTopics()
{
    if (!_mosq || !_connected.load()) {
        return;
    }

    std::string status = statusTopic();
    mosquitto_subscribe(_mosq, nullptr, status.c_str(), 0);
}

void MqttClient::onConnectCallback(struct mosquitto *mosq, void *userdata, int rc)
{
    (void) mosq;
    auto *self = static_cast<MqttClient *>(userdata);
    if (!self) {
        return;
    }

    if (rc == 0) {
        self->_connected.store(true);
        self->subscribeTopics();
        emit self->connected();
    } else {
        self->_connected.store(false);
        emit self->connectionError(QString("MQTT Connection failed with code: %1").arg(rc));
    }
}

void MqttClient::onDisconnectCallback(struct mosquitto *mosq, void *userdata, int rc)
{
    (void) mosq;
    (void) rc;
    auto *self = static_cast<MqttClient *>(userdata);
    if (!self) {
        return;
    }

    self->_connected.store(false);
    emit self->disconnected();
}

void MqttClient::onPublishCallback(struct mosquitto *mosq, void *userdata, int mid)
{
    (void) mosq;
    (void) userdata;
    (void) mid;
}

void MqttClient::onMessageCallback(struct mosquitto *mosq, void *userdata,
                                  const struct mosquitto_message *msg)
{
    (void) mosq;
    auto *self = static_cast<MqttClient *>(userdata);
    if (!self || !msg || !msg->payload || msg->payloadlen <= 0) {
        return;
    }

    std::string topicStr = msg->topic ? msg->topic : "";
    const std::string statusSuffix = "/status";
    if (topicStr.size() < statusSuffix.size() ||
        topicStr.compare(topicStr.size() - statusSuffix.size(),
                         statusSuffix.size(), statusSuffix) != 0) {
        return;
    }
    if (topicStr != self->statusTopic()) {
        return;
    }

    std::array<int, 17> pwm{};
    std::array<bool, 17> valid{};
    valid.fill(false);
    int validCount = 0;

    // 1. Try binary wire protocol first
    rh_msg_view view;
    if (rh_msg_parse(msg->payload, msg->payloadlen, &view) == 0) {
        if (view.msg_type != RH_MSG_STATUS) {
            return;
        }
        int off = 0;
        while (off < static_cast<int>(view.payload_len)) {
            uint8_t type = 0;
            uint8_t len = 0;
            const uint8_t *val = nullptr;
            int next = rh_tlv_next(view.payload, view.payload_len, off,
                                   &type, &val, &len);
            if (next < 0) {
                break;
            }

            if (type == RH_TLV_SERVO && len == sizeof(rh_tlv_servo)) {
                rh_tlv_servo s;
                std::memcpy(&s, val, sizeof(s));
                if (s.chan < 17) {
                    pwm[s.chan] = s.pos;
                    valid[s.chan] = true;
                    validCount++;
                }
            }
            off = next;
        }
    } else {
        // 2. Fallback: Try JSON payload
        QByteArray payloadBytes(static_cast<const char *>(msg->payload), msg->payloadlen);
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(payloadBytes, &err);
        if (err.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("channel") && (obj.contains("pos") || obj.contains("pwm"))) {
                    int ch = obj["channel"].toInt(-1);
                    int pos = obj.contains("pos") ? obj["pos"].toInt() : obj["pwm"].toInt();
                    if (ch >= 0 && ch < 17) {
                        pwm[ch] = pos;
                        valid[ch] = true;
                        validCount++;
                    }
                } else if (obj.contains("servos") && obj["servos"].isArray()) {
                    QJsonArray arr = obj["servos"].toArray();
                    for (int i = 0; i < arr.size() && i < 17; ++i) {
                        pwm[i] = arr[i].toInt();
                        valid[i] = true;
                        validCount++;
                    }
                }
            } else if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (int i = 0; i < arr.size() && i < 17; ++i) {
                    pwm[i] = arr[i].toInt();
                    valid[i] = true;
                    validCount++;
                }
            }
        }
    }

    if (validCount > 0) {
        emit self->telemetryReceived(pwm, valid);
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
