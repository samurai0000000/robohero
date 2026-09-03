/*
 * app_main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "Mqtt.hxx"
#include "RoboHero.hxx"
#include "Shell.hxx"
#include "Store.hxx"
#include "Web.hxx"

static const char *TAG = "main";

static void wifiUpStartWeb(void)
{
    Web::instance().start();
}

extern "C" void app_main(void)
{
    RoboHero::consoleBegin();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    RoboHero::instance().onWifiUp(wifiUpStartWeb);
    RoboHero::instance().start();

    if (RoboHero::instance().wifiReady()) {
        Web::instance().start();
    }

    if (Store::instance().mqttEnabled() && RoboHero::instance().wifiReady()) {
        Mqtt::instance().start();
    }

    uint32_t heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "free heap after wifi+http: %u", (unsigned) heap);
    if (heap < 12000) {
        ESP_LOGE(TAG, "heap below 12 KB after bring-up");
    }

    Shell::instance().start();
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
