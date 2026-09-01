/*
 * robohero_app.c
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
#include "rom/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

#include "robohero_app.h"
#include "robohero_config.h"
#include "robohero_motions.h"
#include "robohero_servo.h"
#include "robohero_store.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_AP_STARTED_BIT BIT1
#define CMD_QUEUE_LEN      1

typedef struct {
    uint8_t type;
    int id;
} motion_cmd_t;

static const char *TAG = "app";

static QueueHandle_t s_cmd_queue;
static volatile bool s_busy;
static volatile bool s_cancel;
static volatile int s_voltage_low;
static int s_voltage = INPUT_VOLTAGE;
static int s_engineering;
static int s_voltage_cab;
static EventGroupHandle_t s_wifi_events;
static bool s_wifi_ready;
static robohero_wifi_up_fn s_wifi_up_fn;

void robohero_app_on_wifi_up(robohero_wifi_up_fn fn)
{
    s_wifi_up_fn = fn;
}

static void notify_wifi_up(void)
{
    if (s_wifi_up_fn) {
        s_wifi_up_fn();
    }
}

static int map_int(int x, int in_min, int in_max, int out_min, int out_max)
{
    if (in_max == in_min) {
        return out_min;
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static bool yield_cancel(void *ctx)
{
    (void) ctx;
    return s_cancel;
}

static bool interruptible_delay(int ms)
{
    while (ms > 0) {
        int slice = (ms < BASEDELAYTIME) ? ms : BASEDELAYTIME;
        vTaskDelay(pdMS_TO_TICKS(slice));
        ms -= slice;
        if (s_cancel) {
            return false;
        }
    }
    return true;
}

static bool run_then_center(const int matrix[][ALLMATRIX], int steps)
{
    servo_program_run(matrix, steps);
    servo_program_center();
    return !s_cancel;
}

static void execute_pm(int prog)
{
    ESP_LOGI(TAG, "Servo_PROGRAM = %d", prog);
    gpio_set_level(LED_PIN, 1);

    switch (prog) {
    case 1:
        servo_program_run(Servo_Prg_10, Servo_Prg_10_Step);
        servo_program_center();
        break;
    case 2:
        servo_program_run(Servo_Prg_11, Servo_Prg_11_Step);
        servo_program_center();
        break;
    case 3:
        servo_program_run(Servo_Prg_12, Servo_Prg_12_Step);
        servo_program_center();
        break;
    case 4:
        servo_program_run(Servo_Prg_13, Servo_Prg_13_Step);
        servo_program_center();
        break;
    case 5:
        servo_program_run(Servo_Prg_14, Servo_Prg_14_Step);
        servo_program_center();
        break;
    case 6:
        servo_program_run(Servo_Prg_15, Servo_Prg_15_Step);
        servo_program_center();
        break;
    case 11:
        servo_program_run(Servo_Prg_20, Servo_Prg_20_Step);
        servo_program_center();
        break;
    case 12:
        servo_program_run(Servo_Prg_21, Servo_Prg_21_Step);
        servo_program_center();
        break;
    case 99:
        servo_program_center();
        interruptible_delay(300);
        break;
    case 100:
        servo_program_zero();
        interruptible_delay(300);
        break;
    default:
        break;
    }
}

static void execute_pms(int prog)
{
    ESP_LOGI(TAG, "Servo_PROGRAM_Stack = %d", prog);
    gpio_set_level(LED_PIN, 1);

    switch (prog) {
    case 1:
        servo_program_run(Servo_Prg_1, Servo_Prg_1_Step);
        servo_program_center();
        break;
    case 2:
        servo_program_run(Servo_Prg_2, Servo_Prg_2_Step);
        servo_program_center();
        break;
    case 3:
        servo_program_run(Servo_Prg_3, Servo_Prg_3_Step);
        servo_program_center();
        break;
    case 4:
        servo_program_run(Servo_Prg_4, Servo_Prg_4_Step);
        servo_program_center();
        break;
    case 5:
        servo_program_run(Servo_Prg_5, Servo_Prg_5_Step);
        servo_program_center();
        break;
    case 6:
        servo_program_run(Servo_Prg_6, Servo_Prg_6_Step);
        servo_program_center();
        break;
    case 7:
        servo_program_run(Servo_Prg_7, Servo_Prg_7_Step);
        servo_program_center();
        break;
    case 8:
        servo_program_run(Servo_Prg_8, Servo_Prg_8_Step);
        servo_program_center();
        break;
    case 9:
        servo_program_run(Servo_Prg_9, Servo_Prg_9_Step);
        servo_program_center();
        break;
    case 99:
        if (run_then_center(Servo_Prg_1, Servo_Prg_1_Step) &&
            interruptible_delay(1000) &&
            run_then_center(Servo_Prg_2, Servo_Prg_2_Step) &&
            interruptible_delay(1000) &&
            run_then_center(Servo_Prg_6, Servo_Prg_6_Step) &&
            interruptible_delay(1000) &&
            run_then_center(Servo_Prg_3, Servo_Prg_3_Step) &&
            interruptible_delay(1000) &&
            run_then_center(Servo_Prg_4, Servo_Prg_4_Step) &&
            interruptible_delay(1000) &&
            run_then_center(Servo_Prg_5, Servo_Prg_5_Step) &&
            interruptible_delay(1000)) {
            run_then_center(Servo_Prg_1, Servo_Prg_1_Step);
        }
        break;
    default:
        break;
    }
}

static void check_voltage(void)
{
    uint16_t adc = 0;
    if (adc_read(&adc) != ESP_OK) {
        return;
    }

    s_voltage = (s_voltage + map_int((int) adc, 0, 1025, 0,
                                     servo_get_voltage_value())) / 2;

    if (s_engineering == 1) {
        if (s_voltage > 601) {
            servo_write_gpio12(120);
            s_voltage_cab--;
            store_set_voltage_trim((int8_t) s_voltage_cab, true);
            servo_set_voltage_value(INPUT_VOLTAGE + s_voltage_cab);
        } else if (s_voltage < 599) {
            servo_write_gpio12(60);
            s_voltage_cab++;
            store_set_voltage_trim((int8_t) s_voltage_cab, true);
            servo_set_voltage_value(INPUT_VOLTAGE + s_voltage_cab);
        } else {
            servo_write_gpio12(90);
        }
    } else {
        if (s_voltage <= INPUT_MIN_VOLTAGE) {
            s_voltage_low = 1;
        } else if (s_voltage >= INPUT_RECOVER_VOLTAGE) {
            s_voltage_low = 0;
        }
    }

    if (s_voltage_low) {
        gpio_set_level(LED_PIN, !gpio_get_level(LED_PIN));
    } else {
        gpio_set_level(LED_PIN, 1);
    }
}

static void motion_task(void *arg)
{
    (void) arg;
    motion_cmd_t cmd;
    TickType_t last_volt = xTaskGetTickCount();

    while (1) {
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (cmd.type == RH_CMD_STOP) {
                s_cancel = true;
                continue;
            }
            if (s_voltage_low &&
                (cmd.type == RH_CMD_PM || cmd.type == RH_CMD_PMS)) {
                ESP_LOGW(TAG, "low voltage: skip motion");
                continue;
            }

            s_cancel = false;
            s_busy = true;

            switch (cmd.type) {
            case RH_CMD_PM:
                execute_pm(cmd.id);
                break;
            case RH_CMD_PMS:
                execute_pms(cmd.id);
                break;
            case RH_CMD_CENTER:
                servo_program_center();
                break;
            case RH_CMD_ZERO:
                servo_program_zero();
                break;
            default:
                break;
            }

            s_busy = false;
        }

        if ((xTaskGetTickCount() - last_volt) >= pdMS_TO_TICKS(2000)) {
            check_voltage();
            last_volt = xTaskGetTickCount();
        }
    }
}

static void enqueue_cmd(uint8_t type, int id)
{
    motion_cmd_t cmd = {
        .type = type,
        .id = id,
    };
    if (s_busy && type != RH_CMD_STOP) {
        s_cancel = true;
    }
    if (s_cmd_queue) {
        xQueueOverwrite(s_cmd_queue, &cmd);
    }
}

bool robohero_app_is_busy(void)
{
    return s_busy;
}

bool robohero_app_is_low_voltage(void)
{
    return s_voltage_low != 0;
}

void robohero_app_reset_low_voltage(void)
{
    s_voltage_low = 0;
}

int robohero_app_get_voltage(void)
{
    return s_voltage;
}

int robohero_app_engineering_model(void)
{
    return s_engineering;
}

bool robohero_app_wifi_ready(void)
{
    return s_wifi_ready;
}

bool robohero_app_apply_netif(void)
{
    esp_err_t err;

    if (store_is_dhcp_enabled()) {
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
    info.ip.addr = store_get_static_ip();
    info.netmask.addr = store_get_static_netmask();
    info.gw.addr = store_get_static_gateway();
    if (info.ip.addr == 0) {
        ESP_LOGW(TAG, "static IP is 0.0.0.0; not applied");
        return false;
    }

    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    if (err != ESP_OK &&
        err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "dhcpc_stop: %s", esp_err_to_name(err));
        return false;
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_ip_info: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t dns_addr = store_get_static_dns();
    if (dns_addr != 0) {
        tcpip_adapter_dns_info_t dns;
        memset(&dns, 0, sizeof(dns));
        ip4_addr_set_u32(ip_2_ip4(&dns.ip), dns_addr);
        err = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA,
                                         TCPIP_ADAPTER_DNS_MAIN, &dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set_dns_info: %s", esp_err_to_name(err));
            return false;
        }
    }

    ESP_LOGI(TAG, "STA static IP " IPSTR, IP2STR(&info.ip));
    return true;
}

bool robohero_app_submit_pm(int id)
{
    if (s_voltage_low) {
        return false;
    }
    enqueue_cmd(RH_CMD_PM, id);
    return true;
}

bool robohero_app_submit_pms(int id)
{
    if (s_voltage_low) {
        return false;
    }
    enqueue_cmd(RH_CMD_PMS, id);
    return true;
}

bool robohero_app_request_stop(void)
{
    bool busy = s_busy;
    if (busy) {
        s_cancel = true;
        enqueue_cmd(RH_CMD_STOP, 0);
    }
    return busy;
}

void robohero_app_submit_center(void)
{
    enqueue_cmd(RH_CMD_CENTER, 0);
}

void robohero_app_submit_zero(void)
{
    enqueue_cmd(RH_CMD_ZERO, 0);
}

static bool uart_escape_pending(void)
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

static void make_default_ap_ssid(char *out, size_t outlen)
{
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    snprintf(out, outlen, "TTR-%02x%02x", mac[4], mac[5]);
}

static void start_softap(const char *ssid, const char *pass, uint8_t channel)
{
    char auto_ssid[16];
    if (ssid == NULL || ssid[0] == '\0') {
        make_default_ap_ssid(auto_ssid, sizeof(auto_ssid));
        ssid = auto_ssid;
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
    if (s_wifi_events) {
        xEventGroupClearBits(s_wifi_events, WIFI_AP_STARTED_BIT);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    if (s_wifi_events) {
        xEventGroupWaitBits(s_wifi_events, WIFI_AP_STARTED_BIT,
                            pdFALSE, pdTRUE, pdMS_TO_TICKS(3000));
    }
    ESP_LOGI(TAG, "AP SSID: %s", ssid);
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void) arg;
    (void) data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        if (s_wifi_events) {
            xEventGroupSetBits(s_wifi_events, WIFI_AP_STARTED_BIT);
        }
        notify_wifi_up();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        if (s_wifi_events) {
            xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        }
        notify_wifi_up();
    }
}

static bool setup_wifi(void)
{
    if (uart_escape_pending()) {
        printf("\nDetected escape key, the  normal booting process is bypassed...\n");
        return false;
    }

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_events = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    uint8_t mode = store_get_wifi_mode();
    const char *ap_ssid = store_get_ap_ssid();
    const char *ap_pass = store_get_ap_password();
    const char *sta_ssid = store_get_sta_ssid();
    const char *sta_pass = store_get_sta_password();
    uint8_t channel = store_get_ap_channel();

    (void) robohero_app_apply_netif();

    if (mode == ROBOHERO_WIFI_OFF) {
        ESP_LOGI(TAG, "Wi-Fi is disabled in NVS");
        return true;
    }

    if (mode == ROBOHERO_WIFI_AP) {
        start_softap(ap_ssid, ap_pass, channel);
        return true;
    }

    wifi_config_t sta_cfg;
    memset(&sta_cfg, 0, sizeof(sta_cfg));
    strncpy((char *) sta_cfg.sta.ssid, sta_ssid ? sta_ssid : "",
            sizeof(sta_cfg.sta.ssid));
    strncpy((char *) sta_cfg.sta.password, sta_pass ? sta_pass : "",
            sizeof(sta_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to '%s'...", sta_ssid ? sta_ssid : "");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(5000));
    if (uart_escape_pending()) {
        printf("\nDetected escape key, the  normal booting process is bypassed...\n");
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
    start_softap(ap_ssid, ap_pass, channel);
    return true;
}

static void probe_engineering(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << GPIO12_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(GPIO12_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    io.mode = GPIO_MODE_INPUT;
    gpio_config(&io);
    vTaskDelay(pdMS_TO_TICKS(100));
    s_engineering = gpio_get_level(GPIO12_PIN);
}

void robohero_terminal_reset(void)
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
void robohero_console_begin(void)
{
    uart_tx_wait_idle(0);
    uart_div_modify(0, UART_CLK_FREQ / 115200);
    robohero_terminal_reset();
    uart_tx_wait_idle(0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void robohero_app_start(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_0, &uart_cfg);
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    store_init();

    gpio_config_t led = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led);
    gpio_set_level(LED_PIN, 0);

    probe_engineering();

    adc_config_t adc_cfg = {
        .mode = ADC_READ_TOUT_MODE,
        .clk_div = 8,
    };
    if (adc_init(&adc_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "adc_init failed");
    }

    servo_init();
    servo_set_yield(yield_cancel, NULL);

    s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(motion_cmd_t));
    xTaskCreate(motion_task, "motion", 3072, NULL, 5, NULL);

    if (s_engineering == 1) {
        robohero_app_submit_pm(100);
    }

    bool wifi_ok = false;
    if (!uart_escape_pending()) {
        wifi_ok = setup_wifi();
    } else {
        printf("\nDetected escape key, the  normal booting process is bypassed...\n");
    }

    s_voltage_low = 0;
    s_wifi_ready = wifi_ok && store_get_wifi_mode() != ROBOHERO_WIFI_OFF;
}
