/*
 * Discovery.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef DISCOVERY_HXX
#define DISCOVERY_HXX

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Discovery {
public:
    static Discovery &instance();

    void start();
    void stop();
    bool isRunning() const { return _running; }

private:
    Discovery();
    ~Discovery();

    void startMdns();
    void stopMdns();

    static void udpTaskWrapper(void *arg);
    void runUdpServer();

    bool _running;
    bool _mdnsStarted;
    TaskHandle_t _udpTask;
    int _udpSocket;
};

#endif /* DISCOVERY_HXX */
