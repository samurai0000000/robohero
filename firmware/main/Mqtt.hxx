/*
 * Mqtt.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MQTT_HXX
#define ROBOHERO_MQTT_HXX

#include <stdbool.h>
#include <stdint.h>

#include "mqtt_client.h"

#include "Config.hxx"

class Mqtt
{
  public:
    static Mqtt &instance();

    bool start();
    void stop();
    bool sendPwmPos(int chan, int pos);
    bool sendVoltage(int voltage);
    bool publishJsonState(int volt = -1, bool lowVoltage = false, bool moving = false);
    bool isRunning() const;
    bool isConnected() const;
    unsigned txCount() const;
    unsigned rxCount() const;
    unsigned droppedCount() const;
    virtual ~Mqtt();

  protected:
    virtual void onControl(const void *data, int len);
    void onHaCommand(const void *data, int len);
    void onHaHead(const void *data, int len);
    bool setPwmPos(int chan, int pos);
    int getLowCutoff() const;
    int getHighCutoff() const;

  private:
    Mqtt();
    Mqtt(const Mqtt &);
    Mqtt &operator=(const Mqtt &);
    static Mqtt _self;

    static esp_err_t eventHandler(esp_mqtt_event_handle_t event);
    static void pubTask(void *arg);
    bool publishStatus(const void *payload, int len);
    void publishHaDiscovery();
    void publishHaEntity(const char *component, const char *objectId, const char *jsonConfig);
    bool enqueuePending(int16_t *slot, int16_t v);
    void clearPending();
    void rebuildTopics();

    esp_mqtt_client_handle_t _client;
    bool _running;
    bool _connected;
    bool _relaxed;
    unsigned _txCount;
    unsigned _rxCount;
    unsigned _dropped;
    int16_t _pwmPending[ALLSERVOS];
    int16_t _voltPending;
    void *_pubTask;
    char _clientId[32];
    char _cleanId[32];
    char _uri[96];
    char _statusTopic[64];
    char _controlTopic[64];
    char _availTopic[64];
    char _stateTopic[64];
    char _cmdTopic[64];
    char _headTopic[64];
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
