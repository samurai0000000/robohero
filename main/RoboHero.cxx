/*
 * RoboHero.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp8266/eagle_soc.h"
extern "C" {
#include "rom/uart.h"
}
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

#include "Config.hxx"
#include "Motions.hxx"
#include "Mqtt.hxx"
#include "RoboHero.hxx"
#include "Servo.hxx"
#include "Store.hxx"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_AP_STARTED_BIT BIT1
#define CMD_QUEUE_LEN       1

static const char *TAG = "app";

RoboHero RoboHero::_self;

RoboHero &RoboHero::instance()
{
    return _self;
}

RoboHero::RoboHero()
    : _cmdQueue(NULL), _wifiEvents(NULL), _busy(false), _cancel(false),
      _voltageLow(0), _voltage(INPUT_VOLTAGE), _engineering(0), _voltageCab(0),
      _wifiReady(false), _wifiUpFn(NULL)
{
}

void RoboHero::onWifiUp(RoboHeroWifiUpFn fn)
{
    _wifiUpFn = fn;
}

void RoboHero::notifyWifiUp()
{
    if (_wifiUpFn) {
        _wifiUpFn();
    }
}

int RoboHero::mapInt(int x, int inMin, int inMax, int outMin, int outMax)
{
    if (inMax == inMin) {
        return outMin;
    }
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

bool RoboHero::yieldCancel(void *ctx)
{
    (void) ctx;
    return instance()._cancel;
}

bool RoboHero::interruptibleDelay(int ms)
{
    while (ms > 0) {
        int slice = (ms < BASEDELAYTIME) ? ms : BASEDELAYTIME;
        vTaskDelay(pdMS_TO_TICKS(slice));
        ms -= slice;
        if (_cancel) {
            return false;
        }
    }
    return true;
}

bool RoboHero::runThenCenter(const int matrix[][ALLMATRIX], int steps)
{
    Servo::instance().programRun(matrix, steps);
    Servo::instance().programCenter();
    return !_cancel;
}

void RoboHero::executePm(int prog)
{
    Servo &sv = Servo::instance();
    ESP_LOGI(TAG, "Servo_PROGRAM = %d", prog);
    gpio_set_level((gpio_num_t) LED_PIN, 1);

    switch (prog) {
    case 1:
        sv.programRun(Servo_Prg_10, Servo_Prg_10_Step);
        sv.programCenter();
        break;
    case 2:
        sv.programRun(Servo_Prg_11, Servo_Prg_11_Step);
        sv.programCenter();
        break;
    case 3:
        sv.programRun(Servo_Prg_12, Servo_Prg_12_Step);
        sv.programCenter();
        break;
    case 4:
        sv.programRun(Servo_Prg_13, Servo_Prg_13_Step);
        sv.programCenter();
        break;
    case 5:
        sv.programRun(Servo_Prg_14, Servo_Prg_14_Step);
        sv.programCenter();
        break;
    case 6:
        sv.programRun(Servo_Prg_15, Servo_Prg_15_Step);
        sv.programCenter();
        break;
    case 11:
        sv.programRun(Servo_Prg_20, Servo_Prg_20_Step);
        sv.programCenter();
        break;
    case 12:
        sv.programRun(Servo_Prg_21, Servo_Prg_21_Step);
        sv.programCenter();
        break;
    case 99:
        sv.programCenter();
        interruptibleDelay(300);
        break;
    case 100:
        sv.programZero();
        interruptibleDelay(300);
        break;
    default:
        break;
    }
}

void RoboHero::executePms(int prog)
{
    Servo &sv = Servo::instance();
    ESP_LOGI(TAG, "Servo_PROGRAM_Stack = %d", prog);
    gpio_set_level((gpio_num_t) LED_PIN, 1);

    switch (prog) {
    case 1:
        sv.programRun(Servo_Prg_1, Servo_Prg_1_Step);
        sv.programCenter();
        break;
    case 2:
        sv.programRun(Servo_Prg_2, Servo_Prg_2_Step);
        sv.programCenter();
        break;
    case 3:
        sv.programRun(Servo_Prg_3, Servo_Prg_3_Step);
        sv.programCenter();
        break;
    case 4:
        sv.programRun(Servo_Prg_4, Servo_Prg_4_Step);
        sv.programCenter();
        break;
    case 5:
        sv.programRun(Servo_Prg_5, Servo_Prg_5_Step);
        sv.programCenter();
        break;
    case 6:
        sv.programRun(Servo_Prg_6, Servo_Prg_6_Step);
        sv.programCenter();
        break;
    case 7:
        sv.programRun(Servo_Prg_7, Servo_Prg_7_Step);
        sv.programCenter();
        break;
    case 8:
        sv.programRun(Servo_Prg_8, Servo_Prg_8_Step);
        sv.programCenter();
        break;
    case 9:
        sv.programRun(Servo_Prg_9, Servo_Prg_9_Step);
        sv.programCenter();
        break;
    case 99:
        if (runThenCenter(Servo_Prg_1, Servo_Prg_1_Step) &&
            interruptibleDelay(1000) &&
            runThenCenter(Servo_Prg_2, Servo_Prg_2_Step) &&
            interruptibleDelay(1000) &&
            runThenCenter(Servo_Prg_6, Servo_Prg_6_Step) &&
            interruptibleDelay(1000) &&
            runThenCenter(Servo_Prg_3, Servo_Prg_3_Step) &&
            interruptibleDelay(1000) &&
            runThenCenter(Servo_Prg_4, Servo_Prg_4_Step) &&
            interruptibleDelay(1000) &&
            runThenCenter(Servo_Prg_5, Servo_Prg_5_Step) &&
            interruptibleDelay(1000)) {
            runThenCenter(Servo_Prg_1, Servo_Prg_1_Step);
        }
        break;
    default:
        break;
    }
}

void RoboHero::checkVoltage()
{
    uint16_t adc = 0;
    if (adc_read(&adc) != ESP_OK) {
        return;
    }

    int prev = _voltage;
    _voltage =
        (_voltage +
         mapInt((int) adc, 0, 1025, 0, Servo::instance().getVoltageValue())) /
        2;

    if (_engineering == 1) {
        if (_voltage > 601) {
            Servo::instance().writeGpio12(120);
            _voltageCab--;
            Store::instance().setVoltageTrim((int8_t) _voltageCab, true);
            Servo::instance().setVoltageValue(INPUT_VOLTAGE + _voltageCab);
        } else if (_voltage < 599) {
            Servo::instance().writeGpio12(60);
            _voltageCab++;
            Store::instance().setVoltageTrim((int8_t) _voltageCab, true);
            Servo::instance().setVoltageValue(INPUT_VOLTAGE + _voltageCab);
        } else {
            Servo::instance().writeGpio12(90);
        }
    } else {
        if (_voltage <= INPUT_MIN_VOLTAGE) {
            _voltageLow = 1;
        } else if (_voltage >= INPUT_RECOVER_VOLTAGE) {
            _voltageLow = 0;
        }
    }

    if (_voltageLow) {
        gpio_set_level((gpio_num_t) LED_PIN,
                       !gpio_get_level((gpio_num_t) LED_PIN));
    } else {
        gpio_set_level((gpio_num_t) LED_PIN, 1);
    }

    if (_voltage != prev) {
        sendVoltage(_voltage);
    }
}

void RoboHero::motionTask(void *arg)
{
    (void) arg;
    RoboHero &rh = instance();
    MotionCmd cmd;
    TickType_t lastVolt = xTaskGetTickCount();

    while (1) {
        if (xQueueReceive((QueueHandle_t) rh._cmdQueue,
                          &cmd,
                          pdMS_TO_TICKS(100)) == pdTRUE) {
            if (cmd.type == RH_CMD_STOP) {
                rh._cancel = true;
                continue;
            }
            if (rh._voltageLow &&
                (cmd.type == RH_CMD_PM || cmd.type == RH_CMD_PMS)) {
                ESP_LOGW(TAG, "low voltage: skip motion");
                continue;
            }

            rh._cancel = false;
            rh._busy = true;

            switch (cmd.type) {
            case RH_CMD_PM:
                rh.executePm(cmd.id);
                break;
            case RH_CMD_PMS:
                rh.executePms(cmd.id);
                break;
            case RH_CMD_CENTER:
                Servo::instance().programCenter();
                break;
            case RH_CMD_ZERO:
                Servo::instance().programZero();
                break;
            default:
                break;
            }

            rh._busy = false;
        }

        if ((xTaskGetTickCount() - lastVolt) >= pdMS_TO_TICKS(2000)) {
            rh.checkVoltage();
            lastVolt = xTaskGetTickCount();
        }
    }
}

void RoboHero::enqueueCmd(uint8_t type, int id)
{
    MotionCmd cmd;
    cmd.type = type;
    cmd.id = id;
    if (_busy && type != RH_CMD_STOP) {
        _cancel = true;
    }
    if (_cmdQueue) {
        xQueueOverwrite((QueueHandle_t) _cmdQueue, &cmd);
    }
}

bool RoboHero::isBusy() const
{
    return _busy;
}

bool RoboHero::isLowVoltage() const
{
    return _voltageLow != 0;
}

void RoboHero::resetLowVoltage()
{
    _voltageLow = 0;
}

int RoboHero::getVoltage() const
{
    return _voltage;
}

int RoboHero::engineeringModel() const
{
    return _engineering;
}

bool RoboHero::wifiReady() const
{
    return _wifiReady;
}

bool RoboHero::sendVoltage(int voltage)
{
    return Mqtt::instance().sendVoltage(voltage);
}

bool RoboHero::applyNetif()
{
    Store &st = Store::instance();
    esp_err_t err;

    if (st.isDhcpEnabled()) {
        err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
        if (err != ESP_OK &&
            err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED) {
            ESP_LOGE(TAG, "dhcpc_start: %s", esp_err_to_name(err));
            return false;
        }
        ESP_LOGI(TAG, "STA DHCP enabled");
        return true;
    }

    tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
    info.ip.addr = st.getStaticIp();
    info.netmask.addr = st.getStaticNetmask();
    info.gw.addr = st.getStaticGateway();
    if (info.ip.addr == 0) {
        ESP_LOGW(TAG, "static IP is 0.0.0.0; not applied");
        return false;
    }

    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    if (err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "dhcpc_stop: %s", esp_err_to_name(err));
        return false;
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_ip_info: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t dnsAddr = st.getStaticDns();
    if (dnsAddr != 0) {
        tcpip_adapter_dns_info_t dns;
        memset(&dns, 0, sizeof(dns));
        ip4_addr_set_u32(ip_2_ip4(&dns.ip), dnsAddr);
        err = tcpip_adapter_set_dns_info(
            TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set_dns_info: %s", esp_err_to_name(err));
            return false;
        }
    }

    ESP_LOGI(TAG, "STA static IP " IPSTR, IP2STR(&info.ip));
    return true;
}

bool RoboHero::submitPm(int id)
{
    if (_voltageLow) {
        return false;
    }
    enqueueCmd(RH_CMD_PM, id);
    return true;
}

bool RoboHero::submitPms(int id)
{
    if (_voltageLow) {
        return false;
    }
    enqueueCmd(RH_CMD_PMS, id);
    return true;
}

bool RoboHero::requestStop()
{
    bool busy = _busy;
    if (busy) {
        _cancel = true;
        enqueueCmd(RH_CMD_STOP, 0);
    }
    return busy;
}

void RoboHero::submitCenter()
{
    enqueueCmd(RH_CMD_CENTER, 0);
}

void RoboHero::submitZero()
{
    enqueueCmd(RH_CMD_ZERO, 0);
}

bool RoboHero::uartEscapePending()
{
    uint8_t c;
    bool escaped = false;
    while (uart_read_bytes(UART_NUM_0, &c, 1, 0) > 0) {
        if (c == '\r' || c == '\n' || c == 0x1b || c == 0x03) {
            escaped = true;
        }
    }
    return escaped;
}

void RoboHero::makeDefaultApSsid(char *out, size_t outlen)
{
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    snprintf(out, outlen, "TTR-%02x%02x", mac[4], mac[5]);
}

void RoboHero::startSoftAp(const char *ssid, const char *pass, uint8_t channel)
{
    char autoSsid[16];
    if (ssid == NULL || ssid[0] == '\0') {
        makeDefaultApSsid(autoSsid, sizeof(autoSsid));
        ssid = autoSsid;
    }

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy((char *) cfg.ap.ssid, ssid, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = (uint8_t) strlen(ssid);
    cfg.ap.channel = channel ? channel : 1;
    cfg.ap.max_connection = 4;
    if (pass && strlen(pass) >= 8) {
        strncpy((char *) cfg.ap.password, pass, sizeof(cfg.ap.password));
        cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &cfg));
    if (_wifiEvents) {
        xEventGroupClearBits((EventGroupHandle_t) _wifiEvents,
                             WIFI_AP_STARTED_BIT);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    if (_wifiEvents) {
        xEventGroupWaitBits((EventGroupHandle_t) _wifiEvents,
                            WIFI_AP_STARTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            pdMS_TO_TICKS(3000));
    }
    ESP_LOGI(TAG, "AP SSID: %s", ssid);
}

extern "C" void roboheroWifiEventHandler(void *arg,
                                         esp_event_base_t base,
                                         int32_t id,
                                         void *data)
{
    (void) arg;
    (void) data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        RoboHero::instance().handleWifiKind(0);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        RoboHero::instance().handleWifiKind(1);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        RoboHero::instance().handleWifiKind(2);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        RoboHero::instance().handleWifiKind(3);
    }
}

void RoboHero::handleWifiKind(int kind)
{
    if (kind == 0) {
        esp_wifi_connect();
        return;
    }
    if (kind == 1) {
        ESP_LOGW(TAG, "STA disconnected");
        return;
    }
    if (kind == 2) {
        if (_wifiEvents) {
            xEventGroupSetBits((EventGroupHandle_t) _wifiEvents,
                               WIFI_AP_STARTED_BIT);
        }
        notifyWifiUp();
        return;
    }
    if (kind == 3) {
        if (_wifiEvents) {
            xEventGroupSetBits((EventGroupHandle_t) _wifiEvents,
                               WIFI_CONNECTED_BIT);
        }
        notifyWifiUp();
    }
}

bool RoboHero::setupWifi()
{
    Store &st = Store::instance();

    if (uartEscapePending()) {
        printf("\nDetected escape key, the  normal booting process"
               " is bypassed...\n");
        return false;
    }

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    _wifiEvents = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &roboheroWifiEventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &roboheroWifiEventHandler, NULL));

    uint8_t mode = st.getWifiMode();
    const char *apSsid = st.getApSsid();
    const char *apPass = st.getApPassword();
    const char *staSsid = st.getStaSsid();
    const char *staPass = st.getStaPassword();
    uint8_t channel = st.getApChannel();

    (void) applyNetif();

    if (mode == ROBOHERO_WIFI_OFF) {
        ESP_LOGI(TAG, "Wi-Fi is disabled in NVS");
        return true;
    }

    if (mode == ROBOHERO_WIFI_AP) {
        startSoftAp(apSsid, apPass, channel);
        return true;
    }

    wifi_config_t staCfg;
    memset(&staCfg, 0, sizeof(staCfg));
    strncpy((char *) staCfg.sta.ssid,
            staSsid ? staSsid : "",
            sizeof(staCfg.sta.ssid));
    strncpy((char *) staCfg.sta.password,
            staPass ? staPass : "",
            sizeof(staCfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &staCfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to '%s'...", staSsid ? staSsid : "");

    EventBits_t bits = xEventGroupWaitBits((EventGroupHandle_t) _wifiEvents,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdTRUE,
                                           pdMS_TO_TICKS(5000));
    if (uartEscapePending()) {
        printf("\nDetected escape key, the  normal booting process"
               " is bypassed...\n");
        esp_wifi_disconnect();
        return false;
    }

    if (bits & WIFI_CONNECTED_BIT) {
        tcpip_adapter_ip_info_t ip;
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
        ESP_LOGI(TAG, "WiFi connected, IP " IPSTR, IP2STR(&ip.ip));
        return true;
    }

    ESP_LOGW(TAG, "STA timed out; enabling AP fallback");
    esp_wifi_stop();
    startSoftAp(apSsid, apPass, channel);
    return true;
}

void RoboHero::probeEngineering()
{
    gpio_config_t io;
    memset(&io, 0, sizeof(io));
    io.pin_bit_mask = (1ULL << GPIO12_PIN);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    gpio_set_level((gpio_num_t) GPIO12_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    io.mode = GPIO_MODE_INPUT;
    gpio_config(&io);
    vTaskDelay(pdMS_TO_TICKS(100));
    _engineering = gpio_get_level((gpio_num_t) GPIO12_PIN);
}

void RoboHero::terminalReset()
{
    static const char seq[] = TERM_RESET_SEQ;

    uart_tx_wait_idle(0);
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        uart_tx_one_char((uint8_t) seq[i]);
    }
}

/*
 * PHY/RTC clock init has already run when app_main starts. Switching
 * baud before that (constructor or CONFIG_CONSOLE_UART_BAUDRATE=115200)
 * leaves UART0 at the wrong rate after rtc_init_clk().
 */
