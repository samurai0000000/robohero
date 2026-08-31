/*
 * app_main.c
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "robohero_app.h"
#include "robohero_shell.h"
#include "robohero_web.h"

static const char *TAG = "main";

void app_main(void)
{
    robohero_console_begin();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    robohero_app_on_wifi_up(web_start);
    robohero_app_start();

    if (robohero_app_wifi_ready()) {
        web_start();
    }

    uint32_t heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "free heap after wifi+http: %u", (unsigned) heap);
    if (heap < 12000) {
        ESP_LOGE(TAG, "heap below 12 KB after bring-up");
    }

    shell_start();
}
