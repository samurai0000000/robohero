/*
 * Shell.cxx
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
#include "lwip/ip_addr.h"
#include "tcpip_adapter.h"

#include "Config.hxx"
#include "Mqtt.hxx"
#include "RoboHero.hxx"
#include "Servo.hxx"
#include "Shell.hxx"
#include "Store.hxx"
#include "Web.hxx"
#include "version.h"

#define CMDLINE_SIZE 128

static const char *const g_commands[] = {
    "help", "version", "reboot", "eeprom", "wifi", "net", "pwm", "mqtt",
};

static void shellPrintf(const char *fmt, ...)
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

static void printVersionBlock(void)
{
    shellPrintf("The RoboHero Firmware\n");
    shellPrintf("Version: %s\n", MYPROJECT_VERSION_STRING);
    shellPrintf("Built: %s@%s %s\n",
                MYPROJECT_WHOAMI,
                MYPROJECT_HOSTNAME,
                MYPROJECT_DATE);
    shellPrintf("-------------------------------------------\n");
    shellPrintf("Copyright (C) 2026, Charles Chiou\n");
}

static void showWelcome(void)
{
    shellPrintf("\n");
    printVersionBlock();
    shellPrintf("> ");
}

static void formatIp(uint32_t addr, char *out, size_t outlen)
{
    ip4_addr_t a;
    a.addr = addr;
    snprintf(out, outlen, IPSTR, IP2STR(&a));
}

static bool parseIp4(const char *s, uint32_t *out)
{
    ip4_addr_t a;

    if (!s || !ip4addr_aton(s, &a)) {
        return false;
    }
    *out = a.addr;
    return true;
}

static void printNvmIp(void)
{
    Store &st = Store::instance();
    char a[16], b[16], c[16], d[16];

    formatIp(st.getStaticIp(), a, sizeof(a));
    formatIp(st.getStaticNetmask(), b, sizeof(b));
    formatIp(st.getStaticGateway(), c, sizeof(c));
    formatIp(st.getStaticDns(), d, sizeof(d));
    shellPrintf("  DHCP:          %s\n", st.isDhcpEnabled() ? "on" : "off");
    shellPrintf("  Static IP:     %s\n", a);
    shellPrintf("  Static Netmask:%s\n", b);
    shellPrintf("  Static Gateway:%s\n", c);
    shellPrintf("  Static DNS:    %s\n", d);
}

static int netTryApply(void)
{
    if (RoboHero::instance().applyNetif()) {
        shellPrintf("ok\n");
        return 0;
    }
    shellPrintf("failed\n");
    return -1;
}

static int netSavedApplyIfStatic(void)
{
    if (Store::instance().isDhcpEnabled()) {
        shellPrintf("ok [saved]\n");
        return 0;
    }
    return netTryApply();
}

static void mqttRestartIfRunning(void)
{
    if (Mqtt::instance().isRunning()) {
        Mqtt::instance().start();
    }
}

static int cmdHelp(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help]\n", argv[0]);
        shellPrintf("  Display list of available commands.\n");
        return 0;
    }

    (void) argc;
    (void) argv;
    shellPrintf("Available commands:\n");
    for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
        if ((i % 4) == 0) {
            shellPrintf("  ");
        }
        shellPrintf("%-12s", g_commands[i]);
        if ((i % 4) == 3) {
            shellPrintf("\n");
        }
    }
    if ((sizeof(g_commands) / sizeof(g_commands[0]) % 4) != 0) {
        shellPrintf("\n");
    }
    return 0;
}

static int cmdVersion(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help]\n", argv[0]);
        shellPrintf("  Display version and build information.\n");
        return 0;
    }

    (void) argc;
    (void) argv;
    printVersionBlock();
    return 0;
}

static int cmdReboot(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help]\n", argv[0]);
        shellPrintf("  Reboot the RoboHero controller board.\n");
        return 0;
    }

    (void) argc;
    (void) argv;
    shellPrintf("Rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

static int cmdEeprom(int argc, char **argv)
{
    Store &st = Store::instance();

    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shellPrintf("  Manage EEPROM parameter storage and "
                    "calibration values.\n");
        shellPrintf("Commands:\n");
        shellPrintf("  eeprom                         "
                    "Display all EEPROM parameters and values\n");
        shellPrintf("  eeprom get <key>               "
                    "Read parameter at key index (0..19)\n");
        shellPrintf("  eeprom set <key> <val>         "
                    "Write parameter at key index (-125..125)\n");
        shellPrintf("  eeprom load                    "
                    "Reload all parameters from physical EEPROM\n");
        shellPrintf("  eeprom save                    "
                    "Save in-memory parameter cache to EEPROM\n");
        shellPrintf("  eeprom reset [trims|all]       "
                    "Reset servo trims or all parameters to default\n");
        shellPrintf("  eeprom factory-reset           "
                    "Reset all settings to compiled defaults\n");
        return 0;
    }

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "show") == 0)) {
        shellPrintf("NVS Parameters (Valid: Yes):\n");
        shellPrintf(" Key  Value  Name\n");
        for (int i = 0; i < STORE_PARAM_COUNT; i++) {
            shellPrintf(
                " %3d  %5d  %s\n", i, (int) st.readKey(i), st.keyName(i));
        }
        shellPrintf("\nWi-Fi & Network:\n");
        shellPrintf("  Boot Wi-Fi Mode: %s\n",
                    st.wifiModeName(st.getWifiMode()));
        shellPrintf("  Station SSID:    %s\n", st.getStaSsid());
        shellPrintf("  SoftAP SSID:     %s\n",
                    st.getApSsid()[0] ? st.getApSsid() : "(auto: TTR-xxxx)");
        shellPrintf("  SoftAP Channel:  %u\n", (unsigned) st.getApChannel());
        shellPrintf("  DHCP Enabled:    %s\n",
                    st.isDhcpEnabled() ? "Yes" : "No");
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "get") == 0) {
        int key = atoi(argv[2]);
        shellPrintf(
            "Key %d (%s) = %d\n", key, st.keyName(key), (int) st.readKey(key));
        return 0;
    }

    if (argc == 4 && strcmp(argv[1], "set") == 0) {
        int key = atoi(argv[2]);
        int val = atoi(argv[3]);
        if (val < -128 || val > 127) {
            shellPrintf("Value out of range (-128..127)!\n");
            return -1;
        }
        st.writeKey(key, (int8_t) val, true);
        if (key == STORE_KEY_PWM_FREQ) {
            Servo::instance().setPwmFrequency(PWM_FREQUENCY + (int8_t) val);
        } else if (key == STORE_KEY_VOLTAGE_CAL) {
            Servo::instance().setVoltageValue(INPUT_VOLTAGE + (int8_t) val);
            RoboHero::instance().resetLowVoltage();
        }
        shellPrintf(
            "Set key %d (%s) = %d [saved]\n", key, st.keyName(key), val);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "load") == 0) {
        shellPrintf(st.load() ? "NVS reloaded.\n" : "Failed to reload NVS!\n");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "save") == 0) {
        shellPrintf(st.save() ? "NVS committed.\n" : "Failed to save NVS!\n");
        return 0;
    }
    if ((argc >= 2 && strcmp(argv[1], "factory-reset") == 0) ||
        (argc >= 3 && strcmp(argv[1], "reset") == 0 &&
         strcmp(argv[2], "all") == 0)) {
        st.factoryReset(true);
        shellPrintf("Factory defaults saved.\n");
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "reset") == 0) {
        st.resetMotionTrims(true);
        shellPrintf("Motion trims reset.\n");
        return 0;
    }

    shellPrintf("syntax error! Type 'eeprom -h' for usage.\n");
    return -1;
}

static int cmdWifi(int argc, char **argv)
{
    Store &st = Store::instance();

    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shellPrintf("  Manage Wi-Fi connection and station/AP "
                    "configuration.\n");
        shellPrintf("Commands:\n");
        shellPrintf("  wifi                           "
                    "Show current Wi-Fi status and configured settings\n");
        shellPrintf("  wifi scan                      "
                    "Scan for available Wi-Fi networks\n");
        shellPrintf("  wifi connect [<ssid> <pass>]   "
                    "Connect to Wi-Fi (uses/saves to EEPROM)\n");
        shellPrintf("  wifi ap [<ssid> <pass> [ch]]   "
                    "Start Access Point (uses/saves to EEPROM)\n");
        shellPrintf("  wifi set-mode <sta|ap|ap-sta|off>  "
                    "Set boot Wi-Fi mode in EEPROM\n");
        shellPrintf("  wifi set-sta <ssid> [pass]     "
                    "Save Station credentials to EEPROM\n");
        shellPrintf("  wifi set-ap <ssid> [pass] [ch] "
                    "Save Access Point credentials to EEPROM\n");
        shellPrintf("  wifi disconnect                "
                    "Disconnect from Wi-Fi network\n");
        return 0;
    }

    if (argc == 1) {
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        const char *modeStr = "Unknown";
        if (mode == WIFI_MODE_AP) {
            modeStr = "AP Mode";
        } else if (mode == WIFI_MODE_STA) {
            modeStr = "Station (Client) Mode";
        } else if (mode == WIFI_MODE_APSTA) {
            modeStr = "AP + Station Mode";
        } else if (mode == WIFI_MODE_NULL) {
            modeStr = "OFF";
        }
        shellPrintf("Active Wi-Fi Status:\n  Active Mode:   %s\n", modeStr);

        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip) ==
                ESP_OK) {
                shellPrintf("  STA IP:        " IPSTR "\n", IP2STR(&ip.ip));
            }
        }
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip) == ESP_OK) {
                shellPrintf("  AP IP:         " IPSTR "\n", IP2STR(&ip.ip));
            }
        }
        shellPrintf("\nNVS Wi-Fi Settings:\n");
        shellPrintf("  Boot Mode:     %s\n", st.wifiModeName(st.getWifiMode()));
        shellPrintf("  Stored STA:    %s\n", st.getStaSsid());
        shellPrintf("  Stored AP:     %s (Ch: %u)\n",
                    st.getApSsid()[0] ? st.getApSsid() : "(auto: TTR-xxxx)",
                    (unsigned) st.getApChannel());
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "scan") == 0) {
        shellPrintf("Scanning for Wi-Fi networks...\n");
        wifi_scan_config_t scan;
        memset(&scan, 0, sizeof(scan));
        if (esp_wifi_scan_start(&scan, true) != ESP_OK) {
            shellPrintf("Scan failed.\n");
            return -1;
        }
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        if (n == 0) {
            shellPrintf("No networks found.\n");
            return 0;
        }
        wifi_ap_record_t *recs = (wifi_ap_record_t *) calloc(n, sizeof(*recs));
        if (recs == NULL) {
            shellPrintf("Out of memory.\n");
            return -1;
        }
        if (esp_wifi_scan_get_ap_records(&n, recs) == ESP_OK) {
            shellPrintf("Found %u networks:\n", (unsigned) n);
            for (uint16_t i = 0; i < n; i++) {
                shellPrintf("  %-32s  %4d  %4d dBm\n",
                            recs[i].ssid,
                            recs[i].primary,
                            recs[i].rssi);
            }
        }
        free(recs);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "connect") == 0) {
        const char *ssid = (argc >= 3) ? argv[2] : st.getStaSsid();
        const char *pass = (argc >= 4) ? argv[3] : st.getStaPassword();
        if (argc >= 3) {
            st.setStaSsid(ssid, false);
            st.setStaPassword(pass, false);
            st.setWifiMode(ROBOHERO_WIFI_STA, true);
        }
        wifi_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        strncpy((char *) cfg.sta.ssid, ssid ? ssid : "", sizeof(cfg.sta.ssid));
        strncpy((char *) cfg.sta.password,
                pass ? pass : "",
                sizeof(cfg.sta.password));
        shellPrintf("Connecting to '%s'...\n", ssid ? ssid : "");
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg);
        esp_wifi_connect();
        Web::instance().start();
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "ap") == 0) {
        const char *ssid = (argc >= 3) ? argv[2] : st.getApSsid();
        const char *pass = (argc >= 4) ? argv[3] : st.getApPassword();
        int ch = (argc >= 5) ? atoi(argv[4]) : st.getApChannel();
        char autoSsid[16];
        if (ssid == NULL || ssid[0] == '\0') {
            uint8_t mac[6];
            esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
            snprintf(
                autoSsid, sizeof(autoSsid), "TTR-%02x%02x", mac[4], mac[5]);
            ssid = autoSsid;
        }
        if (argc >= 3) {
            st.setApSsid(argv[2], false);
            st.setApPassword(pass, false);
            st.setApChannel((uint8_t) ch, false);
            st.setWifiMode(ROBOHERO_WIFI_AP, true);
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
        Web::instance().start();
        shellPrintf("Starting AP '%s' on channel %d...\n", ssid, ch);
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
            shellPrintf("Invalid mode '%s'\n", argv[2]);
            return -1;
        }
        st.setWifiMode(mode, true);
        shellPrintf("Boot Wi-Fi mode set to: %s [saved]\n",
                    st.wifiModeName(mode));
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "set-sta") == 0) {
        st.setStaSsid(argv[2], false);
        st.setStaPassword(argc >= 4 ? argv[3] : "", true);
        shellPrintf("Station credentials saved (SSID: '%s')\n", argv[2]);
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "set-ap") == 0) {
        int ch = (argc >= 5) ? atoi(argv[4]) : 1;
        st.setApSsid(argv[2], false);
        st.setApPassword(argc >= 4 ? argv[3] : "", false);
        st.setApChannel((uint8_t) ch, true);
        shellPrintf("AP credentials saved (SSID: '%s', Ch: %d)\n", argv[2], ch);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "disconnect") == 0) {
        esp_wifi_disconnect();
        shellPrintf("Wi-Fi disconnected.\n");
        return 0;
    }

    shellPrintf("syntax error! Type 'wifi -h' for usage.\n");
    return -1;
}

static int cmdNet(int argc, char **argv)
{
    Store &st = Store::instance();

    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shellPrintf("  Manage IP networking and DNS (STA). "
                    "Settings save to EEPROM.\n");
        shellPrintf("Commands:\n");
        shellPrintf("  net                                    "
                    "Show live IP and DNS\n");
        shellPrintf("  net nvm                                "
                    "Show stored IP config\n");
        shellPrintf("  net apply                              "
                    "Apply stored config to STA\n");
        shellPrintf("  net dhcp <on|off>                      "
                    "Enable or disable DHCP\n");
        shellPrintf("  net ip dhcp                            "
                    "Enable DHCP (alias)\n");
        shellPrintf("  net ip <ip> netmask <mask> gw <gw>     "
                    "Set static IP, mask, gateway\n");
        shellPrintf("  net ip <ip>                            "
                    "Set stored static IP\n");
        shellPrintf("  net netmask <mask>                     "
                    "Set stored netmask\n");
        shellPrintf("  net gateway <gw>                       "
                    "Set stored gateway\n");
        shellPrintf("  net dns <ip>                           "
                    "Set stored DNS\n");
        shellPrintf("  net static <ip> <mask> <gw> [dns]      "
                    "Set static config (alias)\n");
        return 0;
    }

    if (argc == 1) {
        wifi_mode_t mode;
        tcpip_adapter_dns_info_t dns;

        esp_wifi_get_mode(&mode);
        shellPrintf("%s\n", st.isDhcpEnabled() ? "(dhcp)" : "(static ip)");
        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
            shellPrintf("ip:      " IPSTR "\n", IP2STR(&ip.ip));
            shellPrintf("netmask: " IPSTR "\n", IP2STR(&ip.netmask));
            shellPrintf("gateway: " IPSTR "\n", IP2STR(&ip.gw));
            if (tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_STA,
                                           TCPIP_ADAPTER_DNS_MAIN,
                                           &dns) == ESP_OK) {
                shellPrintf("dns:     " IPSTR "\n", IP2STR(ip_2_ip4(&dns.ip)));
            }
        }
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            tcpip_adapter_ip_info_t ip;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip);
            shellPrintf("ap ip:   " IPSTR "\n", IP2STR(&ip.ip));
        }
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "nvm") == 0) {
        printNvmIp();
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "apply") == 0) {
        return netTryApply();
    }

    if (argc >= 3 && strcmp(argv[1], "dhcp") == 0) {
        bool enable =
            (strcmp(argv[2], "on") == 0) || (strcmp(argv[2], "1") == 0);
        if (!enable && strcmp(argv[2], "off") != 0 &&
            strcmp(argv[2], "0") != 0) {
            shellPrintf("syntax error! Type 'net -h' for usage.\n");
            return -1;
        }
        st.setDhcpEnabled(enable, true);
        return netTryApply();
    }

    if (argc == 3 && strcmp(argv[1], "ip") == 0 &&
        strcmp(argv[2], "dhcp") == 0) {
        st.setDhcpEnabled(true, true);
        return netTryApply();
    }

    if (argc == 7 && strcmp(argv[1], "ip") == 0 &&
        strcmp(argv[3], "netmask") == 0 &&
        (strcmp(argv[5], "gw") == 0 || strcmp(argv[5], "gateway") == 0)) {
        uint32_t ip, mask, gw;
        if (!parseIp4(argv[2], &ip) || !parseIp4(argv[4], &mask) ||
            !parseIp4(argv[6], &gw)) {
            shellPrintf("Invalid IP address format!\n");
            return -1;
        }
        st.setStaticIp(ip, false);
        st.setStaticNetmask(mask, false);
        st.setStaticGateway(gw, false);
        st.setDhcpEnabled(false, true);
        return netTryApply();
    }

    if (argc == 3 && strcmp(argv[1], "ip") == 0) {
        uint32_t ip;
        if (!parseIp4(argv[2], &ip)) {
            shellPrintf("Invalid IP address format!\n");
            return -1;
        }
        st.setStaticIp(ip, true);
        return netSavedApplyIfStatic();
    }

    if (argc == 3 && strcmp(argv[1], "netmask") == 0) {
        uint32_t mask;
        if (!parseIp4(argv[2], &mask)) {
            shellPrintf("Invalid IP address format!\n");
            return -1;
        }
        st.setStaticNetmask(mask, true);
        return netSavedApplyIfStatic();
    }

    if (argc == 3 &&
        (strcmp(argv[1], "gateway") == 0 || strcmp(argv[1], "gw") == 0)) {
        uint32_t gw;
        if (!parseIp4(argv[2], &gw)) {
            shellPrintf("Invalid IP address format!\n");
            return -1;
        }
        st.setStaticGateway(gw, true);
        return netSavedApplyIfStatic();
    }

    if (argc == 3 && strcmp(argv[1], "dns") == 0) {
        uint32_t dns;
        if (!parseIp4(argv[2], &dns)) {
            shellPrintf("Invalid IP address format!\n");
            return -1;
        }
        st.setStaticDns(dns, true);
        return netSavedApplyIfStatic();
    }

    if (argc >= 5 && strcmp(argv[1], "static") == 0) {
        uint32_t ip, mask, gw, dns;
        if (!parseIp4(argv[2], &ip) || !parseIp4(argv[3], &mask) ||
            !parseIp4(argv[4], &gw)) {
            shellPrintf("Invalid IP address format!\n");
            return -1;
        }
        if (argc >= 6) {
            if (!parseIp4(argv[5], &dns)) {
                shellPrintf("Invalid IP address format!\n");
                return -1;
            }
        } else {
            dns = gw;
        }
        st.setStaticIp(ip, false);
        st.setStaticNetmask(mask, false);
        st.setStaticGateway(gw, false);
        st.setStaticDns(dns, false);
        st.setDhcpEnabled(false, true);
        return netTryApply();
    }

    shellPrintf("syntax error! Type 'net -h' for usage.\n");
    return -1;
}

static int cmdPwm(int argc, char **argv)
{
    Store &st = Store::instance();
    Servo &sv = Servo::instance();
    RoboHero &rh = RoboHero::instance();

    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shellPrintf("  Manage servo PWM calibration, trims, and "
                    "positions.\n");
        shellPrintf("Commands:\n");
        shellPrintf("  pwm                            "
                    "Show PWM frequency, voltage cal, and trims\n");
        shellPrintf("  pwm freq [<hz>]                "
                    "Get or set PWM driver frequency (Hz)\n");
        shellPrintf("  pwm trim <servo_id> [<val>]    "
                    "Get or set trim for servo 0..16 or delay 17\n");
        shellPrintf("  pwm set <servo_id> <pos>       "
                    "Set raw position (1..270) for servo 0..16\n");
        shellPrintf("  pwm zero                       "
                    "Move servos to zero alignment pose\n");
        shellPrintf("  pwm center                     "
                    "Move servos to standby center pose\n");
        shellPrintf("  pwm run <prog_id>              "
                    "Run motion program (1..6, 11..12, 99, 100)\n");
        shellPrintf("  pwm stop                       "
                    "Cancel the running motion program\n");
        return 0;
    }

    if (argc == 1) {
        shellPrintf("PWM & Calibration Status:\n");
        shellPrintf("  PWM Frequency:   %d Hz (base %d + offset %d)\n",
                    sv.getPwmFrequency(),
                    PWM_FREQUENCY,
                    (int) st.getPwmFreqTrim());
        shellPrintf("  Voltage Setting: %d (base %d + offset %d, "
                    "current ADC volt %d)\n",
                    sv.getVoltageValue(),
                    INPUT_VOLTAGE,
                    (int) st.getVoltageTrim(),
                    rh.getVoltage());
        shellPrintf("  Delay Offset:    %d ms\n", (int) st.getDelayTrim());
        shellPrintf("\nServo Trims and Running Positions:\n");
        for (int i = 0; i < ALLSERVOS; i++) {
            shellPrintf("  %4d   %4d  %10d  %s\n",
                        i,
                        (int) st.getServoTrim(i),
                        sv.getRunningPos(i),
                        (i == 16) ? "GPIO 12" : "PCA9685");
        }
        return 0;
    }

    if (strcmp(argv[1], "freq") == 0) {
        if (argc == 2) {
            shellPrintf("PWM Frequency: %d Hz\n", sv.getPwmFrequency());
            return 0;
        }
        int hz = atoi(argv[2]);
        if (hz < 30 || hz > 200) {
            shellPrintf("Frequency out of range (30..200)!\n");
            return -1;
        }
        int8_t trim = (int8_t) (hz - PWM_FREQUENCY);
        st.setPwmFreqTrim(trim, true);
        sv.setPwmFrequency(hz);
        shellPrintf("PWM Frequency set to %d Hz [saved]\n", hz);
        return 0;
    }

    if (strcmp(argv[1], "trim") == 0) {
        if (argc < 3) {
            shellPrintf("Usage: pwm trim <servo_id|all> [<offset>]\n");
            return -1;
        }
        if (strcmp(argv[2], "all") == 0) {
            for (int i = 0; i < ALLMATRIX; i++) {
                shellPrintf("Trim %d (%s) = %d\n",
                            i,
                            st.keyName(i),
                            (int) st.getMatrixTrim(i));
            }
            return 0;
        }
        int id = atoi(argv[2]);
        if (argc == 3) {
            shellPrintf("Servo %d trim = %d\n", id, (int) st.getMatrixTrim(id));
            return 0;
        }
        int val = atoi(argv[3]);
        st.setMatrixTrim(id, (int8_t) val, true);
        shellPrintf("Servo %d trim set to %d [saved]\n", id, val);
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            shellPrintf("Usage: pwm set <servo_id> <pos>\n");
            return -1;
        }
        int id = atoi(argv[2]);
        int pos = atoi(argv[3]);
        int effective = pos + (int) st.getServoTrim(id);
        sv.setPwm(id, effective);
        sv.setRunningPos(id, effective);
        shellPrintf("Servo %d set to %d (effective %d)\n", id, pos, effective);
        return 0;
    }

    if (strcmp(argv[1], "zero") == 0) {
        if (rh.isBusy()) {
            shellPrintf("Busy; use pwm stop first.\n");
            return -1;
        }
        sv.programZero();
        shellPrintf("Zero pose completed.\n");
        return 0;
    }

    if (strcmp(argv[1], "center") == 0) {
        if (rh.isBusy()) {
            shellPrintf("Busy; use pwm stop first.\n");
            return -1;
        }
        sv.programCenter();
        shellPrintf("Center pose completed.\n");
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            shellPrintf("Usage: pwm run <prog_id>\n");
            return -1;
        }
        if (rh.isLowVoltage()) {
            shellPrintf("Low voltage: motion program not queued\n");
            return -1;
        }
        int prog = atoi(argv[2]);
        rh.submitPm(prog);
        shellPrintf("Triggered servo program %d\n", prog);
        return 0;
    }

    if (strcmp(argv[1], "stop") == 0) {
        if (rh.requestStop()) {
            shellPrintf("Cancel requested\n");
        } else {
            shellPrintf("No motion program running\n");
        }
        return 0;
    }

    shellPrintf("syntax error! Type 'pwm -h' for usage.\n");
    return -1;
}

static int cmdMqtt(int argc, char **argv)
{
    Store &st = Store::instance();
    Mqtt &mq = Mqtt::instance();

    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        shellPrintf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        shellPrintf("  Manage MQTT TCP client (status/control topics).\n");
        shellPrintf("Commands:\n");
        shellPrintf("  mqtt                            "
                    "Show running/connected status, settings, TX/RX\n");
        shellPrintf("  mqtt on                         "
                    "Start MQTT client and save enable\n");
        shellPrintf("  mqtt off                        "
                    "Stop MQTT client and save disable\n");
        shellPrintf("  mqtt host <hostname>            "
                    "Save broker hostname\n");
        shellPrintf("  mqtt port <port>                "
                    "Save broker TCP port (default 1883)\n");
        shellPrintf("  mqtt user <username>            "
                    "Save username (empty string clears)\n");
        shellPrintf("  mqtt pass <password>            "
                    "Save password (not printed)\n");
        shellPrintf("  mqtt client-id <id>             "
                    "Save client id (empty uses TTR-xxxx)\n");
        return 0;
    }

    if (argc == 1) {
        const char *host = st.getMqttHost();
        const char *user = st.getMqttUser();
        const char *cid = st.getMqttClientId();
        shellPrintf("MQTT Status:\n");
        shellPrintf("  Enabled:        %s\n", st.mqttEnabled() ? "yes" : "no");
        shellPrintf("  Running:        %s\n", mq.isRunning() ? "yes" : "no");
        shellPrintf("  Connected:      %s\n", mq.isConnected() ? "yes" : "no");
        shellPrintf("  Host:           %s\n", host[0] ? host : "(empty)");
        shellPrintf("  Port:           %u\n", (unsigned) st.getMqttPort());
        shellPrintf("  User:           %s\n", user[0] ? user : "(empty)");
        shellPrintf("  Password:       %s\n",
                    st.getMqttPass()[0] ? "(set)" : "(empty)");
        shellPrintf("  Client ID:      %s\n",
                    cid[0] ? cid : "(auto: TTR-xxxx)");
        shellPrintf("  TX:             %u\n", mq.txCount());
        shellPrintf("  RX:             %u\n", mq.rxCount());
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "on") == 0) {
        st.setMqttEnabled(true, true);
        if (mq.start()) {
            shellPrintf("MQTT started\n");
            return 0;
        }
        shellPrintf("MQTT start failed\n");
        return -1;
    }

    if (argc == 2 && strcmp(argv[1], "off") == 0) {
        st.setMqttEnabled(false, true);
        mq.stop();
        shellPrintf("MQTT stopped\n");
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "host") == 0) {
        st.setMqttHost(argv[2], true);
        mqttRestartIfRunning();
        shellPrintf("MQTT host saved\n");
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "port") == 0) {
        int port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            shellPrintf("Invalid port\n");
            return -1;
        }
        st.setMqttPort((uint16_t) port, true);
        mqttRestartIfRunning();
        shellPrintf("MQTT port saved\n");
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "user") == 0) {
        st.setMqttUser(argv[2], true);
        mqttRestartIfRunning();
        shellPrintf("MQTT user saved\n");
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "pass") == 0) {
        st.setMqttPass(argv[2], true);
        mqttRestartIfRunning();
        shellPrintf("MQTT password saved\n");
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "client-id") == 0) {
        st.setMqttClientId(argv[2], true);
        mqttRestartIfRunning();
        shellPrintf("MQTT client-id saved\n");
        return 0;
    }

    shellPrintf("syntax error! Type 'mqtt -h' for usage.\n");
    return -1;
}

static int execLine(char *cmdline)
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
        return cmdHelp(argc, argv);
    }
    if (strcmp(argv[0], "version") == 0) {
        return cmdVersion(argc, argv);
    }
    if (strcmp(argv[0], "reboot") == 0) {
        return cmdReboot(argc, argv);
    }
    if (strcmp(argv[0], "eeprom") == 0) {
        return cmdEeprom(argc, argv);
    }
    if (strcmp(argv[0], "wifi") == 0) {
        return cmdWifi(argc, argv);
    }
    if (strcmp(argv[0], "net") == 0) {
        return cmdNet(argc, argv);
    }
    if (strcmp(argv[0], "pwm") == 0) {
        return cmdPwm(argc, argv);
    }
    if (strcmp(argv[0], "mqtt") == 0) {
        return cmdMqtt(argc, argv);
    }

    shellPrintf("Unknown command '%s'!\n", argv[0]);
    return -1;
}

void Shell::task(void *arg)
{
    (void) arg;
    char line[CMDLINE_SIZE];
    int i = 0;
    bool lastCr = false;

    showWelcome();

    while (1) {
        uint8_t c;
        int n = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;
        }

        if (c == '\n' && lastCr) {
            lastCr = false;
            continue;
        }
        lastCr = (c == '\r');

        if (c == '\r' || c == '\n') {
            uart_write_bytes(UART_NUM_0, "\r\n", 2);
            if (i > 0) {
                line[i] = '\0';
                execLine(line);
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

Shell Shell::_self;

Shell &Shell::instance()
{
    return _self;
}

Shell::Shell() {}

void Shell::start()
{
    xTaskCreate(task, "shell", 3072, NULL, 4, NULL);
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
