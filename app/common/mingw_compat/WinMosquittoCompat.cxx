/*
 * WinMosquittoCompat.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "mosquitto.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<int> g_wsaRefCount{0};
std::atomic<unsigned int> g_clientCounter{1};

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void closeSocketHandle(SocketHandle &sock)
{
    if (sock != kInvalidSocket) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        sock = kInvalidSocket;
    }
}

void setSocketNonBlocking(SocketHandle sock, bool nonBlocking)
{
#ifdef _WIN32
    u_long mode = nonBlocking ? 1UL : 0UL;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        if (nonBlocking) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
#endif
}

void encodeRemainingLength(std::vector<uint8_t> &buf, size_t length)
{
    do {
        uint8_t encodedByte = static_cast<uint8_t>(length % 128U);
        length /= 128U;
        if (length > 0U) {
            encodedByte = static_cast<uint8_t>(encodedByte | 0x80U);
        }
        buf.push_back(encodedByte);
    } while (length > 0U);
}

void appendMqttString(std::vector<uint8_t> &buf, const std::string &str)
{
    uint16_t len = static_cast<uint16_t>(str.size() > 65535U ? 65535U : str.size());
    buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFFU));
    buf.push_back(static_cast<uint8_t>(len & 0xFFU));
    buf.insert(buf.end(), str.begin(), str.begin() + len);
}

std::vector<uint8_t> buildConnectPacket(const std::string &clientId,
                                        bool cleanSession,
                                        uint16_t keepaliveSec,
                                        bool hasUsername,
                                        const std::string &username,
                                        bool hasPassword,
                                        const std::string &password)
{
    std::vector<uint8_t> body;
    appendMqttString(body, "MQTT");
    body.push_back(0x04U); // Protocol Level 4 (MQTT v3.1.1)

    uint8_t flags = cleanSession ? 0x02U : 0x00U;
    if (hasUsername) {
        flags = static_cast<uint8_t>(flags | 0x80U);
        if (hasPassword) {
            flags = static_cast<uint8_t>(flags | 0x40U);
        }
    }
    body.push_back(flags);
    body.push_back(static_cast<uint8_t>((keepaliveSec >> 8) & 0xFFU));
    body.push_back(static_cast<uint8_t>(keepaliveSec & 0xFFU));

    appendMqttString(body, clientId);
    if (hasUsername) {
        appendMqttString(body, username);
        if (hasPassword) {
            appendMqttString(body, password);
        }
    }

    std::vector<uint8_t> pkt;
    pkt.push_back(0x10U); // CONNECT
    encodeRemainingLength(pkt, body.size());
    pkt.insert(pkt.end(), body.begin(), body.end());
    return pkt;
}

std::vector<uint8_t> buildPublishPacket(const std::string &topic,
                                        uint16_t mid,
                                        const void *payload,
                                        int payloadlen,
                                        int qos,
                                        bool retain)
{
    std::vector<uint8_t> body;
    appendMqttString(body, topic);
    int clampedQos = (qos < 0) ? 0 : (qos > 1 ? 1 : qos);
    if (clampedQos > 0) {
        body.push_back(static_cast<uint8_t>((mid >> 8) & 0xFFU));
        body.push_back(static_cast<uint8_t>(mid & 0xFFU));
    }
    if (payload != nullptr && payloadlen > 0) {
        const auto *bytes = static_cast<const uint8_t *>(payload);
        body.insert(body.end(), bytes, bytes + payloadlen);
    }

    uint8_t hdr = 0x30U;
    hdr = static_cast<uint8_t>(hdr | ((clampedQos & 0x03) << 1));
    if (retain) {
        hdr = static_cast<uint8_t>(hdr | 0x01U);
    }

    std::vector<uint8_t> pkt;
    pkt.push_back(hdr);
    encodeRemainingLength(pkt, body.size());
    pkt.insert(pkt.end(), body.begin(), body.end());
    return pkt;
}

std::vector<uint8_t> buildSubscribePacket(uint16_t mid,
                                          const std::string &sub,
                                          int qos)
{
    std::vector<uint8_t> body;
    body.push_back(static_cast<uint8_t>((mid >> 8) & 0xFFU));
    body.push_back(static_cast<uint8_t>(mid & 0xFFU));
    appendMqttString(body, sub);
    body.push_back(static_cast<uint8_t>(qos & 0x03));

    std::vector<uint8_t> pkt;
    pkt.push_back(0x82U); // SUBSCRIBE (QoS 1 required in fixed header)
    encodeRemainingLength(pkt, body.size());
    pkt.insert(pkt.end(), body.begin(), body.end());
    return pkt;
}

bool sendAllBytes(SocketHandle sock,
                  const uint8_t *data,
                  size_t len,
                  const std::atomic<bool> &running)
{
    size_t sentTotal = 0;
    while (sentTotal < len && running.load()) {
#ifdef _WIN32
        int n = send(sock,
                     reinterpret_cast<const char *>(data + sentTotal),
                     static_cast<int>(len - sentTotal),
                     0);
        if (n > 0) {
            sentTotal += static_cast<size_t>(n);
            continue;
        }
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sock, &wfds);
            timeval tv{0, 25000};
            select(0, nullptr, &wfds, nullptr, &tv);
            continue;
        }
        return false;
#else
        ssize_t n = send(sock, data + sentTotal, len - sentTotal, 0);
        if (n > 0) {
            sentTotal += static_cast<size_t>(n);
            continue;
        }
        return false;
#endif
    }
    return sentTotal == len;
}

} // namespace

struct mosquitto {
    std::string _clientId;
    bool _cleanSession = true;
    void *_userdata = nullptr;

    bool _hasUsername = false;
    bool _hasPassword = false;
    std::string _username;
    std::string _password;

    std::string _host = "localhost";
    int _port = 1883;
    int _keepalive = 60;

    unsigned int _reconnectDelay = 1;
    unsigned int _reconnectDelayMax = 10;
    bool _reconnectExpBackoff = false;

    void (*_onConnect)(struct mosquitto *, void *, int) = nullptr;
    void (*_onDisconnect)(struct mosquitto *, void *, int) = nullptr;
    void (*_onPublish)(struct mosquitto *, void *, int) = nullptr;
    void (*_onMessage)(struct mosquitto *, void *, const struct mosquitto_message *) = nullptr;

    std::mutex _mutex;
    std::thread _loopThread;
    std::atomic<bool> _running{false};
    std::atomic<bool> _wantConnect{false};
    std::atomic<bool> _connected{false};
    SocketHandle _sock = kInvalidSocket;

    struct QueuedPacket {
        std::vector<uint8_t> bytes;
        int publishMid = 0;
        int qos = 0;
    };
    std::deque<QueuedPacket> _txQueue;
    int _nextMid = 1;

    void runLoop();
    bool tryOpenSocket(SocketHandle &outSock);
    bool processRxBuffer(std::vector<uint8_t> &rxBuf, SocketHandle sock);
};

bool mosquitto::tryOpenSocket(SocketHandle &outSock)
{
    outSock = kInvalidSocket;
    std::string host;
    int port = 1883;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        host = _host;
        port = _port;
    }

    if (host.empty() || port <= 0 || port > 65535) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || res == nullptr) {
        return false;
    }

    for (addrinfo *p = res; p != nullptr && _running.load() && _wantConnect.load(); p = p->ai_next) {
        SocketHandle s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == kInvalidSocket) {
            continue;
        }
        setSocketNonBlocking(s, true);

        int rc = connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen));
        if (rc == 0) {
            outSock = s;
            break;
        }

#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
            closeSocketHandle(s);
            continue;
        }
#endif
        bool connectedOk = false;
        for (int slice = 0; slice < 40 && _running.load() && _wantConnect.load(); ++slice) {
            fd_set wfds;
            fd_set efds;
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            FD_SET(s, &wfds);
            FD_SET(s, &efds);
            timeval tv{0, 25000}; // 25 ms per slice (max 1 s)
            int sel = select(static_cast<int>(s + 1), nullptr, &wfds, &efds, &tv);
            if (sel > 0) {
                if (FD_ISSET(s, &efds)) {
                    break;
                }
                if (FD_ISSET(s, &wfds)) {
                    int soErr = 0;
#ifdef _WIN32
                    int optLen = sizeof(soErr);
                    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&soErr), &optLen);
#else
                    socklen_t optLen = sizeof(soErr);
                    getsockopt(s, SOL_SOCKET, SO_ERROR, &soErr, &optLen);
#endif
                    if (soErr == 0) {
                        connectedOk = true;
                    }
                    break;
                }
            } else if (sel < 0) {
                break;
            }
        }

        if (connectedOk) {
            outSock = s;
            break;
        }
        closeSocketHandle(s);
    }

    freeaddrinfo(res);
    return outSock != kInvalidSocket;
}

bool mosquitto::processRxBuffer(std::vector<uint8_t> &rxBuf, SocketHandle sock)
{
    while (rxBuf.size() >= 2U) {
        uint8_t fixedHdr = rxBuf[0];
        size_t multiplier = 1U;
        size_t remLen = 0U;
        size_t idx = 1U;
        bool completeLen = false;

        while (idx < rxBuf.size() && idx <= 4U) {
            uint8_t encoded = rxBuf[idx++];
            remLen += static_cast<size_t>(encoded & 0x7FU) * multiplier;
            multiplier *= 128U;
            if ((encoded & 0x80U) == 0U) {
                completeLen = true;
                break;
            }
        }

        if (!completeLen) {
            if (idx > 4U) {
                return false; // Malformed remaining length
            }
            return true; // Need more bytes
        }

        size_t totalPktLen = idx + remLen;
        if (rxBuf.size() < totalPktLen) {
            return true; // Wait for full packet
        }

        const uint8_t *body = rxBuf.data() + idx;
        uint8_t pktType = static_cast<uint8_t>((fixedHdr >> 4) & 0x0FU);

        if (pktType == 2U) { // CONNACK
            int rc = (remLen >= 2U) ? static_cast<int>(body[1]) : MOSQ_ERR_PROTOCOL;
            if (rc == 0) {
                _connected.store(true);
            }
            void (*cb)(struct mosquitto *, void *, int) = nullptr;
            void *ud = nullptr;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                cb = _onConnect;
                ud = _userdata;
            }
            if (cb != nullptr) {
                cb(this, ud, rc);
            }
            if (rc != 0) {
                return false;
            }
        } else if (pktType == 3U) { // PUBLISH
            int qos = static_cast<int>((fixedHdr >> 1) & 0x03U);
            bool retain = (fixedHdr & 0x01U) != 0U;
            if (remLen >= 2U) {
                uint16_t topicLen = static_cast<uint16_t>((static_cast<uint16_t>(body[0]) << 8) | body[1]);
                size_t offset = 2U + topicLen;
                uint16_t pktMid = 0U;
                if (qos > 0) {
                    offset += 2U;
                }
                if (offset <= remLen) {
                    std::string topicStr(reinterpret_cast<const char *>(body + 2), topicLen);
                    if (qos > 0) {
                        pktMid = static_cast<uint16_t>(
                            (static_cast<uint16_t>(body[2 + topicLen]) << 8) |
                            body[3 + topicLen]);
                        uint8_t puback[4] = {
                            0x40U, 0x02U,
                            static_cast<uint8_t>((pktMid >> 8) & 0xFFU),
                            static_cast<uint8_t>(pktMid & 0xFFU)
                        };
                        sendAllBytes(sock, puback, sizeof(puback), _running);
                    }

                    const uint8_t *payloadPtr = body + offset;
                    int payloadLen = static_cast<int>(remLen - offset);
                    std::vector<char> topicMutable(topicStr.begin(), topicStr.end());
                    topicMutable.push_back('\0');
                    std::vector<uint8_t> payloadCopy(payloadPtr, payloadPtr + payloadLen);

                    mosquitto_message msg{};
                    msg.mid = static_cast<int>(pktMid);
                    msg.topic = topicMutable.data();
                    msg.payload = payloadCopy.empty() ? nullptr : payloadCopy.data();
                    msg.payloadlen = payloadLen;
                    msg.qos = qos;
                    msg.retain = retain;

                    void (*msgCb)(struct mosquitto *, void *, const struct mosquitto_message *) = nullptr;
                    void *ud = nullptr;
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
                        msgCb = _onMessage;
                        ud = _userdata;
                    }
                    if (msgCb != nullptr) {
                        msgCb(this, ud, &msg);
                    }
                }
            }
        } else if (pktType == 4U) { // PUBACK
            if (remLen >= 2U) {
                int mid = static_cast<int>((static_cast<uint16_t>(body[0]) << 8) | body[1]);
                void (*pubCb)(struct mosquitto *, void *, int) = nullptr;
                void *ud = nullptr;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    pubCb = _onPublish;
                    ud = _userdata;
                }
                if (pubCb != nullptr) {
                    pubCb(this, ud, mid);
                }
            }
        }

        rxBuf.erase(rxBuf.begin(), rxBuf.begin() + static_cast<std::ptrdiff_t>(totalPktLen));
    }
    return true;
}

void mosquitto::runLoop()
{
    while (_running.load()) {
        if (!_wantConnect.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        SocketHandle sock = kInvalidSocket;
        if (!tryOpenSocket(sock)) {
            void (*connCb)(struct mosquitto *, void *, int) = nullptr;
            void *ud = nullptr;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                connCb = _onConnect;
                ud = _userdata;
            }
            if (connCb != nullptr && _running.load() && _wantConnect.load()) {
                connCb(this, ud, MOSQ_ERR_CONN_REFUSED);
            }
            for (int i = 0; i < 20 && _running.load() && _wantConnect.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            continue;
        }

        std::vector<uint8_t> connectPkt;
        int keepaliveSec = 60;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _sock = sock;
            keepaliveSec = (_keepalive > 5) ? _keepalive : 60;
            connectPkt = buildConnectPacket(_clientId,
                                            _cleanSession,
                                            static_cast<uint16_t>(keepaliveSec),
                                            _hasUsername,
                                            _username,
                                            _hasPassword,
                                            _password);
        }

        if (!sendAllBytes(sock, connectPkt.data(), connectPkt.size(), _running)) {
            std::lock_guard<std::mutex> lock(_mutex);
            closeSocketHandle(_sock);
            continue;
        }

        std::vector<uint8_t> rxBuf;
        rxBuf.reserve(4096);
        auto lastPingTime = std::chrono::steady_clock::now();

        while (_running.load() && _wantConnect.load()) {
            // 1. Flush outgoing packets if CONNACK received
            if (_connected.load()) {
                std::deque<QueuedPacket> pending;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    pending.swap(_txQueue);
                }
                bool sendFailed = false;
                for (const auto &qp : pending) {
                    if (!sendAllBytes(sock, qp.bytes.data(), qp.bytes.size(), _running)) {
                        sendFailed = true;
                        break;
                    }
                    if (qp.publishMid > 0 && qp.qos == 0) {
                        void (*pubCb)(struct mosquitto *, void *, int) = nullptr;
                        void *ud = nullptr;
                        {
                            std::lock_guard<std::mutex> lock(_mutex);
                            pubCb = _onPublish;
                            ud = _userdata;
                        }
                        if (pubCb != nullptr) {
                            pubCb(this, ud, qp.publishMid);
                        }
                    }
                }
                if (sendFailed) {
                    break;
                }
            }

            // 2. Poll incoming socket readable state
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            timeval tv{0, 25000}; // 25 ms
            int sel = select(static_cast<int>(sock + 1), &rfds, nullptr, nullptr, &tv);
            if (sel > 0 && FD_ISSET(sock, &rfds)) {
                uint8_t chunk[2048];
#ifdef _WIN32
                int n = recv(sock, reinterpret_cast<char *>(chunk), sizeof(chunk), 0);
#else
                ssize_t n = recv(sock, chunk, sizeof(chunk), 0);
#endif
                if (n <= 0) {
                    break;
                }
                rxBuf.insert(rxBuf.end(), chunk, chunk + n);
                if (!processRxBuffer(rxBuf, sock)) {
                    break;
                }
            } else if (sel < 0) {
                break;
            }

            // 3. Periodic PINGREQ
            if (_connected.load()) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
                    now - lastPingTime).count();
                if (elapsedSec >= (keepaliveSec / 2)) {
                    const uint8_t pingReq[2] = {0xC0U, 0x00U};
                    if (!sendAllBytes(sock, pingReq, sizeof(pingReq), _running)) {
                        break;
                    }
                    lastPingTime = now;
                }
            }
        }

        bool wasConnected = _connected.exchange(false);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_sock != kInvalidSocket) {
                if (wasConnected) {
                    const uint8_t discPkt[2] = {0xE0U, 0x00U};
                    send(_sock, reinterpret_cast<const char *>(discPkt), 2, 0);
                }
                closeSocketHandle(_sock);
            }
        }

        if (wasConnected) {
            void (*discCb)(struct mosquitto *, void *, int) = nullptr;
            void *ud = nullptr;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                discCb = _onDisconnect;
                ud = _userdata;
            }
            if (discCb != nullptr) {
                discCb(this, ud, 0);
            }
        }
    }
}

extern "C" {

int mosquitto_lib_init(void)
{
    int prev = g_wsaRefCount.fetch_add(1);
#ifdef _WIN32
    if (prev == 0) {
        WSADATA wsaData{};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            g_wsaRefCount.fetch_sub(1);
            return MOSQ_ERR_ERRNO;
        }
    }
#else
    (void)prev;
#endif
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_lib_cleanup(void)
{
    int cur = g_wsaRefCount.fetch_sub(1);
#ifdef _WIN32
    if (cur == 1) {
        WSACleanup();
    }
#else
    (void)cur;
#endif
    return MOSQ_ERR_SUCCESS;
}

struct mosquitto *mosquitto_new(const char *id, bool clean_session, void *obj)
{
    auto *m = new mosquitto();
    if (id != nullptr && id[0] != '\0') {
        m->_clientId = id;
    } else {
        unsigned int seq = g_clientCounter.fetch_add(1);
#ifdef _WIN32
        unsigned long pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
        unsigned long pid = static_cast<unsigned long>(getpid());
#endif
        m->_clientId = "robohero-" + std::to_string(pid) + "-" + std::to_string(seq);
    }
    m->_cleanSession = clean_session;
    m->_userdata = obj;
    return m;
}

void mosquitto_destroy(struct mosquitto *mosq)
{
    if (mosq == nullptr) {
        return;
    }
    mosquitto_disconnect(mosq);
    mosquitto_loop_stop(mosq, true);
    delete mosq;
}

int mosquitto_username_pw_set(struct mosquitto *mosq,
                              const char *username,
                              const char *password)
{
    if (mosq == nullptr) {
        return MOSQ_ERR_INVAL;
    }
    std::lock_guard<std::mutex> lock(mosq->_mutex);
    if (username != nullptr && username[0] != '\0') {
        mosq->_hasUsername = true;
        mosq->_username = username;
        if (password != nullptr) {
            mosq->_hasPassword = true;
            mosq->_password = password;
        } else {
            mosq->_hasPassword = false;
            mosq->_password.clear();
        }
    } else {
        mosq->_hasUsername = false;
        mosq->_hasPassword = false;
        mosq->_username.clear();
        mosq->_password.clear();
    }
    return MOSQ_ERR_SUCCESS;
}

void mosquitto_connect_callback_set(
    struct mosquitto *mosq,
    void (*on_connect)(struct mosquitto *, void *, int))
{
    if (mosq == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mosq->_mutex);
    mosq->_onConnect = on_connect;
}

void mosquitto_disconnect_callback_set(
    struct mosquitto *mosq,
    void (*on_disconnect)(struct mosquitto *, void *, int))
{
    if (mosq == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mosq->_mutex);
    mosq->_onDisconnect = on_disconnect;
}

void mosquitto_publish_callback_set(
    struct mosquitto *mosq,
    void (*on_publish)(struct mosquitto *, void *, int))
{
    if (mosq == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mosq->_mutex);
    mosq->_onPublish = on_publish;
}

void mosquitto_message_callback_set(
    struct mosquitto *mosq,
    void (*on_message)(struct mosquitto *, void *, const struct mosquitto_message *))
{
    if (mosq == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mosq->_mutex);
    mosq->_onMessage = on_message;
}

int mosquitto_reconnect_delay_set(struct mosquitto *mosq,
                                  unsigned int reconnect_delay,
                                  unsigned int reconnect_delay_max,
                                  bool reconnect_exponential_backoff)
{
    if (mosq == nullptr) {
        return MOSQ_ERR_INVAL;
    }
    std::lock_guard<std::mutex> lock(mosq->_mutex);
    mosq->_reconnectDelay = reconnect_delay;
    mosq->_reconnectDelayMax = reconnect_delay_max;
    mosq->_reconnectExpBackoff = reconnect_exponential_backoff;
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_connect_async(struct mosquitto *mosq,
                            const char *host,
                            int port,
                            int keepalive)
{
    if (mosq == nullptr || host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
        return MOSQ_ERR_INVAL;
    }
    {
        std::lock_guard<std::mutex> lock(mosq->_mutex);
        mosq->_host = host;
        mosq->_port = port;
        mosq->_keepalive = keepalive > 0 ? keepalive : 60;
        mosq->_txQueue.clear();
    }
    mosq->_wantConnect.store(true);
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_connect(struct mosquitto *mosq,
                      const char *host,
                      int port,
                      int keepalive)
{
    return mosquitto_connect_async(mosq, host, port, keepalive);
}

int mosquitto_disconnect(struct mosquitto *mosq)
{
    if (mosq == nullptr) {
        return MOSQ_ERR_INVAL;
    }
    mosq->_wantConnect.store(false);
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_loop_start(struct mosquitto *mosq)
{
    if (mosq == nullptr) {
        return MOSQ_ERR_INVAL;
    }
    bool expected = false;
    if (mosq->_running.compare_exchange_strong(expected, true)) {
        if (mosq->_loopThread.joinable()) {
            mosq->_loopThread.join();
        }
        mosq->_loopThread = std::thread([mosq]() {
            mosq->runLoop();
        });
    }
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_loop_stop(struct mosquitto *mosq, bool force)
{
    (void)force;
    if (mosq == nullptr) {
        return MOSQ_ERR_INVAL;
    }
    mosq->_wantConnect.store(false);
    mosq->_running.store(false);
    if (mosq->_loopThread.joinable()) {
        if (mosq->_loopThread.get_id() != std::this_thread::get_id()) {
            mosq->_loopThread.join();
        }
    }
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_publish(struct mosquitto *mosq,
                      int *mid,
                      const char *topic,
                      int payloadlen,
                      const void *payload,
                      int qos,
                      bool retain)
{
    if (mosq == nullptr || topic == nullptr || topic[0] == '\0' || payloadlen < 0) {
        return MOSQ_ERR_INVAL;
    }
    if (!mosq->_connected.load()) {
        return MOSQ_ERR_NO_CONN;
    }

    std::lock_guard<std::mutex> lock(mosq->_mutex);
    int assignedMid = mosq->_nextMid++;
    if (mosq->_nextMid > 65535) {
        mosq->_nextMid = 1;
    }
    if (mid != nullptr) {
        *mid = assignedMid;
    }

    mosquitto::QueuedPacket qp;
    qp.bytes = buildPublishPacket(topic,
                                  static_cast<uint16_t>(assignedMid),
                                  payload,
                                  payloadlen,
                                  qos,
                                  retain);
    qp.publishMid = assignedMid;
    qp.qos = qos;
    mosq->_txQueue.push_back(std::move(qp));
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_subscribe(struct mosquitto *mosq,
                        int *mid,
                        const char *sub,
                        int qos)
{
    if (mosq == nullptr || sub == nullptr || sub[0] == '\0') {
        return MOSQ_ERR_INVAL;
    }
    if (!mosq->_connected.load()) {
        return MOSQ_ERR_NO_CONN;
    }

    std::lock_guard<std::mutex> lock(mosq->_mutex);
    int assignedMid = mosq->_nextMid++;
    if (mosq->_nextMid > 65535) {
        mosq->_nextMid = 1;
    }
    if (mid != nullptr) {
        *mid = assignedMid;
    }

    mosquitto::QueuedPacket qp;
    qp.bytes = buildSubscribePacket(static_cast<uint16_t>(assignedMid), sub, qos);
    qp.publishMid = 0;
    qp.qos = qos;
    mosq->_txQueue.push_back(std::move(qp));
    return MOSQ_ERR_SUCCESS;
}

const char *mosquitto_strerror(int mosq_errno)
{
    switch (mosq_errno) {
    case MOSQ_ERR_SUCCESS:
        return "No error";
    case MOSQ_ERR_NOMEM:
        return "Out of memory";
    case MOSQ_ERR_PROTOCOL:
        return "MQTT protocol error";
    case MOSQ_ERR_INVAL:
        return "Invalid function arguments provided";
    case MOSQ_ERR_NO_CONN:
        return "The client is not currently connected";
    case MOSQ_ERR_CONN_REFUSED:
        return "Connection refused";
    case MOSQ_ERR_NOT_FOUND:
        return "Message not found";
    case MOSQ_ERR_CONN_LOST:
        return "Connection lost";
    case MOSQ_ERR_TLS:
        return "TLS error";
    case MOSQ_ERR_PAYLOAD_SIZE:
        return "Payload too large";
    case MOSQ_ERR_NOT_SUPPORTED:
        return "Functionality not supported";
    case MOSQ_ERR_AUTH:
        return "Authentication failed";
    case MOSQ_ERR_ACL_DENIED:
        return "Access denied by ACL";
    default:
        return "Unknown MQTT error";
    }
}

} // extern "C"

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
