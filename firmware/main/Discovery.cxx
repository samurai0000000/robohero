/*
 * Discovery.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "esp_log.h"
#include "mdns.h"
#include "Discovery.hxx"

static const char *TAG = "discovery";
#define DISCOVERY_UDP_PORT 8266

Discovery &Discovery::instance()
{
    static Discovery inst;
    return inst;
}

Discovery::Discovery()
    : _running(false)
    , _mdnsStarted(false)
    , _udpTask(NULL)
    , _udpSocket(-1)
{
}

Discovery::~Discovery()
{
    stop();
}

void Discovery::start()
{
    if (_running) {
        return;
    }
    _running = true;

    startMdns();

    xTaskCreate(udpTaskWrapper, "discovery_udp", 2048, this, 5, &_udpTask);
}

void Discovery::stop()
{
    if (!_running) {
        return;
    }
    _running = false;

    if (_udpSocket >= 0) {
        close(_udpSocket);
        _udpSocket = -1;
    }

    _udpTask = NULL;

    stopMdns();
}

void Discovery::startMdns()
{
    if (_mdnsStarted) {
        return;
    }

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %d", err);
        return;
    }

    mdns_hostname_set("robohero");
    mdns_instance_name_set("RoboHero");
    mdns_service_add("RoboHero", "_http", "_tcp", 80, NULL, 0);
    _mdnsStarted = true;
    ESP_LOGI(TAG, "mDNS started: robohero.local -> _http._tcp.:80");
}

void Discovery::stopMdns()
{
    if (!_mdnsStarted) {
        return;
    }
    mdns_free();
    _mdnsStarted = false;
    ESP_LOGI(TAG, "mDNS stopped");
}

void Discovery::udpTaskWrapper(void *arg)
{
    Discovery *self = static_cast<Discovery *>(arg);
    self->runUdpServer();
    vTaskDelete(NULL);
}

void Discovery::runUdpServer()
{
    _udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (_udpSocket < 0) {
        ESP_LOGE(TAG, "Unable to create UDP discovery socket: %d", _udpSocket);
        return;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(DISCOVERY_UDP_PORT);

    int err = bind(_udpSocket, (struct sockaddr *)&saddr, sizeof(saddr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind UDP discovery port %d: %d",
                 DISCOVERY_UDP_PORT, err);
        close(_udpSocket);
        _udpSocket = -1;
        return;
    }

    ESP_LOGI(TAG, "UDP discovery listener started on port %d", DISCOVERY_UDP_PORT);

    char rx_buffer[128];
    while (_running) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(_udpSocket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            if (!_running) {
                break;
            }
            ESP_LOGW(TAG, "recvfrom failed on UDP discovery socket");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        rx_buffer[len] = '\0';
        if (strstr(rx_buffer, "ROBOHERO_DISCOVER") != NULL) {
            const char reply[] = "ROBOHERO_HERE:80:RoboHero";
            sendto(_udpSocket, reply, strlen(reply), 0,
                   (struct sockaddr *)&source_addr, sizeof(source_addr));
            ESP_LOGI(TAG, "Replied to discovery ping from %s:%d",
                     inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port));
        }
    }

    if (_udpSocket >= 0) {
        close(_udpSocket);
        _udpSocket = -1;
    }
}
