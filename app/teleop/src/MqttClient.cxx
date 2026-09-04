/*
 * MqttClient.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MqttClient.hxx"
#include <mosquitto.h>
#include <robohero/msg.h>
#include <iostream>
#include <cstring>

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , _mosq(nullptr)
    , _connected(false)
    , _port(1883)
{
    mosquitto_lib_init();
    _mosq = mosquitto_new(nullptr, true, this);
    if (_mosq) {
        mosquitto_connect_callback_set(_mosq, onConnectCallback);
        mosquitto_disconnect_callback_set(_mosq, onDisconnectCallback);
        mosquitto_publish_callback_set(_mosq, onPublishCallback);
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
    (void) mosq;
    auto *self = static_cast<MqttClient *>(userdata);
    if (!self) {
        return;
    }

    if (rc == 0) {
        self->_connected = true;
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

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