void RoboHero::consoleBegin()
{
    uart_tx_wait_idle(0);
    uart_div_modify(0, UART_CLK_FREQ / 115200);
    terminalReset();
    uart_tx_wait_idle(0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void RoboHero::start()
{
    uart_config_t uartCfg;
    memset(&uartCfg, 0, sizeof(uartCfg));
    uartCfg.baud_rate = 115200;
    uartCfg.data_bits = UART_DATA_8_BITS;
    uartCfg.parity = UART_PARITY_DISABLE;
    uartCfg.stop_bits = UART_STOP_BITS_1;
    uartCfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(UART_NUM_0, &uartCfg);
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    Store::instance().init();

    gpio_config_t led;
    memset(&led, 0, sizeof(led));
    led.pin_bit_mask = (1ULL << LED_PIN);
    led.mode = GPIO_MODE_OUTPUT;
    led.pull_up_en = GPIO_PULLUP_DISABLE;
    led.pull_down_en = GPIO_PULLDOWN_DISABLE;
    led.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&led);
    gpio_set_level((gpio_num_t) LED_PIN, 0);

    probeEngineering();

    adc_config_t adcCfg;
    memset(&adcCfg, 0, sizeof(adcCfg));
    adcCfg.mode = ADC_READ_TOUT_MODE;
    adcCfg.clk_div = 8;
    if (adc_init(&adcCfg) != ESP_OK) {
        ESP_LOGW(TAG, "adc_init failed");
    }

    Servo::instance().init();
    Servo::instance().setYield(yieldCancel, NULL);

    _cmdQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(MotionCmd));
    xTaskCreate(motionTask, "motion", 3072, NULL, 5, NULL);

    if (_engineering == 1) {
        submitPm(100);
    }

    bool wifiOk = false;
    if (!uartEscapePending()) {
        wifiOk = setupWifi();
    } else {
        printf("\nDetected escape key, the  normal booting process"
               " is bypassed...\n");
    }

    _voltageLow = 0;
    _wifiReady = wifiOk && Store::instance().getWifiMode() != ROBOHERO_WIFI_OFF;
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
