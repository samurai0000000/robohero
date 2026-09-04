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
#include <atomic>
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
    void setRobotId(const std::string &robotId);

    std::string controlTopic() const;
    std::string statusTopic() const;

    bool sendPwm(const std::array<int, 17> &pwmValues, const std::string &topic);
    bool sendCenter(const std::string &topic);
    bool sendRelax(const std::string &topic);
    bool sendStop(const std::string &topic);
    QString lastError() const;
    void resetPwmTxCache();

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &errorMessage);
    void messageSent(int bytes);
    void telemetryReceived(const std::array<int, 17> &pwmValues,
                           const std::array<bool, 17> &validMask);

private:
    struct mosquitto *_mosq;
    std::atomic<bool> _connected;
    std::string _host;
    int _port;
    std::string _robotId;
    QString _lastError;
    std::array<int, 17> _lastSentPwm;
    std::array<bool, 17> _lastSentValid;

    void subscribeTopics();
    std::string sanitizedId() const;

    static void onConnectCallback(struct mosquitto *mosq, void *userdata, int rc);
    static void onDisconnectCallback(struct mosquitto *mosq, void *userdata, int rc);
    static void onPublishCallback(struct mosquitto *mosq, void *userdata, int mid);
    static void onMessageCallback(struct mosquitto *mosq, void *userdata,
                                  const struct mosquitto_message *msg);
};

typedef std::array<int, 17> ServoPwmArray;
typedef std::array<bool, 17> ServoValidArray;
Q_DECLARE_METATYPE(ServoPwmArray)
Q_DECLARE_METATYPE(ServoValidArray)

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
