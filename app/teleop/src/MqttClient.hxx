/*
 * MqttClient.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MQTT_CLIENT_HXX
#define ROBOHERO_MQTT_CLIENT_HXX

#include <QObject>
#include <QString>
#include <array>
#include <string>
#include <memory>

struct mosquitto;

class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient() override;

    bool connectToBroker(const std::string &host, int port,
                         const std::string &username, const std::string &password,
                         int keepaliveSec = 60);
    void disconnectFromBroker();
    bool isConnected() const;

    bool sendPwm(const std::array<int, 17> &pwmValues, const std::string &topic);
    bool sendCenter(const std::string &topic);
    bool sendRelax(const std::string &topic);
    bool sendStop(const std::string &topic);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &errorMessage);
    void messageSent(int bytes);

private:
    struct mosquitto *_mosq;
    bool _connected;
    std::string _host;
    int _port;

    static void onConnectCallback(struct mosquitto *mosq, void *userdata, int rc);
    static void onDisconnectCallback(struct mosquitto *mosq, void *userdata, int rc);
    static void onPublishCallback(struct mosquitto *mosq, void *userdata, int mid);
};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
