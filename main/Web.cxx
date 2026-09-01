/*
 * Web.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "Config.hxx"
#include "RoboHero.hxx"
#include "Servo.hxx"
#include "Store.hxx"
#include "Web.hxx"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t calibrate_html_start[] asm("_binary_calibrate_html_start");
extern const uint8_t calibrate_html_end[] asm("_binary_calibrate_html_end");

Web Web::_self;

Web &Web::instance()
{
    return _self;
}

Web::Web() : _server(NULL) {}

static void noCache(httpd_req_t *req)
{
    httpd_resp_set_hdr(
        req, "Cache-Control", "no-cache, no-store, must-revalidate");
}

static void sendJson(httpd_req_t *req, const char *status, const char *body)
{
    noCache(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status);
    httpd_resp_send(req, body, strlen(body));
}

static void sendHtml(httpd_req_t *req, const uint8_t *start, const uint8_t *end)
{
    noCache(req);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *) start, end - start);
}

static bool
queryGet(const char *query, const char *key, char *out, size_t outlen)
{
    return query && httpd_query_key_value(query, key, out, outlen) == ESP_OK;
}

static esp_err_t handleIndex(httpd_req_t *req)
{
    RoboHero &rh = RoboHero::instance();
    char query[256];
    char val[32];
    query[0] = '\0';
    if (httpd_req_get_url_query_len(req) + 1 <= sizeof(query)) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    if (queryGet(query, "stop", val, sizeof(val))) {
        bool busy = rh.requestStop();
        sendJson(req,
                 "200 OK",
                 busy ? "{\"status\":\"ok\",\"stopped\":true}"
                      : "{\"status\":\"ok\",\"stopped\":false}");
        return ESP_OK;
    }

    if (queryGet(query, "pm", val, sizeof(val))) {
        if (rh.isLowVoltage()) {
            sendJson(
                req, "503 Service Unavailable", "{\"status\":\"voltage_low\"}");
            return ESP_OK;
        }
        int pm = atoi(val);
        rh.submitPm(pm);
        char body[64];
        snprintf(body, sizeof(body), "{\"status\":\"ok\",\"pm\":%d}", pm);
        sendJson(req, "200 OK", body);
        return ESP_OK;
    }

    if (queryGet(query, "pms", val, sizeof(val))) {
        if (rh.isLowVoltage()) {
            sendJson(
                req, "503 Service Unavailable", "{\"status\":\"voltage_low\"}");
            return ESP_OK;
        }
        int pms = atoi(val);
        rh.submitPms(pms);
        char body[64];
        snprintf(body, sizeof(body), "{\"status\":\"ok\",\"pms\":%d}", pms);
        sendJson(req, "200 OK", body);
        return ESP_OK;
    }

    if (rh.isBusy()) {
        sendJson(req, "503 Service Unavailable", "{\"status\":\"busy\"}");
        return ESP_OK;
    }

    sendHtml(req, index_html_start, index_html_end);
    return ESP_OK;
}

static esp_err_t handleCalibrate(httpd_req_t *req)
{
    RoboHero &rh = RoboHero::instance();
    Store &st = Store::instance();

    if (rh.isBusy()) {
        sendJson(req, "503 Service Unavailable", "{\"status\":\"busy\"}");
        return ESP_OK;
    }

    char query[512];
    char val[32];
    query[0] = '\0';
    if (httpd_req_get_url_query_len(req) + 1 <= sizeof(query)) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    if (queryGet(query, "apply", val, sizeof(val)) &&
        queryGet(query, "key", val, sizeof(val))) {
        int key = atoi(val);
        if (!queryGet(query, "val", val, sizeof(val))) {
            sendJson(req, "400 Bad Request", "{\"status\":\"error\"}");
            return ESP_OK;
        }
        int ival = atoi(val);
        if (ival < -125) {
            ival = -125;
        }
        if (ival > 125) {
            ival = 125;
        }
        Servo::instance().applyTrim(key, (int8_t) ival);
        if (key == STORE_KEY_VOLTAGE_CAL) {
            rh.resetLowVoltage();
        }
        char body[96];
        snprintf(body,
                 sizeof(body),
                 "{\"status\":\"ok\",\"applied\":true,\"key\":%d,\"val\":%d}",
                 key,
                 ival);
        sendJson(req, "200 OK", body);
        return ESP_OK;
    }

    if (queryGet(query, "save", val, sizeof(val))) {
        for (int i = 0; i <= 19; i++) {
            char keyname[8];
            snprintf(keyname, sizeof(keyname), "t%u", (unsigned) i);
            if (queryGet(query, keyname, val, sizeof(val))) {
                int ival = atoi(val);
                if (ival < -125) {
                    ival = -125;
                }
                if (ival > 125) {
                    ival = 125;
                }
                Servo::instance().applyTrim(i, (int8_t) ival);
            }
        }
        if (queryGet(query, "t19", val, sizeof(val))) {
            rh.resetLowVoltage();
        }
        bool saved = st.save();
        sendJson(req,
                 "200 OK",
                 saved ? "{\"status\":\"ok\",\"msg\":\"Saved to EEPROM!\"}"
                       : "{\"status\":\"error\","
                         "\"msg\":\"Failed to save EEPROM\"}");
        return ESP_OK;
    }

    if (queryGet(query, "json", val, sizeof(val))) {
        char json[160];
        int n = snprintf(json, sizeof(json), "{\"trims\":[");
        for (int i = 0; i <= 19; i++) {
            n += snprintf(json + n,
                          sizeof(json) - (size_t) n,
                          "%s%d",
                          (i > 0) ? "," : "",
                          (int) st.readKey(i));
        }
        snprintf(json + n, sizeof(json) - (size_t) n, "]}");
        sendJson(req, "200 OK", json);
        return ESP_OK;
    }

    if (queryGet(query, "pose", val, sizeof(val))) {
        if (strcmp(val, "zero") == 0) {
            rh.submitZero();
        } else if (strcmp(val, "center") == 0) {
            rh.submitCenter();
        }
        char body[64];
        snprintf(
            body, sizeof(body), "{\"status\":\"ok\",\"pose\":\"%s\"}", val);
        sendJson(req, "200 OK", body);
        return ESP_OK;
    }

    sendHtml(req, calibrate_html_start, calibrate_html_end);
    return ESP_OK;
}

void Web::start()
{
    if (_server) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.stack_size = 5120;

    httpd_handle_t handle = NULL;
    esp_err_t err = httpd_start(&handle, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        _server = NULL;
        return;
    }
    _server = handle;

    httpd_uri_t indexUri;
    memset(&indexUri, 0, sizeof(indexUri));
    indexUri.uri = "/";
    indexUri.method = HTTP_GET;
    indexUri.handler = handleIndex;

    httpd_uri_t calUri;
    memset(&calUri, 0, sizeof(calUri));
    calUri.uri = "/calibrate";
    calUri.method = HTTP_GET;
    calUri.handler = handleCalibrate;
    httpd_register_uri_handler((httpd_handle_t) _server, &indexUri);
    httpd_register_uri_handler((httpd_handle_t) _server, &calUri);
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
