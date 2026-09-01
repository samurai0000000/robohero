/*
 * Mqtt.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MQTT_HXX
#define ROBOHERO_MQTT_HXX

#include <stdbool.h>
#include "mqtt_client.h"

class Mqtt
{
  public:
    static Mqtt &instance();

    bool start();
    void stop();
    bool sendPwmPos(int chan, int pos);
    bool sendVoltage(int voltage);
    bool isRunning() const;
    bool isConnected() const;
    unsigned txCount() const;
    unsigned rxCount() const;
    virtual ~Mqtt();

  protected:
    virtual void onControl(const char *data, int len);
    bool setPwmPos(int chan, int pos);
    int getLowCutoff() const;
    int getHighCutoff() const;

  private:
    Mqtt();
    Mqtt(const Mqtt &);
    Mqtt &operator=(const Mqtt &);
    static Mqtt _self;

    static esp_err_t eventHandler(esp_mqtt_event_handle_t event);
    bool publishStatus(const char *payload);
    void rebuildTopics();

    esp_mqtt_client_handle_t _client;
    bool _running;
    bool _connected;
    unsigned _txCount;
    unsigned _rxCount;
    char _clientId[32];
    char _uri[96];
    char _statusTopic[64];
    char _controlTopic[64];
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
