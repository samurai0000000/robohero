/*
 * msg.h
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Packed MQTT wire format. C ABI for firmware and host.
 */

#ifndef ROBOHERO_MSG_H
#define ROBOHERO_MSG_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RH_SERVO_COUNT 17

#define RH_MSG_MAGIC 0x31424852u /* 'RHB1' LE */

#define RH_MSG_STATUS  1u
#define RH_MSG_STOP    2u
#define RH_MSG_CENTER  3u
#define RH_MSG_ZERO    4u
#define RH_MSG_RELAX   5u
#define RH_MSG_PM      6u
#define RH_MSG_PMS     7u
#define RH_MSG_SET_PWM 8u

#define RH_TLV_TIME    1u /* len=4, uint32 ms */
#define RH_TLV_VOLTAGE 2u /* len=2, int16 */
#define RH_TLV_SERVO   3u /* len=3, rh_tlv_servo */
#define RH_TLV_PROG    4u /* len=2, int16 program id */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t msg_type;
    uint8_t payload_len;
} rh_msg_hdr;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t len;
} rh_tlv;

typedef struct __attribute__((packed)) {
    uint8_t chan;
    int16_t pos;
} rh_tlv_servo;

typedef struct {
    uint8_t msg_type;
    uint8_t payload_len;
    const uint8_t *payload;
} rh_msg_view;

static inline int rh_msg_begin(uint8_t *buf, size_t cap, uint8_t msg_type)
{
    rh_msg_hdr hdr;
    uint32_t magic;

    if (buf == NULL || cap < sizeof(hdr)) {
        return -1;
    }
    magic = RH_MSG_MAGIC;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(&hdr.magic, &magic, sizeof(magic));
    hdr.msg_type = msg_type;
    memcpy(buf, &hdr, sizeof(hdr));
    return (int) sizeof(hdr);
}

static inline int rh_tlv_put(uint8_t *buf,
                             size_t cap,
                             int used,
                             uint8_t type,
                             const void *val,
                             uint8_t len)
{
    size_t need;

    if (buf == NULL || used < (int) sizeof(rh_msg_hdr)) {
        return -1;
    }
    need = (size_t) used + 2u + (size_t) len;
    if (need > cap) {
        return -1;
    }
    buf[used] = type;
    buf[used + 1] = len;
    if (len > 0) {
        if (val == NULL) {
            return -1;
        }
        memcpy(buf + used + 2, val, len);
    }
    return (int) need;
}

static inline int rh_msg_finish(uint8_t *buf, int used)
{
    int n;
    uint8_t plen;

    if (buf == NULL) {
        return -1;
    }
    n = used - (int) sizeof(rh_msg_hdr);
    if (n < 0 || n > 255) {
        return -1;
    }
    plen = (uint8_t) n;
    memcpy(buf + offsetof(rh_msg_hdr, payload_len), &plen, sizeof(plen));
    return used;
}

static inline int rh_msg_empty(uint8_t *buf, size_t cap, uint8_t msg_type)
{
    int n;

    n = rh_msg_begin(buf, cap, msg_type);
    if (n < 0) {
        return -1;
    }
    return rh_msg_finish(buf, n);
}

static inline int
rh_msg_parse(const void *data, int len, rh_msg_view *out)
{
    rh_msg_hdr hdr;
    uint32_t magic;

    if (data == NULL || out == NULL || len < (int) sizeof(hdr)) {
        return -1;
    }
    memcpy(&hdr, data, sizeof(hdr));
    memcpy(&magic, &hdr.magic, sizeof(magic));
    if (magic != RH_MSG_MAGIC) {
        return -1;
    }
    if (len != (int) sizeof(hdr) + (int) hdr.payload_len) {
        return -1;
    }
    out->msg_type = hdr.msg_type;
    out->payload_len = hdr.payload_len;
    out->payload = (const uint8_t *) data + sizeof(hdr);
    return 0;
}

static inline int rh_tlv_next(const uint8_t *payload,
                              uint8_t payload_len,
                              int off,
                              uint8_t *type,
                              const uint8_t **val,
                              uint8_t *len)
{
    uint8_t n;

    if (payload == NULL || type == NULL || val == NULL || len == NULL) {
        return -1;
    }
    if (off < 0 || off + 2 > (int) payload_len) {
        return -1;
    }
    n = payload[off + 1];
    if (off + 2 + (int) n > (int) payload_len) {
        return -1;
    }
    *type = payload[off];
    *len = n;
    *val = payload + off + 2;
    return off + 2 + (int) n;
}

#ifdef __cplusplus
static_assert(sizeof(rh_msg_hdr) == 6, "rh_msg_hdr");
static_assert(sizeof(rh_tlv_servo) == 3, "rh_tlv_servo");
}
#endif

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
