/*
 * mosquitto.h
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MINGW_COMPAT_MOSQUITTO_H
#define ROBOHERO_MINGW_COMPAT_MOSQUITTO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOSQ_ERR_SUCCESS        0
#define MOSQ_ERR_NOMEM          1
#define MOSQ_ERR_PROTOCOL       2
#define MOSQ_ERR_INVAL          3
#define MOSQ_ERR_NO_CONN        4
#define MOSQ_ERR_CONN_REFUSED   5
#define MOSQ_ERR_NOT_FOUND      6
#define MOSQ_ERR_CONN_LOST      7
#define MOSQ_ERR_TLS            8
#define MOSQ_ERR_PAYLOAD_SIZE   9
#define MOSQ_ERR_NOT_SUPPORTED  10
#define MOSQ_ERR_AUTH           11
#define MOSQ_ERR_ACL_DENIED     12
#define MOSQ_ERR_UNKNOWN        13
#define MOSQ_ERR_ERRNO          14

struct mosquitto;

struct mosquitto_message {
    int mid;
    char *topic;
    void *payload;
    int payloadlen;
    int qos;
    bool retain;
};

int mosquitto_lib_init(void);
int mosquitto_lib_cleanup(void);

struct mosquitto *mosquitto_new(const char *id, bool clean_session, void *obj);
void mosquitto_destroy(struct mosquitto *mosq);

int mosquitto_username_pw_set(struct mosquitto *mosq,
                              const char *username,
                              const char *password);

void mosquitto_connect_callback_set(
    struct mosquitto *mosq,
    void (*on_connect)(struct mosquitto *, void *, int));

void mosquitto_disconnect_callback_set(
    struct mosquitto *mosq,
    void (*on_disconnect)(struct mosquitto *, void *, int));

void mosquitto_publish_callback_set(
    struct mosquitto *mosq,
    void (*on_publish)(struct mosquitto *, void *, int));

void mosquitto_message_callback_set(
    struct mosquitto *mosq,
    void (*on_message)(struct mosquitto *, void *, const struct mosquitto_message *));

int mosquitto_reconnect_delay_set(struct mosquitto *mosq,
                                  unsigned int reconnect_delay,
                                  unsigned int reconnect_delay_max,
                                  bool reconnect_exponential_backoff);

int mosquitto_connect(struct mosquitto *mosq,
                      const char *host,
                      int port,
                      int keepalive);

int mosquitto_connect_async(struct mosquitto *mosq,
                            const char *host,
                            int port,
                            int keepalive);

int mosquitto_disconnect(struct mosquitto *mosq);

int mosquitto_loop_start(struct mosquitto *mosq);
int mosquitto_loop_stop(struct mosquitto *mosq, bool force);

int mosquitto_publish(struct mosquitto *mosq,
                      int *mid,
                      const char *topic,
                      int payloadlen,
                      const void *payload,
                      int qos,
                      bool retain);

int mosquitto_subscribe(struct mosquitto *mosq,
                        int *mid,
                        const char *sub,
                        int qos);

const char *mosquitto_strerror(int mosq_errno);

#ifdef __cplusplus
}
#endif

#endif /* ROBOHERO_MINGW_COMPAT_MOSQUITTO_H */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
