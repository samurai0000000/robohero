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
    _connected = false;
}

bool MqttClient::isConnected() const
{
    return _connected;
}

void MqttClient::setRobotId(const std::string &robotId)
{
    _robotId = robotId.empty() ? "robohero" : robotId;
}

bool MqttClient::sendPwm(const std::array<int, 17> &pwmValues, const std::string &topic)
{
    if (!_mosq || !_connected) {
        return false;
    }

    uint8_t buffer[256];
    int used = rh_msg_begin(buffer, sizeof(buffer), RH_MSG_SET_PWM);
    if (used < 0) {
        return false;
    }

    for (uint8_t ch = 0; ch < 17; ++ch) {
        rh_tlv_servo servo;
        servo.chan = ch;
        servo.pos = static_cast<int16_t>(pwmValues[ch]);
        used = rh_tlv_put(buffer, sizeof(buffer), used, RH_TLV_SERVO, &servo, sizeof(servo));
        if (used < 0) {
            return false;
        }
    }

    used = rh_msg_finish(buffer, used);
    if (used < 0) {
        return false;
    }

    int mid = 0;
    int rc = mosquitto_publish(_mosq, &mid, topic.c_str(), used, buffer, 0, false);
    if (rc == MOSQ_ERR_SUCCESS) {
        emit messageSent(used);
        return true;
    }

    return false;
}

bool MqttClient::sendCenter(const std::string &topic)
{
    if (!_mosq || !_connected) {
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
    if (!_mosq || !_connected) {
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
    if (!_mosq || !_connected) {
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

void MqttClient::onConnectCallback(struct mosquitto *mosq, void *userdata, int rc)
{
    auto *self = static_cast<MqttClient *>(userdata);
    if (!self) {
        return;
    }

    if (rc == 0) {
        self->_connected = true;

        std::string rid = self->_robotId.empty() ? "robohero" : self->_robotId;
        std::string statusTopic = "robot/robohero/" + rid + "/status";
        std::string wildcardStatus = "robot/robohero/+/status";
        std::string controlTopic = "robot/robohero/" + rid + "/control";
        std::string cmdTopic = "robot/robohero/" + rid + "/cmd";
        std::string shortStatus = rid + "/status";
        std::string shortCmd = rid + "/cmd";

        mosquitto_subscribe(mosq, nullptr, statusTopic.c_str(), 0);
        mosquitto_subscribe(mosq, nullptr, wildcardStatus.c_str(), 0);
        mosquitto_subscribe(mosq, nullptr, controlTopic.c_str(), 0);
        mosquitto_subscribe(mosq, nullptr, cmdTopic.c_str(), 0);
        mosquitto_subscribe(mosq, nullptr, shortStatus.c_str(), 0);
        mosquitto_subscribe(mosq, nullptr, shortCmd.c_str(), 0);

        emit self->connected();
    } else {
        self->_connected = false;
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

    self->_connected = false;
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

    std::array<int, 17> pwm{};
    std::array<bool, 17> valid{};
    valid.fill(false);

    // 1. Try binary wire protocol first
    rh_msg_view view;
    if (rh_msg_parse(msg->payload, msg->payloadlen, &view) == 0) {
        if (view.msg_type == RH_MSG_STATUS || view.msg_type == RH_MSG_SET_PWM) {
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
                    }
                }
                off = next;
            }
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
                    }
                } else if (obj.contains("servos") && obj["servos"].isArray()) {
                    QJsonArray arr = obj["servos"].toArray();
                    for (int i = 0; i < arr.size() && i < 17; ++i) {
                        pwm[i] = arr[i].toInt();
                        valid[i] = true;
                    }
                }
            } else if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (int i = 0; i < arr.size() && i < 17; ++i) {
                    pwm[i] = arr[i].toInt();
                    valid[i] = true;
                }
            }
        }
    }

    bool anyValid = false;
    for (bool v : valid) {
        if (v) {
            anyValid = true;
            break;
        }
    }

    if (anyValid) {
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
