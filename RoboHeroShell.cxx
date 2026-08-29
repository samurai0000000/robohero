/*
 * RoboHeroShell.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "RoboHeroShell.hxx"
#include "RoboHeroApp.hxx"
#include "version.h"

static const char *const g_shell_commands[] = {
    "help",
    "version",
    "reboot",
    "eeprom",
    "wifi",
    "net",
    "pwm",
};

static const size_t g_shell_commands_count =
    sizeof(g_shell_commands) / sizeof(g_shell_commands[0]);

RoboHeroShell::RoboHeroShell(RoboHeroApp *app)
    : _app(app)
    , _banner("The RoboHero Firmware")
    , _version("Version: " MYPROJECT_VERSION_STRING)
    , _built("Built: " MYPROJECT_WHOAMI "@" MYPROJECT_HOSTNAME " " MYPROJECT_DATE)
    , _copyright("Copyright (C) 2026, Charles Chiou")
    , _noEcho(false)
    , _lastWasCr(false)
{
    _inproc.i = 0;
    _inproc.cmdline[0] = '\0';
}

RoboHeroShell::~RoboHeroShell()
{
}

void RoboHeroShell::setApp(RoboHeroApp *app)
{
    _app = app;
}

void RoboHeroShell::showWelcome(void)
{
    this->printf("\r\n\x0f\x1b(B\x1b)B\x1b[0m\x1b[2K\r");
    this->printf("%s\n", banner());
    this->printf("%s\n", version());
    this->printf("%s\n", built());
    this->printf("-------------------------------------------\n");
    this->printf("%s\n", copyright());
    this->printf("> ");
}

int RoboHeroShell::process(void)
{
    int ret = 0;
    uint8_t c;
    bool isCr = false;

    if (!this->rx_ready()) {
        return 0;
    }

    if (this->rx_read(&c, sizeof(c)) != sizeof(c)) {
        return 0;
    }

    if ((c == '\n') && _lastWasCr) {
        _lastWasCr = false;
        return 0;
    }

    _lastWasCr = false;

    if ((c == '\r') || (c == '\n')) {
        if (c == '\r') {
            _lastWasCr = true;
            isCr = true;
        }

        if (!_noEcho) {
            uint8_t crlf[2] = { '\r', '\n' };
            this->tx_write(crlf, 2);
        }

        if (_inproc.i > 0) {
            _inproc.cmdline[_inproc.i] = '\0';
            ret = this->exec(_inproc.cmdline);
            _inproc.i = 0;
            _inproc.cmdline[0] = '\0';
        }

        if (!_noEcho) {
            this->printf("> ");
        }

        return ret;
    }

    if ((c == 0x08) || (c == 0x7f)) {
        if (_inproc.i > 0) {
            _inproc.i--;
            _inproc.cmdline[_inproc.i] = '\0';
            if (!_noEcho) {
                uint8_t bs[3] = { 0x08, ' ', 0x08 };
                this->tx_write(bs, 3);
            }
        }
        return 0;
    }

    if (c == 0x03) {
        _inproc.i = 0;
        _inproc.cmdline[0] = '\0';
        if (!_noEcho) {
            this->printf("^C\n> ");
        }
        return 0;
    }

    if (isprint((int) c)) {
        if (_inproc.i < (CMDLINE_SIZE - 1)) {
            _inproc.cmdline[_inproc.i] = (char) c;
            _inproc.i++;
            _inproc.cmdline[_inproc.i] = '\0';
            if (!_noEcho) {
                this->tx_write(&c, sizeof(c));
            }
        } else {
            if (!_noEcho) {
                uint8_t bel = 0x07;
                this->tx_write(&bel, 1);
            }
        }
    }

    return 0;
}

int RoboHeroShell::tx_write(const uint8_t *buf, size_t size)
{
    if ((buf == NULL) || (size == 0)) {
        return 0;
    }

    return (int) Serial.write(buf, size);
}

int RoboHeroShell::printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = this->vprintf(format, ap);
    va_end(ap);

    return ret;
}

int RoboHeroShell::vprintf(const char *format, va_list ap)
{
    char pbuf[256];
    int n;

    n = vsnprintf(pbuf, sizeof(pbuf), format, ap);
    if (n <= 0) {
        return 0;
    }

    if ((size_t) n >= sizeof(pbuf)) {
        n = (int) sizeof(pbuf) - 1;
        pbuf[n] = '\0';
    }

    char outbuf[512];
    int outlen = 0;

    for (int i = 0; i < n; i++) {
        if (pbuf[i] == '\n') {
            if ((i == 0) || (pbuf[i - 1] != '\r')) {
                if (outlen < (int) sizeof(outbuf) - 2) {
                    outbuf[outlen++] = '\r';
                }
            }
        }
        if (outlen < (int) sizeof(outbuf) - 1) {
            outbuf[outlen++] = pbuf[i];
        }
    }
    outbuf[outlen] = '\0';

    return this->tx_write((const uint8_t *) outbuf, (size_t) outlen);
}

int RoboHeroShell::rx_ready(void) const
{
    return Serial.available() > 0 ? 1 : 0;
}

int RoboHeroShell::rx_read(uint8_t *buf, size_t size)
{
    int count = 0;

    if (buf == NULL) {
        return -1;
    }

    while ((size > 0) && (Serial.available() > 0)) {
        *buf = (uint8_t) Serial.read();
        buf++;
        size--;
        count++;
    }

    return count;
}

int RoboHeroShell::exec(char *cmdline)
{
    int ret = 0;
    int argc = 0;
    char *argv[32];

    if (cmdline == NULL) {
        ret = -1;
        goto done;
    }

    memset(argv, 0, sizeof(argv));

    while ((*cmdline != '\0') && (argc < 32)) {
        while ((*cmdline != '\0') && isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        argv[argc] = cmdline;
        argc++;

        while ((*cmdline != '\0') && !isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        *cmdline = '\0';
        cmdline++;
    }

    if (argc < 1) {
        ret = -1;
        goto done;
    }

    if (strcmp(argv[0], "help") == 0) {
        ret = this->help(argc, argv);
    } else if (strcmp(argv[0], "version") == 0) {
        ret = this->version(argc, argv);
    } else if (strcmp(argv[0], "reboot") == 0) {
        ret = this->reboot(argc, argv);
    } else if (strcmp(argv[0], "eeprom") == 0) {
        ret = this->eeprom(argc, argv);
    } else if (strcmp(argv[0], "wifi") == 0) {
        ret = this->wifi(argc, argv);
    } else if (strcmp(argv[0], "net") == 0) {
        ret = this->net(argc, argv);
    } else if (strcmp(argv[0], "pwm") == 0) {
        ret = this->pwm(argc, argv);
    } else {
        ret = this->unknown_command(argc, argv);
    }

done:

    return ret;
}

int RoboHeroShell::help(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help]\n", argv[0]);
        this->printf("  Display list of available commands.\n");
        return 0;
    }

    (void)(argc);
    (void)(argv);

    this->printf("Available commands:\n");

    for (size_t i = 0; i < g_shell_commands_count; i++) {
        if ((i % 4) == 0) {
            this->printf("  ");
        }

        this->printf("%-12s", g_shell_commands[i]);

        if ((i % 4) == 3) {
            this->printf("\n");
        }
    }

    if ((g_shell_commands_count % 4) != 0) {
        this->printf("\n");
    }

    return 0;
}

int RoboHeroShell::version(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help]\n", argv[0]);
        this->printf("  Display version and build information.\n");
        return 0;
    }

    (void)(argc);
    (void)(argv);

    this->printf("%s\n", banner());
    this->printf("%s\n", version());
    this->printf("%s\n", built());
    this->printf("-------------------------------------------\n");
    this->printf("%s\n", copyright());

    return 0;
}

int RoboHeroShell::reboot(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help]\n", argv[0]);
        this->printf("  Reboot the RoboHero controller board.\n");
        return 0;
    }

    (void)(argc);
    (void)(argv);

    this->printf("Rebooting...\n");
    delay(100);
    ESP.restart();
    return 0;
}

int RoboHeroShell::eeprom(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Manage EEPROM parameter storage and calibration values.\n");
        this->printf("Commands:\n");
        this->printf("  eeprom                         Display all EEPROM parameters and values\n");
        this->printf("  eeprom get <key>               Read parameter at key index (0..19)\n");
        this->printf("  eeprom set <key> <val>         Write parameter at key index (-125..125)\n");
        this->printf("  eeprom load                    Reload all parameters from physical EEPROM\n");
        this->printf("  eeprom save                    Save in-memory parameter cache to EEPROM\n");
        this->printf("  eeprom reset [trims|all]       Reset servo trims or all parameters to default\n");
        this->printf("  eeprom factory-reset           Reset all settings to compiled defaults\n");
        return 0;
    }

    if (_app == NULL) {
        this->printf("Application instance not available!\n");
        return -1;
    }

    RoboHeroEeprom &eep = _app->getEeprom();

    if ((argc == 1) || ((argc == 2) && (strcmp(argv[1], "show") == 0))) {
        this->printf("EEPROM Parameters (total %u bytes, Valid: %s):\n",
                     (unsigned int) eep.getSize(),
                     eep.isValid() ? "Yes" : "No");
        this->printf(" Key  Offset  Value  Name\n");
        this->printf(" ---  ------  -----  --------------------\n");
        for (int i = 0; i < (int) eep.getParamCount(); i++) {
            this->printf(" %3d  0x%04x  %5d  %s\n",
                         i, i, (int) eep.readKeyValue(i), RoboHeroEeprom::getKeyName(i));
        }

        this->printf("\nWi-Fi & Network Configuration in EEPROM:\n");
        this->printf("  Boot Wi-Fi Mode: %s\n", RoboHeroEeprom::getWifiModeName(eep.getWifiMode()));
        this->printf("  Station SSID:    %s\n", eep.getStaSSID());
        this->printf("  Station Pass:    %s\n", strlen(eep.getStaPassword()) > 0 ? "********" : "(none)");
        this->printf("  SoftAP SSID:     %s\n", strlen(eep.getApSSID()) > 0 ? eep.getApSSID() : "(auto: TTR-xxxx)");
        this->printf("  SoftAP Pass:     %s\n", strlen(eep.getApPassword()) > 0 ? "********" : "(none)");
        this->printf("  SoftAP Channel:  %u\n", (unsigned int) eep.getApChannel());
        this->printf("  DHCP Enabled:    %s\n", eep.isDhcpEnabled() ? "Yes" : "No");
        if (!eep.isDhcpEnabled()) {
            IPAddress ip(eep.getStaticIP());
            IPAddress mask(eep.getStaticNetmask());
            IPAddress gw(eep.getStaticGateway());
            IPAddress dns(eep.getStaticDNS());
            this->printf("  Static IP:       %s\n", ip.toString().c_str());
            this->printf("  Static Netmask:  %s\n", mask.toString().c_str());
            this->printf("  Static Gateway:  %s\n", gw.toString().c_str());
            this->printf("  Static DNS:      %s\n", dns.toString().c_str());
        }
        return 0;
    }

    if ((argc == 3) && (strcmp(argv[1], "get") == 0)) {
        int key = atoi(argv[2]);
        if ((key < 0) || (key >= (int) eep.getSize())) {
            this->printf("Invalid key %d (valid: 0..%u)!\n", key, (unsigned int) eep.getSize() - 1);
            return -1;
        }
        this->printf("Key %d (%s) = %d\n", key, RoboHeroEeprom::getKeyName(key), (int) eep.readKeyValue(key));
        return 0;
    }

    if ((argc == 4) && (strcmp(argv[1], "set") == 0)) {
        int key = atoi(argv[2]);
        int val = atoi(argv[3]);
        if ((key < 0) || (key >= (int) eep.getSize())) {
            this->printf("Invalid key %d (valid: 0..%u)!\n", key, (unsigned int) eep.getSize() - 1);
            return -1;
        }
        if ((val < -128) || (val > 127)) {
            this->printf("Value out of range (-128..127)!\n");
            return -1;
        }
        if (eep.writeKeyValue((int8_t) key, (int8_t) val, true)) {
            if (key == EEPROM_KEY_PWM_FREQ) {
                _app->getServo().setPWMFrequency(PWM_Frequency + (int8_t) val);
            } else if (key == EEPROM_KEY_VOLTAGE_CAL) {
                _app->getServo().setVoltageValue(Input_Voltage + (int8_t) val);
                _app->resetLowVoltage();
            }
            this->printf("Set key %d (%s) = %d [saved]\n", key, RoboHeroEeprom::getKeyName(key), val);
            return 0;
        } else {
            this->printf("Failed to write/save EEPROM key %d!\n", key);
            return -1;
        }
    }

    if ((argc == 2) && (strcmp(argv[1], "load") == 0)) {
        if (eep.load()) {
            this->printf("EEPROM reloaded successfully.\n");
            return 0;
        } else {
            this->printf("Failed to reload EEPROM!\n");
            return -1;
        }
    }

    if ((argc == 2) && (strcmp(argv[1], "save") == 0)) {
        if (eep.save()) {
            this->printf("EEPROM committed to flash successfully.\n");
            return 0;
        } else {
            this->printf("Failed to save EEPROM!\n");
            return -1;
        }
    }

    if (((argc >= 2) && (strcmp(argv[1], "factory-reset") == 0)) ||
        ((argc >= 3) && (strcmp(argv[1], "reset") == 0) && (strcmp(argv[2], "all") == 0))) {
        eep.factoryReset(true);
        this->printf("All EEPROM parameters reset to factory defaults [saved].\n");
        return 0;
    }

    if ((argc >= 2) && (strcmp(argv[1], "reset") == 0)) {
        eep.resetMotionTrims(true);
        this->printf("Motion and servo trims (0..17) reset to 0 [saved].\n");
        return 0;
    }

    this->printf("syntax error! Type 'eeprom -h' for usage.\n");
    return -1;
}

int RoboHeroShell::wifi(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Manage Wi-Fi connection and station/AP configuration.\n");
        this->printf("Commands:\n");
        this->printf("  wifi                           Show current Wi-Fi status and configured settings\n");
        this->printf("  wifi scan                      Scan for available Wi-Fi networks\n");
        this->printf("  wifi connect [<ssid> <pass>]   Connect to Wi-Fi (uses/saves to EEPROM)\n");
        this->printf("  wifi ap [<ssid> <pass> [ch]]   Start Access Point (uses/saves to EEPROM)\n");
        this->printf("  wifi set-mode <sta|ap|ap-sta|off>  Set boot Wi-Fi mode in EEPROM\n");
        this->printf("  wifi set-sta <ssid> [pass]     Save Station credentials to EEPROM\n");
        this->printf("  wifi set-ap <ssid> [pass] [ch] Save Access Point credentials to EEPROM\n");
        this->printf("  wifi disconnect                Disconnect from Wi-Fi network\n");
        return 0;
    }

    if (_app == NULL) {
        this->printf("Application instance not available!\n");
        return -1;
    }

    RoboHeroEeprom &eep = _app->getEeprom();

    if (argc == 1) {
        WiFiMode_t mode = WiFi.getMode();
        const char *modeStr = "Unknown";
        if (mode == WIFI_AP) {
            modeStr = "AP Mode";
        } else if (mode == WIFI_STA) {
            modeStr = "Station (Client) Mode";
        } else if (mode == WIFI_AP_STA) {
            modeStr = "AP + Station Mode";
        } else if (mode == WIFI_OFF) {
            modeStr = "OFF";
        }

        this->printf("Active Wi-Fi Status:\n");
        this->printf("  Active Mode:   %s\n", modeStr);

        if ((mode == WIFI_STA) || (mode == WIFI_AP_STA)) {
            wl_status_t status = WiFi.status();
            const char *statusStr = "Unknown";
            switch (status) {
            case WL_CONNECTED: statusStr = "Connected"; break;
            case WL_NO_SSID_AVAIL: statusStr = "SSID Not Found"; break;
            case WL_CONNECT_FAILED: statusStr = "Connection Failed"; break;
            case WL_CONNECTION_LOST: statusStr = "Connection Lost"; break;
            case WL_DISCONNECTED: statusStr = "Disconnected"; break;
            case WL_IDLE_STATUS: statusStr = "Idle"; break;
            default: break;
            }
            this->printf("  STA Status:    %s\n", statusStr);
            if (status == WL_CONNECTED) {
                uint8_t *bssid = WiFi.BSSID();
                this->printf("  STA SSID:      %s\n", WiFi.SSID().c_str());
                if (bssid != NULL) {
                    this->printf("  STA BSSID:     %02x:%02x:%02x:%02x:%02x:%02x\n",
                                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
                } else {
                    this->printf("  STA BSSID:     %s\n", WiFi.BSSIDstr().c_str());
                }
                this->printf("  STA Channel:   %d\n", WiFi.channel());
                this->printf("  STA RSSI:      %d dBm\n", WiFi.RSSI());
                this->printf("  STA IP:        %s\n", WiFi.localIP().toString().c_str());
            }
        }

        if ((mode == WIFI_AP) || (mode == WIFI_AP_STA)) {
            uint8_t mac[WL_MAC_ADDR_LENGTH];
            WiFi.softAPmacAddress(mac);
            this->printf("  AP IP:         %s\n", WiFi.softAPIP().toString().c_str());
            this->printf("  AP Clients:    %d\n", WiFi.softAPgetStationNum());
            this->printf("  AP MAC:        %02x:%02x:%02x:%02x:%02x:%02x\n",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        this->printf("\nEEPROM Wi-Fi Settings:\n");
        this->printf("  Boot Mode:     %s\n", RoboHeroEeprom::getWifiModeName(eep.getWifiMode()));
        this->printf("  Stored STA:    %s\n", eep.getStaSSID());
        this->printf("  Stored AP:     %s (Ch: %u)\n",
                     strlen(eep.getApSSID()) > 0 ? eep.getApSSID() : "(auto: TTR-xxxx)",
                     (unsigned int) eep.getApChannel());
        return 0;
    }

    if ((argc == 2) && (strcmp(argv[1], "scan") == 0)) {
        this->printf("Scanning for Wi-Fi networks...\n");
        int n = WiFi.scanNetworks();
        if (n == 0) {
            this->printf("No networks found.\n");
        } else {
            this->printf("Found %d networks:\n", n);
            this->printf("  %-32s  %-4s  %-7s  %s\n", "SSID", "Ch", "RSSI", "Enc");
            this->printf("  %-32s  %-4s  %-7s  ----\n", "--------------------------------", "----", "-------");
            for (int i = 0; i < n; i++) {
                const char *encStr = "Open";
                switch (WiFi.encryptionType(i)) {
                case ENC_TYPE_WEP: encStr = "WEP"; break;
                case ENC_TYPE_TKIP: encStr = "WPA/TKIP"; break;
                case ENC_TYPE_CCMP: encStr = "WPA2/CCMP"; break;
                case ENC_TYPE_AUTO: encStr = "Auto"; break;
                case ENC_TYPE_NONE: encStr = "Open"; break;
                default: break;
                }
                this->printf("  %-32s  %4d  %4d dBm  %s\n",
                             WiFi.SSID(i).c_str(),
                             WiFi.channel(i),
                             WiFi.RSSI(i),
                             encStr);
            }
        }
        return 0;
    }

    if ((argc >= 2) && (strcmp(argv[1], "connect") == 0)) {
        const char *ssid = (argc >= 3) ? argv[2] : eep.getStaSSID();
        const char *pass = (argc >= 4) ? argv[3] : eep.getStaPassword();

        if (argc >= 3) {
            eep.setStaSSID(ssid, false);
            eep.setStaPassword(pass, false);
            eep.setWifiMode(ROBOHERO_WIFI_STA, true);
        }

        this->printf("Connecting to '%s'...\n", ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, pass);
        int timeout = 20;
        while ((WiFi.status() != WL_CONNECTED) && (timeout > 0)) {
            delay(500);
            this->printf(".");
            timeout--;
        }
        if (WiFi.status() == WL_CONNECTED) {
            this->printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            this->printf("\nConnection failed (status %d)!\n", (int) WiFi.status());
        }
        return 0;
    }

    if ((argc >= 2) && (strcmp(argv[1], "ap") == 0)) {
        const char *ssid = (argc >= 3) ? argv[2] : eep.getApSSID();
        const char *pass = (argc >= 4) ? argv[3] : eep.getApPassword();
        int ch = (argc >= 5) ? atoi(argv[4]) : (int) eep.getApChannel();

        String apSsidStr;
        if ((ssid == NULL) || (strlen(ssid) == 0)) {
            uint8_t mac[WL_MAC_ADDR_LENGTH];
            WiFi.softAPmacAddress(mac);
            char macIDBuf[8];
            snprintf(macIDBuf, sizeof(macIDBuf), "%02x%02x",
                     mac[WL_MAC_ADDR_LENGTH - 2],
                     mac[WL_MAC_ADDR_LENGTH - 1]);
            apSsidStr = "TTR-" + String(macIDBuf);
            ssid = apSsidStr.c_str();
        }

        if (argc >= 3) {
            eep.setApSSID(argv[2], false);
            eep.setApPassword(pass, false);
            eep.setApChannel((uint8_t) ch, false);
            eep.setWifiMode(ROBOHERO_WIFI_AP, true);
        }

        this->printf("Starting AP '%s' on channel %d...\n", ssid, ch);
        WiFi.mode(WIFI_AP);
        if (strlen(pass) > 0) {
            WiFi.softAP(ssid, pass, ch);
        } else {
            WiFi.softAP(ssid);
        }
        this->printf("AP active! IP: %s\n", WiFi.softAPIP().toString().c_str());
        return 0;
    }

    if ((argc >= 3) && (strcmp(argv[1], "set-mode") == 0)) {
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
            this->printf("Invalid mode '%s' (valid: sta, ap, ap-sta, off)!\n", argv[2]);
            return -1;
        }
        eep.setWifiMode(mode, true);
        this->printf("Boot Wi-Fi mode set to: %s [saved]\n", RoboHeroEeprom::getWifiModeName(mode));
        return 0;
    }

    if ((argc >= 3) && (strcmp(argv[1], "set-sta") == 0)) {
        const char *ssid = argv[2];
        const char *pass = (argc >= 4) ? argv[3] : "";
        eep.setStaSSID(ssid, false);
        eep.setStaPassword(pass, true);
        this->printf("Station credentials saved to EEPROM (SSID: '%s') [saved]\n", ssid);
        return 0;
    }

    if ((argc >= 3) && (strcmp(argv[1], "set-ap") == 0)) {
        const char *ssid = argv[2];
        const char *pass = (argc >= 4) ? argv[3] : "";
        int ch = (argc >= 5) ? atoi(argv[4]) : 1;
        eep.setApSSID(ssid, false);
        eep.setApPassword(pass, false);
        eep.setApChannel((uint8_t) ch, true);
        this->printf("Access Point credentials saved to EEPROM (SSID: '%s', Ch: %d) [saved]\n", ssid, ch);
        return 0;
    }

    if ((argc == 2) && (strcmp(argv[1], "disconnect") == 0)) {
        WiFi.disconnect();
        this->printf("Wi-Fi disconnected.\n");
        return 0;
    }

    this->printf("syntax error! Type 'wifi -h' for usage.\n");
    return -1;
}

int RoboHeroShell::net(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Manage IP networking and network status.\n");
        this->printf("Commands:\n");
        this->printf("  net                            Show active IP configuration and MAC\n");
        this->printf("  net dhcp <on|off>              Enable or disable DHCP in EEPROM\n");
        this->printf("  net static <ip> <mask > <gw> [dns] Set static IP configuration in EEPROM\n");
        return 0;
    }

    if (_app == NULL) {
        this->printf("Application instance not available!\n");
        return -1;
    }

    RoboHeroEeprom &eep = _app->getEeprom();

    if (argc == 1) {
        WiFiMode_t mode = WiFi.getMode();
        this->printf("Active Network Configuration:\n");
        this->printf("  Hostname:      %s\n", WiFi.hostname().c_str());

        if ((mode == WIFI_STA) || (mode == WIFI_AP_STA)) {
            uint8_t mac[WL_MAC_ADDR_LENGTH];
            WiFi.macAddress(mac);
            this->printf("  STA IP:        %s\n", WiFi.localIP().toString().c_str());
            this->printf("  STA Netmask:   %s\n", WiFi.subnetMask().toString().c_str());
            this->printf("  STA Gateway:   %s\n", WiFi.gatewayIP().toString().c_str());
            this->printf("  STA DNS:       %s\n", WiFi.dnsIP().toString().c_str());
            this->printf("  STA MAC:       %02x:%02x:%02x:%02x:%02x:%02x\n",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        if ((mode == WIFI_AP) || (mode == WIFI_AP_STA)) {
            uint8_t mac[WL_MAC_ADDR_LENGTH];
            WiFi.softAPmacAddress(mac);
            this->printf("  AP IP:         %s\n", WiFi.softAPIP().toString().c_str());
            this->printf("  AP MAC:        %02x:%02x:%02x:%02x:%02x:%02x\n",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        this->printf("\nEEPROM IP Configuration:\n");
        this->printf("  DHCP Enabled:  %s\n", eep.isDhcpEnabled() ? "Yes" : "No");
        if (!eep.isDhcpEnabled()) {
            IPAddress ip(eep.getStaticIP());
            IPAddress mask(eep.getStaticNetmask());
            IPAddress gw(eep.getStaticGateway());
            IPAddress dns(eep.getStaticDNS());
            this->printf("  Static IP:     %s\n", ip.toString().c_str());
            this->printf("  Static Netmask:%s\n", mask.toString().c_str());
            this->printf("  Static Gateway:%s\n", gw.toString().c_str());
            this->printf("  Static DNS:    %s\n", dns.toString().c_str());
        }

        return 0;
    }

    if ((argc >= 3) && (strcmp(argv[1], "dhcp") == 0)) {
        bool enable = (strcmp(argv[2], "on") == 0) || (strcmp(argv[2], "1") == 0) || (strcmp(argv[2], "enable") == 0);
        eep.setDhcpEnabled(enable, true);
        this->printf("DHCP %s [saved]\n", enable ? "Enabled" : "Disabled");
        return 0;
    }

    if ((argc >= 5) && (strcmp(argv[1], "static") == 0)) {
        IPAddress ip, mask, gw, dns;
        if (!ip.fromString(argv[2]) || !mask.fromString(argv[3]) || !gw.fromString(argv[4])) {
            this->printf("Invalid IP address format! Format: <ip> <netmask> <gateway> [dns]\n");
            return -1;
        }
        if (argc >= 6) {
            dns.fromString(argv[5]);
        } else {
            dns = gw;
        }

        eep.setStaticIP((uint32_t) ip, false);
        eep.setStaticNetmask((uint32_t) mask, false);
        eep.setStaticGateway((uint32_t) gw, false);
        eep.setStaticDNS((uint32_t) dns, false);
        eep.setDhcpEnabled(false, true);

        this->printf("Static IP configured: IP=%s Mask=%s GW=%s DNS=%s [saved]\n",
                     ip.toString().c_str(), mask.toString().c_str(),
                     gw.toString().c_str(), dns.toString().c_str());
        return 0;
    }

    this->printf("syntax error! Type 'net -h' for usage.\n");
    return -1;
}

int RoboHeroShell::pwm(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Manage servo PWM calibration, trims, and positions.\n");
        this->printf("Commands:\n");
        this->printf("  pwm                            Show PWM frequency, voltage cal, and trims\n");
        this->printf("  pwm freq [<hz>]                Get or set PWM driver frequency (Hz)\n");
        this->printf("  pwm trim <servo_id> [<val>]    Get or set trim for servo 0..16 or delay 17\n");
        this->printf("  pwm set <servo_id> <pos>       Set raw position (1..270) for servo 0..16\n");
        this->printf("  pwm zero                       Move servos to zero alignment pose\n");
        this->printf("  pwm center                     Move servos to standby center pose\n");
        this->printf("  pwm run <prog_id>              Run motion program (1..6, 11..12, 99, 100)\n");
        return 0;
    }

    if (_app == NULL) {
        this->printf("Application instance not available!\n");
        return -1;
    }

    RoboHeroServo &servo = _app->getServo();
    RoboHeroEeprom &eep = _app->getEeprom();

    if (argc == 1) {
        this->printf("PWM & Calibration Status:\n");
        this->printf("  PWM Frequency:   %d Hz (base %d + offset %d)\n",
                     servo.getPWMFrequencySetting(),
                     PWM_Frequency,
                     (int) eep.getPwmFreqTrim());
        this->printf("  Voltage Setting: %d (base %d + offset %d, current ADC volt %d)\n",
                     servo.getVoltageValueSetting(),
                     Input_Voltage,
                     (int) eep.getVoltageTrim(),
                     _app->getVoltage());
        this->printf("  Delay Offset:    %d ms\n", (int) eep.getDelayTrim());
        this->printf("\nServo Trims and Running Positions:\n");
        this->printf("  Servo  Trim  RunningPos  Type\n");
        this->printf("  -----  ----  ----------  ----------\n");
        for (int i = 0; i < ALLSERVOS; i++) {
            const char *typeStr = (i == 16) ? "GPIO 12" : "PCA9685";
            this->printf("  %4d   %4d  %10d  %s\n",
                         i,
                         (int) eep.getServoTrim(i),
                         servo.getRunningServoPos(i),
                         typeStr);
        }
        return 0;
    }

    if (strcmp(argv[1], "freq") == 0) {
        if (argc == 2) {
            this->printf("PWM Frequency: %d Hz (trim offset %d)\n",
                         servo.getPWMFrequencySetting(),
                         (int) eep.getPwmFreqTrim());
            return 0;
        }
        int hz = atoi(argv[2]);
        if ((hz < 30) || (hz > 200)) {
            this->printf("Frequency %d Hz out of valid range (30..200)!\n", hz);
            return -1;
        }
        int8_t trim = (int8_t) (hz - PWM_Frequency);
        eep.setPwmFreqTrim(trim, true);
        servo.setPWMFrequency(hz);
        this->printf("PWM Frequency set to %d Hz (offset %d) [saved]\n", hz, (int) trim);
        return 0;
    }

    if (strcmp(argv[1], "trim") == 0) {
        if (argc < 3) {
            this->printf("Usage: pwm trim <servo_id|all> [<offset>]\n");
            return -1;
        }
        if (strcmp(argv[2], "all") == 0) {
            for (int i = 0; i < ALLMATRIX; i++) {
                this->printf("Trim %d (%s) = %d\n", i, RoboHeroEeprom::getKeyName(i), (int) eep.getMatrixTrim(i));
            }
            return 0;
        }
        int id = atoi(argv[2]);
        if ((id < 0) || (id >= ALLMATRIX)) {
            this->printf("Invalid matrix/servo ID %d (valid: 0..%d)!\n", id, ALLMATRIX - 1);
            return -1;
        }
        if (argc == 3) {
            this->printf("Servo %d (%s) trim = %d\n", id, RoboHeroEeprom::getKeyName(id), (int) eep.getMatrixTrim(id));
            return 0;
        }
        int val = atoi(argv[3]);
        if ((val < -125) || (val > 125)) {
            this->printf("Trim value %d out of valid range (-125..125)!\n", val);
            return -1;
        }
        eep.setMatrixTrim(id, (int8_t) val, true);
        this->printf("Servo %d (%s) trim set to %d [saved]\n", id, RoboHeroEeprom::getKeyName(id), val);
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            this->printf("Usage: pwm set <servo_id> <pos>\n");
            return -1;
        }
        int id = atoi(argv[2]);
        int pos = atoi(argv[3]);
        if ((id < 0) || (id >= ALLSERVOS)) {
            this->printf("Invalid servo ID %d (valid: 0..%d)!\n", id, ALLSERVOS - 1);
            return -1;
        }
        if ((pos < PWMRES_Min) || (pos > PWMRES_Max)) {
            this->printf("Position %d out of valid range (%d..%d)!\n", pos, PWMRES_Min, PWMRES_Max);
            return -1;
        }
        int effectivePos = pos + (int) eep.getServoTrim(id);
        servo.setPWMtoServo(id, effectivePos);
        servo.setRunningServoPos(id, effectivePos);
        this->printf("Servo %d set to position %d (effective with trim: %d)\n", id, pos, effectivePos);
        return 0;
    }

    if (strcmp(argv[1], "zero") == 0) {
        this->printf("Moving servos to Zero pose...\n");
        servo.programZero();
        this->printf("Zero pose completed.\n");
        return 0;
    }

    if (strcmp(argv[1], "center") == 0) {
        this->printf("Moving servos to Standby/Center pose...\n");
        servo.programCenter();
        this->printf("Center pose completed.\n");
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            this->printf("Usage: pwm run <prog_id>\n");
            this->printf("Programs: 1:Forward 2:Backward 3:TurnLeft 4:TurnRight 5:MoveLeft 6:MoveRight 11:FaceUp 12:FaceDown 99:Center 100:Zero\n");
            return -1;
        }
        int prog = atoi(argv[2]);
        _app->setServoProgram(prog);
        this->printf("Triggered servo program %d\n", prog);
        return 0;
    }

    this->printf("syntax error! Type 'pwm -h' for usage.\n");
    return -1;
}

int RoboHeroShell::unknown_command(int argc, char **argv)
{
    (void)(argc);

    this->printf("Unknown command '%s'!\n", argv[0]);

    return -1;
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
