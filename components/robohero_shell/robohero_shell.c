/*
 * robohero_shell.c
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "tcpip_adapter.h"

#include "robohero_app.h"
#include "robohero_config.h"
#include "robohero_servo.h"
#include "robohero_shell.h"
#include "robohero_store.h"
#include "robohero_web.h"
#include "version.h"

#define CMDLINE_SIZE 128

static const char *const g_commands[] = {
    "help", "version", "reboot", "eeprom", "wifi", "net", "pwm",
};

static void shell_printf(const char *fmt, ...)
{
    char pbuf[256];
    char out[320];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(pbuf, sizeof(pbuf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    if (n >= (int) sizeof(pbuf)) {
        n = (int) sizeof(pbuf) - 1;
    }

    int outlen = 0;
    for (int i = 0; i < n && outlen < (int) sizeof(out) - 2; i++) {
        if (pbuf[i] == '\n' && (i == 0 || pbuf[i - 1] != '\r')) {
            out[outlen++] = '\r';
        }
        out[outlen++] = pbuf[i];
    }
    uart_write_bytes(UART_NUM_0, out, outlen);
}

static void print_version_block(void)
{
    shell_printf("The RoboHero Firmware\n");
    shell_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    shell_printf("Built: %s@%s %s\n",
                 MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    shell_printf("-------------------------------------------\n");
    shell_printf("Copyright (C) 2026, Charles Chiou\n");
}

static void show_welcome(void)
{
    uart_write_bytes(UART_NUM_0, TERM_RESET_SEQ, sizeof(TERM_RESET_SEQ) - 1);
    shell_printf("\n");
    print_version_block();
    shell_printf("> ");
}

static void format_ip(uint32_t addr, char *out, size_t outlen)
{
    ip4_addr_t a;
    a.addr = addr;
    snprintf(out, outlen, IPSTR, IP2STR(&a));
}

static int cmd_help(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help]\n", argv[0]);
        shell_printf("  Display list of available commands.\n");
        return 0;
    }

    (void) argc;
    (void) argv;
    shell_printf("Available commands:\n");
    for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
        if ((i % 4) == 0) {
            shell_printf("  ");
        }
        shell_printf("%-12s", g_commands[i]);
        if ((i % 4) == 3) {
            shell_printf("\n");
        }
    }
    if ((sizeof(g_commands) / sizeof(g_commands[0]) % 4) != 0) {
        shell_printf("\n");
    }
    return 0;
}

static int cmd_version(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help]\n", argv[0]);
        shell_printf("  Display version and build information.\n");
        return 0;
    }

    (void) argc;
    (void) argv;
    print_version_block();
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help]\n", argv[0]);
        shell_printf("  Reboot the RoboHero controller board.\n");
        return 0;
    }

    (void) argc;
    (void) argv;
    shell_printf("Rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

static int cmd_eeprom(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shell_printf("  Manage EEPROM parameter storage and calibration values.\n");
        shell_printf("Commands:\n");
        shell_printf("  eeprom                         Display all EEPROM parameters and values\n");
        shell_printf("  eeprom get <key>               Read parameter at key index (0..19)\n");
        shell_printf("  eeprom set <key> <val>         Write parameter at key index (-125..125)\n");
        shell_printf("  eeprom load                    Reload all parameters from physical EEPROM\n");
        shell_printf("  eeprom save                    Save in-memory parameter cache to EEPROM\n");
        shell_printf("  eeprom reset [trims|all]       Reset servo trims or all parameters to default\n");
        shell_printf("  eeprom factory-reset           Reset all settings to compiled defaults\n");
        return 0;
    }

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "show") == 0)) {
        shell_printf("NVS Parameters (Valid: Yes):\n");
        shell_printf(" Key  Value  Name\n");
        for (int i = 0; i < STORE_PARAM_COUNT; i++) {
            shell_printf(" %3d  %5d  %s\n", i, (int) store_read_key(i), store_key_name(i));
        }
        shell_printf("\nWi-Fi & Network:\n");
        shell_printf("  Boot Wi-Fi Mode: %s\n", store_wifi_mode_name(store_get_wifi_mode()));
        shell_printf("  Station SSID:    %s\n", store_get_sta_ssid());
        shell_printf("  SoftAP SSID:     %s\n",
                     store_get_ap_ssid()[0] ? store_get_ap_ssid() : "(auto: TTR-xxxx)");
        shell_printf("  SoftAP Channel:  %u\n", (unsigned) store_get_ap_channel());
        shell_printf("  DHCP Enabled:    %s\n", store_is_dhcp_enabled() ? "Yes" : "No");
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "get") == 0) {
        int key = atoi(argv[2]);
        shell_printf("Key %d (%s) = %d\n", key, store_key_name(key), (int) store_read_key(key));
        return 0;
    }

    if (argc == 4 && strcmp(argv[1], "set") == 0) {
        int key = atoi(argv[2]);
        int val = atoi(argv[3]);
        if (val < -128 || val > 127) {
            shell_printf("Value out of range (-128..127)!\n");
            return -1;
        }
        store_write_key(key, (int8_t) val, true);
        if (key == STORE_KEY_PWM_FREQ) {
            servo_set_pwm_frequency(PWM_FREQUENCY + (int8_t) val);
        } else if (key == STORE_KEY_VOLTAGE_CAL) {
            servo_set_voltage_value(INPUT_VOLTAGE + (int8_t) val);
            robohero_app_reset_low_voltage();
        }
        shell_printf("Set key %d (%s) = %d [saved]\n", key, store_key_name(key), val);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "load") == 0) {
        shell_printf(store_load() ? "NVS reloaded.\n" : "Failed to reload NVS!\n");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "save") == 0) {
        shell_printf(store_save() ? "NVS committed.\n" : "Failed to save NVS!\n");
        return 0;
    }
    if ((argc >= 2 && strcmp(argv[1], "factory-reset") == 0) ||
        (argc >= 3 && strcmp(argv[1], "reset") == 0 && strcmp(argv[2], "all") == 0)) {
        store_factory_reset(true);
        shell_printf("Factory defaults saved.\n");
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "reset") == 0) {
        store_reset_motion_trims(true);
        shell_printf("Motion trims reset.\n");
        return 0;
    }

    shell_printf("syntax error! Type 'eeprom -h' for usage.\n");
    return -1;
}

static int cmd_wifi(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shell_printf("  Manage Wi-Fi connection and station/AP configuration.\n");
        shell_printf("Commands:\n");
        shell_printf("  wifi                           Show current Wi-Fi status and configured settings\n");
        shell_printf("  wifi scan                      Scan for available Wi-Fi networks\n");
        shell_printf("  wifi connect [<ssid> <pass>]   Connect to Wi-Fi (uses/saves to EEPROM)\n");
        shell_printf("  wifi ap [<ssid> <pass> [ch]]   Start Access Point (uses/saves to EEPROM)\n");
        shell_printf("  wifi set-mode <sta|ap|ap-sta|off>  Set boot Wi-Fi mode in EEPROM\n");
        shell_printf("  wifi set-sta <ssid> [pass]     Save Station credentials to EEPROM\n");
        shell_printf("  wifi set-ap <ssid> [pass] [ch] Save Access Point credentials to EEPROM\n");
        shell_printf("  wifi disconnect                Disconnect from Wi-Fi network\n");
        return 0;
    }

    if (argc == 1) {
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        const char *mode_str = "Unknown";
        if (mode == WIFI_MODE_AP) {
            mode_str = "AP Mode";
        } else if (mode == WIFI_MODE_STA) {
            mode_str = "Station (Client) Mode";
        } else if (mode == WIFI_MODE_APSTA) {
            mode_str = "AP + Station Mode";
        } else if (mode == WIFI_MODE_NULL) {
            mode_str = "OFF";
        }
        shell_printf("Active Wi-Fi Status:\n  Active Mode:   %s\n", mode_str);

        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip) == ESP_OK) {
                shell_printf("  STA IP:        " IPSTR "\n", IP2STR(&ip.ip));
            }
        }
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip) == ESP_OK) {
                shell_printf("  AP IP:         " IPSTR "\n", IP2STR(&ip.ip));
            }
        }
        shell_printf("\nNVS Wi-Fi Settings:\n");
        shell_printf("  Boot Mode:     %s\n", store_wifi_mode_name(store_get_wifi_mode()));
        shell_printf("  Stored STA:    %s\n", store_get_sta_ssid());
        shell_printf("  Stored AP:     %s (Ch: %u)\n",
                     store_get_ap_ssid()[0] ? store_get_ap_ssid() : "(auto: TTR-xxxx)",
                     (unsigned) store_get_ap_channel());
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "scan") == 0) {
        shell_printf("Scanning for Wi-Fi networks...\n");
        wifi_scan_config_t scan;
        memset(&scan, 0, sizeof(scan));
        if (esp_wifi_scan_start(&scan, true) != ESP_OK) {
            shell_printf("Scan failed.\n");
            return -1;
        }
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        if (n == 0) {
            shell_printf("No networks found.\n");
            return 0;
        }
        wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
        if (recs == NULL) {
            shell_printf("Out of memory.\n");
            return -1;
        }
        if (esp_wifi_scan_get_ap_records(&n, recs) == ESP_OK) {
            shell_printf("Found %u networks:\n", (unsigned) n);
            for (uint16_t i = 0; i < n; i++) {
                shell_printf("  %-32s  %4d  %4d dBm\n",
                             recs[i].ssid, recs[i].primary, recs[i].rssi);
            }
        }
        free(recs);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "connect") == 0) {
        const char *ssid = (argc >= 3) ? argv[2] : store_get_sta_ssid();
        const char *pass = (argc >= 4) ? argv[3] : store_get_sta_password();
        if (argc >= 3) {
            store_set_sta_ssid(ssid, false);
            store_set_sta_password(pass, false);
            store_set_wifi_mode(ROBOHERO_WIFI_STA, true);
        }
        wifi_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        strncpy((char *) cfg.sta.ssid, ssid ? ssid : "", sizeof(cfg.sta.ssid));
        strncpy((char *) cfg.sta.password, pass ? pass : "", sizeof(cfg.sta.password));
        shell_printf("Connecting to '%s'...\n", ssid ? ssid : "");
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg);
        esp_wifi_connect();
        web_start();
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "ap") == 0) {
        const char *ssid = (argc >= 3) ? argv[2] : store_get_ap_ssid();
        const char *pass = (argc >= 4) ? argv[3] : store_get_ap_password();
        int ch = (argc >= 5) ? atoi(argv[4]) : store_get_ap_channel();
        char auto_ssid[16];
        if (ssid == NULL || ssid[0] == '\0') {
            uint8_t mac[6];
            esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
            snprintf(auto_ssid, sizeof(auto_ssid), "TTR-%02x%02x", mac[4], mac[5]);
            ssid = auto_ssid;
        }
        if (argc >= 3) {
            store_set_ap_ssid(argv[2], false);
            store_set_ap_password(pass, false);
            store_set_ap_channel((uint8_t) ch, false);
            store_set_wifi_mode(ROBOHERO_WIFI_AP, true);
        }
        wifi_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        strncpy((char *) cfg.ap.ssid, ssid, sizeof(cfg.ap.ssid));
        cfg.ap.ssid_len = (uint8_t) strlen(ssid);
        cfg.ap.channel = (uint8_t) ch;
        cfg.ap.max_connection = 4;
        if (pass && strlen(pass) >= 8) {
            strncpy((char *) cfg.ap.password, pass, sizeof(cfg.ap.password));
            cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        } else {
            cfg.ap.authmode = WIFI_AUTH_OPEN;
        }
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_set_config(ESP_IF_WIFI_AP, &cfg);
        esp_wifi_start();
        web_start();
        shell_printf("Starting AP '%s' on channel %d...\n", ssid, ch);
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "set-mode") == 0) {
        uint8_t mode = ROBOHERO_WIFI_STA;
        if (strcmp(argv[2], "sta") == 0) {
            mode = ROBOHERO_WIFI_STA;
        } else if (strcmp(argv[2], "ap") == 0) {
            mode = ROBOHERO_WIFI_AP;
        } else if (strcmp(argv[2], "ap-sta") == 0) {
            mode = ROBOHERO_WIFI_AP_STA;
        } else if (strcmp(argv[2], "off") == 0) {
            mode = ROBOHERO_WIFI_OFF;
        } else {
            shell_printf("Invalid mode '%s'\n", argv[2]);
            return -1;
        }
        store_set_wifi_mode(mode, true);
        shell_printf("Boot Wi-Fi mode set to: %s [saved]\n", store_wifi_mode_name(mode));
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "set-sta") == 0) {
        store_set_sta_ssid(argv[2], false);
        store_set_sta_password(argc >= 4 ? argv[3] : "", true);
        shell_printf("Station credentials saved (SSID: '%s')\n", argv[2]);
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "set-ap") == 0) {
        int ch = (argc >= 5) ? atoi(argv[4]) : 1;
        store_set_ap_ssid(argv[2], false);
        store_set_ap_password(argc >= 4 ? argv[3] : "", false);
        store_set_ap_channel((uint8_t) ch, true);
        shell_printf("AP credentials saved (SSID: '%s', Ch: %d)\n", argv[2], ch);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "disconnect") == 0) {
        esp_wifi_disconnect();
        shell_printf("Wi-Fi disconnected.\n");
        return 0;
    }

    shell_printf("syntax error! Type 'wifi -h' for usage.\n");
    return -1;
}

static int cmd_net(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shell_printf("  Manage IP networking and network status.\n");
        shell_printf("Commands:\n");
        shell_printf("  net                            Show active IP configuration and MAC\n");
        shell_printf("  net dhcp <on|off>              Enable or disable DHCP in EEPROM\n");
        shell_printf("  net static <ip> <mask > <gw> [dns] Set static IP configuration in EEPROM\n");
        return 0;
    }

    if (argc == 1) {
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        shell_printf("Active Network Configuration:\n");
        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
            shell_printf("  STA IP:        " IPSTR "\n", IP2STR(&ip.ip));
            shell_printf("  STA Netmask:   " IPSTR "\n", IP2STR(&ip.netmask));
            shell_printf("  STA Gateway:   " IPSTR "\n", IP2STR(&ip.gw));
        }
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip);
            shell_printf("  AP IP:         " IPSTR "\n", IP2STR(&ip.ip));
        }
        shell_printf("\nNVS IP Configuration:\n");
        shell_printf("  DHCP Enabled:  %s\n", store_is_dhcp_enabled() ? "Yes" : "No");
        if (!store_is_dhcp_enabled()) {
            char a[16], b[16], c[16], d[16];
            format_ip(store_get_static_ip(), a, sizeof(a));
            format_ip(store_get_static_netmask(), b, sizeof(b));
            format_ip(store_get_static_gateway(), c, sizeof(c));
            format_ip(store_get_static_dns(), d, sizeof(d));
            shell_printf("  Static IP:     %s\n", a);
            shell_printf("  Static Netmask:%s\n", b);
            shell_printf("  Static Gateway:%s\n", c);
            shell_printf("  Static DNS:    %s\n", d);
        }
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "dhcp") == 0) {
        bool enable = (strcmp(argv[2], "on") == 0) || (strcmp(argv[2], "1") == 0);
        store_set_dhcp_enabled(enable, true);
        shell_printf("DHCP %s [saved]\n", enable ? "Enabled" : "Disabled");
        return 0;
    }

    if (argc >= 5 && strcmp(argv[1], "static") == 0) {
        ip4_addr_t ip, mask, gw, dns;
        if (!ip4addr_aton(argv[2], &ip) || !ip4addr_aton(argv[3], &mask) ||
            !ip4addr_aton(argv[4], &gw)) {
            shell_printf("Invalid IP address format!\n");
            return -1;
        }
        if (argc >= 6) {
            ip4addr_aton(argv[5], &dns);
        } else {
            dns = gw;
        }
        store_set_static_ip(ip.addr, false);
        store_set_static_netmask(mask.addr, false);
        store_set_static_gateway(gw.addr, false);
        store_set_static_dns(dns.addr, false);
        store_set_dhcp_enabled(false, true);
        shell_printf("Static IP configured [saved]\n");
        return 0;
    }

    shell_printf("syntax error! Type 'net -h' for usage.\n");
    return -1;
}

static int cmd_pwm(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shell_printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shell_printf("  Manage servo PWM calibration, trims, and positions.\n");
        shell_printf("Commands:\n");
        shell_printf("  pwm                            Show PWM frequency, voltage cal, and trims\n");
        shell_printf("  pwm freq [<hz>]                Get or set PWM driver frequency (Hz)\n");
        shell_printf("  pwm trim <servo_id> [<val>]    Get or set trim for servo 0..16 or delay 17\n");
        shell_printf("  pwm set <servo_id> <pos>       Set raw position (1..270) for servo 0..16\n");
        shell_printf("  pwm zero                       Move servos to zero alignment pose\n");
        shell_printf("  pwm center                     Move servos to standby center pose\n");
        shell_printf("  pwm run <prog_id>              Run motion program (1..6, 11..12, 99, 100)\n");
        shell_printf("  pwm stop                       Cancel the running motion program\n");
        return 0;
    }

    if (argc == 1) {
        shell_printf("PWM & Calibration Status:\n");
        shell_printf("  PWM Frequency:   %d Hz (base %d + offset %d)\n",
                     servo_get_pwm_frequency(), PWM_FREQUENCY,
                     (int) store_get_pwm_freq_trim());
        shell_printf("  Voltage Setting: %d (base %d + offset %d, current ADC volt %d)\n",
                     servo_get_voltage_value(), INPUT_VOLTAGE,
                     (int) store_get_voltage_trim(), robohero_app_get_voltage());
        shell_printf("  Delay Offset:    %d ms\n", (int) store_get_delay_trim());
        shell_printf("\nServo Trims and Running Positions:\n");
        for (int i = 0; i < ALLSERVOS; i++) {
            shell_printf("  %4d   %4d  %10d  %s\n", i,
                         (int) store_get_servo_trim(i),
                         servo_get_running_pos(i),
                         (i == 16) ? "GPIO 12" : "PCA9685");
        }
        return 0;
    }

    if (strcmp(argv[1], "freq") == 0) {
        if (argc == 2) {
            shell_printf("PWM Frequency: %d Hz\n", servo_get_pwm_frequency());
            return 0;
        }
        int hz = atoi(argv[2]);
        if (hz < 30 || hz > 200) {
            shell_printf("Frequency out of range (30..200)!\n");
            return -1;
        }
        int8_t trim = (int8_t) (hz - PWM_FREQUENCY);
        store_set_pwm_freq_trim(trim, true);
        servo_set_pwm_frequency(hz);
        shell_printf("PWM Frequency set to %d Hz [saved]\n", hz);
        return 0;
    }

    if (strcmp(argv[1], "trim") == 0) {
        if (argc < 3) {
            shell_printf("Usage: pwm trim <servo_id|all> [<offset>]\n");
            return -1;
        }
        if (strcmp(argv[2], "all") == 0) {
            for (int i = 0; i < ALLMATRIX; i++) {
                shell_printf("Trim %d (%s) = %d\n", i, store_key_name(i),
                             (int) store_get_matrix_trim(i));
            }
            return 0;
        }
        int id = atoi(argv[2]);
        if (argc == 3) {
            shell_printf("Servo %d trim = %d\n", id, (int) store_get_matrix_trim(id));
            return 0;
        }
        int val = atoi(argv[3]);
        store_set_matrix_trim(id, (int8_t) val, true);
        shell_printf("Servo %d trim set to %d [saved]\n", id, val);
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            shell_printf("Usage: pwm set <servo_id> <pos>\n");
            return -1;
        }
        int id = atoi(argv[2]);
        int pos = atoi(argv[3]);
        int effective = pos + (int) store_get_servo_trim(id);
        servo_set_pwm(id, effective);
        servo_set_running_pos(id, effective);
        shell_printf("Servo %d set to %d (effective %d)\n", id, pos, effective);
        return 0;
    }

    if (strcmp(argv[1], "zero") == 0) {
        if (robohero_app_is_busy()) {
            shell_printf("Busy; use pwm stop first.\n");
            return -1;
        }
        servo_program_zero();
        shell_printf("Zero pose completed.\n");
        return 0;
    }

    if (strcmp(argv[1], "center") == 0) {
        if (robohero_app_is_busy()) {
            shell_printf("Busy; use pwm stop first.\n");
            return -1;
        }
        servo_program_center();
        shell_printf("Center pose completed.\n");
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            shell_printf("Usage: pwm run <prog_id>\n");
            return -1;
        }
        if (robohero_app_is_low_voltage()) {
            shell_printf("Low voltage: motion program not queued\n");
            return -1;
        }
        int prog = atoi(argv[2]);
        robohero_app_submit_pm(prog);
        shell_printf("Triggered servo program %d\n", prog);
        return 0;
    }

    if (strcmp(argv[1], "stop") == 0) {
        if (robohero_app_request_stop()) {
            shell_printf("Cancel requested\n");
        } else {
            shell_printf("No motion program running\n");
        }
        return 0;
    }

    shell_printf("syntax error! Type 'pwm -h' for usage.\n");
    return -1;
}

static int exec_line(char *cmdline)
{
    int argc = 0;
    char *argv[16];

    while (*cmdline && argc < 16) {
        while (*cmdline && isspace((int) *cmdline)) {
            cmdline++;
        }
        if (*cmdline == '\0') {
            break;
        }
        argv[argc++] = cmdline;
        while (*cmdline && !isspace((int) *cmdline)) {
            cmdline++;
        }
        if (*cmdline) {
            *cmdline++ = '\0';
        }
    }
    if (argc < 1) {
        return 0;
    }

    if (strcmp(argv[0], "help") == 0) {
        return cmd_help(argc, argv);
    }
    if (strcmp(argv[0], "version") == 0) {
        return cmd_version(argc, argv);
    }
    if (strcmp(argv[0], "reboot") == 0) {
        return cmd_reboot(argc, argv);
    }
    if (strcmp(argv[0], "eeprom") == 0) {
        return cmd_eeprom(argc, argv);
    }
    if (strcmp(argv[0], "wifi") == 0) {
        return cmd_wifi(argc, argv);
    }
    if (strcmp(argv[0], "net") == 0) {
        return cmd_net(argc, argv);
    }
    if (strcmp(argv[0], "pwm") == 0) {
        return cmd_pwm(argc, argv);
    }

    shell_printf("Unknown command '%s'!\n", argv[0]);
    return -1;
}

static void shell_task(void *arg)
{
    (void) arg;
    char line[CMDLINE_SIZE];
    int i = 0;
    bool last_cr = false;

    show_welcome();

    while (1) {
        uint8_t c;
        int n = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;
        }

        if (c == '\n' && last_cr) {
            last_cr = false;
            continue;
        }
        last_cr = (c == '\r');

        if (c == '\r' || c == '\n') {
            uart_write_bytes(UART_NUM_0, "\r\n", 2);
            if (i > 0) {
                line[i] = '\0';
                exec_line(line);
                i = 0;
            }
            uart_write_bytes(UART_NUM_0, "> ", 2);
            continue;
        }

        if (c == 0x08 || c == 0x7f) {
            if (i > 0) {
                i--;
                uart_write_bytes(UART_NUM_0, "\b \b", 3);
            }
            continue;
        }

        if (c == 0x03) {
            i = 0;
            uart_write_bytes(UART_NUM_0, "^C\r\n> ", 6);
            continue;
        }

        if (isprint((int) c) && i < CMDLINE_SIZE - 1) {
            line[i++] = (char) c;
            uart_write_bytes(UART_NUM_0, (const char *) &c, 1);
        }
    }
}

void shell_start(void)
{
    xTaskCreate(shell_task, "shell", 3072, NULL, 4, NULL);
}
