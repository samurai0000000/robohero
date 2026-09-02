/*
 * Mqtt.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_HOST_MQTT_HXX
#define ROBOHERO_HOST_MQTT_HXX

#include <mosquitto.h>

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "Config.hxx"
#include "Configuration.hxx"

using namespace std;

struct TelemetryData
{
    string robotId;
    uint32_t timestampMs = 0;
    int voltage = -1;
    array<int, ALLSERVOS> servoPositions{};
    array<bool, ALLSERVOS> servoValid{};
    bool hasVoltage = false;
    string rawPayload;

    TelemetryData()
    {
        servoPositions.fill(-1);
        servoValid.fill(false);
    }
};

class Mqtt
{
  public:
    using TelemetryCallback = function<void(const TelemetryData &)>;
    using ConnectionCallback = function<void(bool connected, const string &info)>;
    using LogCallback = function<void(const string &msg)>;

    static Mqtt &instance();

    bool start(const string &host = "", int port = 0);
    void stop();

    bool isConnected() const;
    bool isRunning() const;

    bool sendStop();
    bool sendCenter();
    bool sendZero();
    bool sendRelax();
    bool sendPm(int id);
    bool sendPms(int id);
    bool sendPwm(int chan, int pos);

    // Latest Telemetry access
    TelemetryData getLatestTelemetry() const;
    int getVoltage() const;
    int getServoPosition(int chan) const;
    bool isServoValid(int chan) const;

    // Callbacks
    void onTelemetry(TelemetryCallback cb);
    void onConnection(ConnectionCallback cb);
    void onLog(LogCallback cb);

    string getBrokerUri() const;
    string getControlTopic() const;
    string getStatusTopic() const;

  private:
    Mqtt();
    ~Mqtt();
    Mqtt(const Mqtt &) = delete;
    Mqtt &operator=(const Mqtt &) = delete;

    static void onConnectCallback(struct mosquitto *mosq, void *obj, int rc);
    static void onDisconnectCallback(struct mosquitto *mosq, void *obj, int rc);
    static void onMessageCallback(struct mosquitto *mosq, void *obj,
                                  const struct mosquitto_message *msg);
    static void onLogCallback(struct mosquitto *mosq, void *obj, int level,
                              const char *str);

    void parseStatusPayload(const string &topic,
                            const uint8_t *data,
                            int len);
    bool sendBytes(const uint8_t *data, int len);
    void rebuildTopics();

    static Mqtt _self;
    struct mosquitto *_mosq;
    atomic<bool> _running;
    atomic<bool> _connected;

    string _host;
    int _port;
    string _username;
    string _password;
    string _clientId;
    string _robotId;
    int _keepalive;

    string _controlTopic;
    string _statusTopic;

    mutable mutex _telemetryMutex;
    TelemetryData _latestTelemetry;

    TelemetryCallback _telemetryCb;
    ConnectionCallback _connectionCb;
    LogCallback _logCb;
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
