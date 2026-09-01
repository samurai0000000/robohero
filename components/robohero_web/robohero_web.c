/*
 * robohero_web.c
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "robohero_app.h"
#include "robohero_servo.h"
#include "robohero_store.h"
#include "robohero_web.h"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t calibrate_html_start[] asm("_binary_calibrate_html_start");
extern const uint8_t calibrate_html_end[] asm("_binary_calibrate_html_end");

static void no_cache(httpd_req_t *req)
{
    httpd_resp_set_hdr(
        req, "Cache-Control", "no-cache, no-store, must-revalidate");
}

static void send_json(httpd_req_t *req, const char *status, const char *body)
{
    no_cache(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status);
    httpd_resp_send(req, body, strlen(body));
}

static void
send_html(httpd_req_t *req, const uint8_t *start, const uint8_t *end)
{
    no_cache(req);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *) start, end - start);
}

static bool
query_get(const char *query, const char *key, char *out, size_t outlen)
{
    return query && httpd_query_key_value(query, key, out, outlen) == ESP_OK;
}

static esp_err_t handle_index(httpd_req_t *req)
{
    char query[256];
    char val[32];
    query[0] = '\0';
    if (httpd_req_get_url_query_len(req) + 1 <= sizeof(query)) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    if (query_get(query, "stop", val, sizeof(val))) {
        bool busy = robohero_app_request_stop();
        send_json(req,
                  "200 OK",
                  busy ? "{\"status\":\"ok\",\"stopped\":true}"
                       : "{\"status\":\"ok\",\"stopped\":false}");
        return ESP_OK;
    }

    if (query_get(query, "pm", val, sizeof(val))) {
        if (robohero_app_is_low_voltage()) {
            send_json(
                req, "503 Service Unavailable", "{\"status\":\"voltage_low\"}");
            return ESP_OK;
        }
        int pm = atoi(val);
        robohero_app_submit_pm(pm);
        char body[64];
        snprintf(body, sizeof(body), "{\"status\":\"ok\",\"pm\":%d}", pm);
        send_json(req, "200 OK", body);
        return ESP_OK;
    }

    if (query_get(query, "pms", val, sizeof(val))) {
        if (robohero_app_is_low_voltage()) {
            send_json(
                req, "503 Service Unavailable", "{\"status\":\"voltage_low\"}");
            return ESP_OK;
        }
        int pms = atoi(val);
        robohero_app_submit_pms(pms);
        char body[64];
        snprintf(body, sizeof(body), "{\"status\":\"ok\",\"pms\":%d}", pms);
        send_json(req, "200 OK", body);
        return ESP_OK;
    }

    if (robohero_app_is_busy()) {
        send_json(req, "503 Service Unavailable", "{\"status\":\"busy\"}");
        return ESP_OK;
    }

    send_html(req, index_html_start, index_html_end);
    return ESP_OK;
}

static esp_err_t handle_calibrate(httpd_req_t *req)
{
    if (robohero_app_is_busy()) {
        send_json(req, "503 Service Unavailable", "{\"status\":\"busy\"}");
        return ESP_OK;
    }

    char query[512];
    char val[32];
    query[0] = '\0';
    if (httpd_req_get_url_query_len(req) + 1 <= sizeof(query)) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    if (query_get(query, "apply", val, sizeof(val)) &&
        query_get(query, "key", val, sizeof(val))) {
        int key = atoi(val);
        if (!query_get(query, "val", val, sizeof(val))) {
            send_json(req, "400 Bad Request", "{\"status\":\"error\"}");
            return ESP_OK;
        }
        int ival = atoi(val);
        if (ival < -125) {
            ival = -125;
        }
        if (ival > 125) {
            ival = 125;
        }
        servo_apply_trim(key, (int8_t) ival);
        if (key == STORE_KEY_VOLTAGE_CAL) {
            robohero_app_reset_low_voltage();
        }
        char body[96];
        snprintf(body,
                 sizeof(body),
                 "{\"status\":\"ok\",\"applied\":true,\"key\":%d,\"val\":%d}",
                 key,
                 ival);
        send_json(req, "200 OK", body);
        return ESP_OK;
    }

    if (query_get(query, "save", val, sizeof(val))) {
        for (int i = 0; i <= 19; i++) {
            char keyname[8];
            snprintf(keyname, sizeof(keyname), "t%u", (unsigned) i);
            if (query_get(query, keyname, val, sizeof(val))) {
                int ival = atoi(val);
                if (ival < -125) {
                    ival = -125;
                }
                if (ival > 125) {
                    ival = 125;
                }
                servo_apply_trim(i, (int8_t) ival);
            }
        }
        if (query_get(query, "t19", val, sizeof(val))) {
            robohero_app_reset_low_voltage();
        }
        bool saved = store_save();
        send_json(req,
                  "200 OK",
                  saved ? "{\"status\":\"ok\",\"msg\":\"Saved to EEPROM!\"}"
                        : "{\"status\":\"error\","
                          "\"msg\":\"Failed to save EEPROM\"}");
        return ESP_OK;
    }

    if (query_get(query, "json", val, sizeof(val))) {
        char json[160];
        int n = snprintf(json, sizeof(json), "{\"trims\":[");
        for (int i = 0; i <= 19; i++) {
            n += snprintf(json + n,
                          sizeof(json) - (size_t) n,
                          "%s%d",
                          (i > 0) ? "," : "",
                          (int) store_read_key(i));
        }
        snprintf(json + n, sizeof(json) - (size_t) n, "]}");
        send_json(req, "200 OK", json);
        return ESP_OK;
    }

    if (query_get(query, "pose", val, sizeof(val))) {
        if (strcmp(val, "zero") == 0) {
            robohero_app_submit_zero();
        } else if (strcmp(val, "center") == 0) {
            robohero_app_submit_center();
        }
        char body[64];
        snprintf(
            body, sizeof(body), "{\"status\":\"ok\",\"pose\":\"%s\"}", val);
        send_json(req, "200 OK", body);
        return ESP_OK;
    }

    send_html(req, calibrate_html_start, calibrate_html_end);
    return ESP_OK;
}

static httpd_handle_t s_server;

void web_start(void)
{
    if (s_server) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.stack_size = 5120;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return;
    }

    const httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_index,
    };
    const httpd_uri_t cal_uri = {
        .uri = "/calibrate",
        .method = HTTP_GET,
        .handler = handle_calibrate,
    };
    httpd_register_uri_handler(s_server, &index_uri);
    httpd_register_uri_handler(s_server, &cal_uri);
    ESP_LOGI(TAG, "HTTP server on port %d", config.server_port);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
