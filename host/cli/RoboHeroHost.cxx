/*
 * RoboHeroHost.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <csignal>
#include <cstdarg>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

#include "RoboHeroHost.hxx"
#include "version.h"

using namespace std;

RoboHeroHost RoboHeroHost::_self;

static void signalHandler(int sig)
{
    (void) sig;
    RoboHeroHost::instance().requestExit();
}

RoboHeroHost &RoboHeroHost::instance()
{
    return _self;
}

RoboHeroHost::RoboHeroHost()
    : _running(false), _ncursesActive(false),
      _winTop(nullptr), _winMid(nullptr), _winBottom(nullptr),
      _topHeight(7), _bottomHeight(3),
      _scrollOffset(0), _cursorPos(0)
{
}

RoboHeroHost::~RoboHeroHost()
{
    cleanupNcurses();
}

void RoboHeroHost::requestExit()
{
    _running.store(false);
}

void RoboHeroHost::log(const string &msg)
{
    lock_guard<mutex> lock(_logMutex);

    stringstream ss(msg);
    string line;
    while (getline(ss, line)) {
        if (_logLines.size() >= MAX_LOG_LINES) {
            _logLines.pop_front();
        }
        _logLines.push_back(line);
    }
}

void RoboHeroHost::logf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    log(string(buf));
}

void RoboHeroHost::initNcurses()
{
    if (_ncursesActive) {
        return;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, -1);   // Connected / Success
        init_pair(2, COLOR_RED, -1);     // Disconnected / Alert
        init_pair(3, COLOR_CYAN, -1);    // Servos / Headers
        init_pair(4, COLOR_YELLOW, -1);  // Voltages / Info
        init_pair(5, COLOR_WHITE, COLOR_BLUE); // Title bar
    }

    _ncursesActive = true;
    createWindows();
}

void RoboHeroHost::cleanupNcurses()
{
    if (_ncursesActive) {
        destroyWindows();
        endwin();
        _ncursesActive = false;
    }
}

void RoboHeroHost::createWindows()
{
    destroyWindows();

    int totalLines = LINES;
    int totalCols = COLS;

    if (totalLines < 12) {
        _topHeight = 5;
        _bottomHeight = 2;
    } else {
        _topHeight = 7;
        _bottomHeight = 3;
    }

    int midHeight = totalLines - _topHeight - _bottomHeight;
    if (midHeight < 1) {
        midHeight = 1;
    }

    _winTop = newwin(_topHeight, totalCols, 0, 0);
    _winMid = newwin(midHeight, totalCols, _topHeight, 0);
    _winBottom = newwin(_bottomHeight, totalCols, _topHeight + midHeight, 0);

    keypad(_winBottom, TRUE);
    wtimeout(_winBottom, 50); // 50 ms timeout for non-blocking loop
}

void RoboHeroHost::destroyWindows()
{
    if (_winTop) {
        delwin(_winTop);
        _winTop = nullptr;
    }
    if (_winMid) {
        delwin(_winMid);
        _winMid = nullptr;
    }
    if (_winBottom) {
        delwin(_winBottom);
        _winBottom = nullptr;
    }
}

void RoboHeroHost::handleResize()
{
    endwin();
    refresh();
    createWindows();
    redrawAll();
}

void RoboHeroHost::drawTop()
{
    if (!_winTop) {
        return;
    }

    werase(_winTop);

    int totalCols = getmaxx(_winTop);
    Mqtt &mqtt = Mqtt::instance();
    bool connected = mqtt.isConnected();
    TelemetryData telem = mqtt.getLatestTelemetry();

    // Line 0: Header Banner
    wattron(_winTop, COLOR_PAIR(5) | A_BOLD);
    mvwhline(_winTop, 0, 0, ' ', totalCols);
    mvwprintw(_winTop, 0, 1, "RoboHero Host v%s", MYPROJECT_VERSION_STRING);
    string brokerStatus = connected ? "[MQTT CONNECTED]" : "[MQTT DISCONNECTED]";
    mvwprintw(_winTop, 0, totalCols - (int) brokerStatus.length() - 2, "%s",
              brokerStatus.c_str());
    wattroff(_winTop, COLOR_PAIR(5) | A_BOLD);

    // Line 1: Connection & Telemetry info
    mvwprintw(_winTop, 1, 1, "Broker: %s  |  Robot ID: %s",
              mqtt.getBrokerUri().c_str(),
              Configuration::instance().getMqttRobotId().c_str());

    // Voltage & Battery (Integer value)
    wattron(_winTop, A_BOLD);
    if (telem.hasVoltage && telem.voltage >= 0) {
        if (telem.voltage < INPUT_MIN_VOLTAGE) {
            wattron(_winTop, COLOR_PAIR(2));
            mvwprintw(_winTop, 1, totalCols - 24, "Batt: %d [LOW!]", telem.voltage);
            wattroff(_winTop, COLOR_PAIR(2));
        } else {
            wattron(_winTop, COLOR_PAIR(1));
            mvwprintw(_winTop, 1, totalCols - 20, "Batt: %d", telem.voltage);
            wattroff(_winTop, COLOR_PAIR(1));
        }
    } else {
        wattron(_winTop, COLOR_PAIR(4));
        mvwprintw(_winTop, 1, totalCols - 20, "Batt: --");
        wattroff(_winTop, COLOR_PAIR(4));
    }
    wattroff(_winTop, A_BOLD);

    // Separator line
    wattron(_winTop, COLOR_PAIR(3));
    mvwhline(_winTop, 2, 0, ACS_HLINE, totalCols);
    mvwprintw(_winTop, 2, 2, "[ Servo Positions (0..16) ]");
    wattroff(_winTop, COLOR_PAIR(3));

    // Lines 3, 4, 5: 17 Servo Grid
    for (int i = 0; i < ALLSERVOS; i++) {
        int row = 3 + (i / 6);
        int col = 1 + (i % 6) * 13;

        if (row >= _topHeight - 1 || col + 12 > totalCols) {
            break;
        }

        wattron(_winTop, COLOR_PAIR(3));
        mvwprintw(_winTop, row, col, "S%02d:", i);
        wattroff(_winTop, COLOR_PAIR(3));

        if (telem.servoValid[i]) {
            wattron(_winTop, A_BOLD);
            mvwprintw(_winTop, row, col + 4, "%4d", telem.servoPositions[i]);
            wattroff(_winTop, A_BOLD);
        } else {
            mvwprintw(_winTop, row, col + 4, " ---");
        }
    }

    // Bottom border of top pane
    mvwhline(_winTop, _topHeight - 1, 0, ACS_HLINE, totalCols);

    wrefresh(_winTop);
}

void RoboHeroHost::drawMiddle()
{
    if (!_winMid) {
        return;
    }

    werase(_winMid);

    int midHeight = getmaxy(_winMid);
    int midCols = getmaxx(_winMid);

    lock_guard<mutex> lock(_logMutex);

    size_t totalLines = _logLines.size();
    size_t maxScroll = (totalLines > (size_t) midHeight) ? (totalLines - midHeight) : 0;
    if (_scrollOffset > maxScroll) {
        _scrollOffset = maxScroll;
    }

    size_t startLine = (totalLines > (size_t) midHeight)
                           ? (totalLines - midHeight - _scrollOffset)
                           : 0;

    int y = 0;
    for (size_t i = startLine; i < totalLines && y < midHeight; i++, y++) {
        string line = _logLines[i];
        if ((int) line.length() > midCols - 1) {
            line = line.substr(0, midCols - 1);
        }
        mvwprintw(_winMid, y, 1, "%s", line.c_str());
    }

    // Scroll indicator if scrolled up
    if (_scrollOffset > 0) {
        wattron(_winMid, COLOR_PAIR(4) | A_REVERSE);
        string ind = " [SCROLLED UP: +" + to_string(_scrollOffset) + "] ";
        mvwprintw(_winMid, 0, midCols - (int) ind.length() - 2, "%s", ind.c_str());
        wattroff(_winMid, COLOR_PAIR(4) | A_REVERSE);
    }

    wrefresh(_winMid);
}

void RoboHeroHost::drawBottom()
{
    if (!_winBottom) {
        return;
    }

    werase(_winBottom);

    int totalCols = getmaxx(_winBottom);

    // Separator line
    wattron(_winBottom, COLOR_PAIR(3));
    mvwhline(_winBottom, 0, 0, ACS_HLINE, totalCols);
    mvwprintw(_winBottom, 0, 2, "[ Command Prompt ]");
    wattroff(_winBottom, COLOR_PAIR(3));

    // Prompt Line
    wattron(_winBottom, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(_winBottom, 1, 1, "robohero> ");
    wattroff(_winBottom, COLOR_PAIR(1) | A_BOLD);

    int promptLen = 11;
    mvwprintw(_winBottom, 1, promptLen, "%s", _inputLine.c_str());

    wmove(_winBottom, 1, promptLen + (int) _cursorPos);
    wrefresh(_winBottom);
}

void RoboHeroHost::redrawAll()
{
    drawTop();
    drawMiddle();
    drawBottom();
}

void RoboHeroHost::handleKey(int ch)
{
    int midHeight = _winMid ? getmaxy(_winMid) : 10;
    int pageSize = max(1, midHeight - 2);

    if (ch == KEY_RESIZE) {
        handleResize();
        return;
    }

    // Navigation keys for middle pane scroll history
    if (ch == KEY_UP) {
        _scrollOffset++;
        drawMiddle();
        return;
    } else if (ch == KEY_DOWN) {
        if (_scrollOffset > 0) {
            _scrollOffset--;
        }
        drawMiddle();
        return;
    } else if (ch == KEY_PPAGE) { // Page Up
        _scrollOffset += pageSize;
        drawMiddle();
        return;
    } else if (ch == KEY_NPAGE) { // Page Down
        if (_scrollOffset > (size_t) pageSize) {
            _scrollOffset -= pageSize;
        } else {
            _scrollOffset = 0;
        }
        drawMiddle();
        return;
    }

    // Any other key resets scroll to the end/latest line
    if (_scrollOffset > 0) {
        _scrollOffset = 0;
        drawMiddle();
    }

    // Line editing and command dispatch
    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        string cmd = _inputLine;
        _inputLine.clear();
        _cursorPos = 0;
        executeCommand(cmd);
        drawBottom();
        drawMiddle();
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (_cursorPos > 0 && !_inputLine.empty()) {
            _inputLine.erase(_cursorPos - 1, 1);
            _cursorPos--;
            drawBottom();
        }
    } else if (ch == KEY_DC) { // Delete
        if (_cursorPos < _inputLine.length()) {
            _inputLine.erase(_cursorPos, 1);
            drawBottom();
        }
    } else if (ch == KEY_LEFT) {
        if (_cursorPos > 0) {
            _cursorPos--;
            drawBottom();
        }
    } else if (ch == KEY_RIGHT) {
        if (_cursorPos < _inputLine.length()) {
            _cursorPos++;
            drawBottom();
        }
    } else if (ch == KEY_HOME || ch == 1) { // Home / Ctrl+A
        _cursorPos = 0;
        drawBottom();
    } else if (ch == KEY_END || ch == 5) { // End / Ctrl+E
        _cursorPos = _inputLine.length();
        drawBottom();
    } else if (ch == 3 || ch == 21) { // Ctrl+C / Ctrl+U
        _inputLine.clear();
        _cursorPos = 0;
        drawBottom();
    } else if (ch == 4) { // Ctrl+D
        if (_inputLine.empty()) {
            requestExit();
        }
    } else if (isprint(ch)) {
        _inputLine.insert(_cursorPos, 1, static_cast<char>(ch));
        _cursorPos++;
        drawBottom();
    }
}

void RoboHeroHost::executeCommand(const string &cmdLine)
{
    string trimmed = cmdLine;
    while (!trimmed.empty() && isspace(trimmed.front())) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && isspace(trimmed.back())) trimmed.pop_back();

    if (trimmed.empty()) {
        return;
    }

    log("> " + trimmed);

    stringstream ss(trimmed);
    string verb;
    ss >> verb;

    vector<string> args;
    string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }

    transform(verb.begin(), verb.end(), verb.begin(), ::tolower);

    if (verb == "help" || verb == "?") {
        cmdHelp();
    } else if (verb == "status") {
        cmdStatus();
    } else if (verb == "connect") {
        cmdConnect(args);
    } else if (verb == "disconnect") {
        cmdDisconnect();
    } else if (verb == "config") {
        cmdConfig(args);
    } else if (verb == "pm") {
        cmdPm(args);
    } else if (verb == "pms") {
        cmdPms(args);
    } else if (verb == "pwm") {
        cmdPwm(args);
    } else if (verb == "stop") {
        cmdStop();
    } else if (verb == "center") {
        cmdCenter();
    } else if (verb == "zero") {
        cmdZero();
    } else if (verb == "relax") {
        cmdRelax();
    } else if (verb == "version") {
        cmdVersion();
    } else if (verb == "clear") {
        cmdClear();
    } else if (verb == "quit" || verb == "exit") {
        requestExit();
    } else {
        logf("Unknown command: '%s'. Type 'help' for available commands.", verb.c_str());
    }
}

void RoboHeroHost::cmdHelp()
{
    log("Available Commands:");
    log("  help, ?                    - Show this help message");
    log("  status                     - Show connection and telemetry details");
    log("  connect [host] [port]      - Connect to MQTT broker");
    log("  disconnect                 - Disconnect from MQTT broker");
    log("  config [show|save|reload]  - View/save configuration settings");
    log("  config set <key> <val>     - Change a configuration value (e.g. config set host 192.168.1.10)");
    log("  pm <id>                    - Trigger motion program (1..15, 20..21)");
    log("  pms <id>                   - Trigger single motion program step");
    log("  pwm <chan> <pos>           - Move servo channel (0..16) to pulse pos (1..270)");
    log("  stop                       - Request robot stop current motion");
    log("  center                     - Return servos to center pose");
    log("  zero                       - Move servos to zero pose");
    log("  relax                      - Relax servos (turn off PWM)");
    log("  version                    - Display version and build information");
    log("  clear                      - Clear the output log buffer");
    log("  quit, exit                 - Exit the host application");
    log("Navigation: [Up/Down/PgUp/PgDn] scrolls output history (500 lines max).");
}

void RoboHeroHost::cmdStatus()
{
    Mqtt &mqtt = Mqtt::instance();
    Configuration &cfg = Configuration::instance();
    TelemetryData telem = mqtt.getLatestTelemetry();

    logf("MQTT Broker:   %s (%s)", mqtt.getBrokerUri().c_str(),
         mqtt.isConnected() ? "Connected" : "Disconnected");
    logf("Target Robot:  %s", cfg.getMqttRobotId().c_str());
    logf("Control Topic: %s", mqtt.getControlTopic().c_str());
    logf("Status Topic:  %s", mqtt.getStatusTopic().c_str());
    logf("Config File:   %s", cfg.getConfigPath().c_str());

    if (telem.hasVoltage && telem.voltage >= 0) {
        logf("Battery Volt:  %d (min: %d)", telem.voltage, INPUT_MIN_VOLTAGE);
    } else {
        log("Battery Volt:  --");
    }

    if (telem.timestampMs > 0) {
        logf("Timestamp:     %u ms", telem.timestampMs);
    }
}

void RoboHeroHost::cmdConnect(const vector<string> &args)
{
    string host = "";
    int port = 0;
    if (!args.empty()) {
        host = args[0];
    }
    if (args.size() >= 2) {
        try {
            port = stoi(args[1]);
        } catch (...) {}
    }

    logf("Connecting to MQTT broker %s...",
         host.empty() ? Configuration::instance().getMqttHost().c_str() : host.c_str());
    if (!Mqtt::instance().start(host, port)) {
        log("Failed to initiate MQTT connection.");
    }
}

void RoboHeroHost::cmdDisconnect()
{
    log("Disconnecting from MQTT broker...");
    Mqtt::instance().stop();
    log("Disconnected.");
}

void RoboHeroHost::cmdConfig(const vector<string> &args)
{
    Configuration &cfg = Configuration::instance();
    if (args.empty() || args[0] == "show") {
        logf("Configuration (%s):", cfg.getConfigPath().c_str());
        logf("  mqtt.host      = \"%s\"", cfg.getMqttHost().c_str());
        logf("  mqtt.port      = %d", cfg.getMqttPort());
        logf("  mqtt.username  = \"%s\"", cfg.getMqttUsername().c_str());
        logf("  mqtt.client_id = \"%s\"", cfg.getMqttClientId().c_str());
        logf("  mqtt.robot_id  = \"%s\"", cfg.getMqttRobotId().c_str());
        logf("  mqtt.keepalive = %d", cfg.getMqttKeepalive());
    } else if (args[0] == "save") {
        if (cfg.save()) {
            logf("Configuration saved to %s", cfg.getConfigPath().c_str());
        } else {
            logf("Failed to save configuration to %s", cfg.getConfigPath().c_str());
        }
    } else if (args[0] == "reload") {
        if (cfg.load()) {
            logf("Configuration reloaded from %s", cfg.getConfigPath().c_str());
        } else {
            logf("Failed to reload configuration from %s", cfg.getConfigPath().c_str());
        }
    } else if (args[0] == "set" && args.size() >= 3) {
        string key = args[1];
        string val = args[2];
        if (key == "host" || key == "mqtt.host") {
            cfg.setMqttHost(val);
        } else if (key == "port" || key == "mqtt.port") {
            cfg.setMqttPort(stoi(val));
        } else if (key == "username" || key == "mqtt.username") {
            cfg.setMqttUsername(val);
        } else if (key == "password" || key == "mqtt.password") {
            cfg.setMqttPassword(val);
        } else if (key == "client_id" || key == "mqtt.client_id") {
            cfg.setMqttClientId(val);
        } else if (key == "robot_id" || key == "mqtt.robot_id") {
            cfg.setMqttRobotId(val);
        } else if (key == "keepalive" || key == "mqtt.keepalive") {
            cfg.setMqttKeepalive(stoi(val));
        } else {
            logf("Unknown config parameter '%s'", key.c_str());
            return;
        }
        cfg.save();
        logf("Updated and saved %s = %s", key.c_str(), val.c_str());
    } else {
        log("Usage: config [show | save | reload | set <key> <val>]");
    }
}

void RoboHeroHost::cmdPm(const vector<string> &args)
{
    if (args.empty()) {
        log("Usage: pm <program_id> (e.g. pm 1)");
        return;
    }
    int id = 0;
    try {
        id = stoi(args[0]);
    } catch (...) {
        log("Invalid program ID: " + args[0]);
        return;
    }
    if (Mqtt::instance().sendPm(id)) {
        logf("Sent PM command: %d", id);
    } else {
        log("Failed to send PM command (MQTT not connected)");
    }
}

void RoboHeroHost::cmdPms(const vector<string> &args)
{
    if (args.empty()) {
        log("Usage: pms <program_id> (e.g. pms 1)");
        return;
    }
    int id = 0;
    try {
        id = stoi(args[0]);
    } catch (...) {
        log("Invalid program ID: " + args[0]);
        return;
    }
    if (Mqtt::instance().sendPms(id)) {
        logf("Sent PMS command: %d", id);
    } else {
        log("Failed to send PMS command (MQTT not connected)");
    }
}

void RoboHeroHost::cmdPwm(const vector<string> &args)
{
    if (args.size() < 2) {
        log("Usage: pwm <channel 0..16> <pos 1..270>");
        return;
    }
    int chan = 0;
    int pos = 0;
    try {
        chan = stoi(args[0]);
        pos = stoi(args[1]);
    } catch (...) {
        log("Invalid arguments for pwm command");
        return;
    }
    if (chan < 0 || chan >= ALLSERVOS) {
        logf("Channel %d out of range (0..%d)", chan, ALLSERVOS - 1);
        return;
    }
    if (pos < PWMRES_MIN || pos > PWMRES_MAX) {
        logf("Position %d out of range (%d..%d)", pos, PWMRES_MIN, PWMRES_MAX);
        return;
    }
    if (Mqtt::instance().sendPwm(chan, pos)) {
        logf("Sent PWM command: Ch%d -> %d", chan, pos);
    } else {
        log("Failed to send PWM command (MQTT not connected)");
    }
}

void RoboHeroHost::cmdStop()
{
    if (Mqtt::instance().sendStop()) {
        log("Sent: STOP");
    } else {
        log("Failed to send STOP (MQTT not connected)");
    }
}

void RoboHeroHost::cmdCenter()
{
    if (Mqtt::instance().sendCenter()) {
        log("Sent: CENTER");
    } else {
        log("Failed to send CENTER (MQTT not connected)");
    }
}

void RoboHeroHost::cmdZero()
{
    if (Mqtt::instance().sendZero()) {
        log("Sent: ZERO");
    } else {
        log("Failed to send ZERO (MQTT not connected)");
    }
}

void RoboHeroHost::cmdRelax()
{
    if (Mqtt::instance().sendRelax()) {
        log("Sent: RELAX (servos limp)");
    } else {
        log("Failed to send RELAX (MQTT not connected)");
    }
}

void RoboHeroHost::cmdVersion()
{
    logf("The RoboHero Host Application");
    logf("Version: %s", MYPROJECT_VERSION_STRING);
    logf("Built:   %s@%s %s", MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    logf("Copyright (C) 2026, Charles Chiou");
}

void RoboHeroHost::cmdClear()
{
    lock_guard<mutex> lock(_logMutex);
    _logLines.clear();
    _scrollOffset = 0;
}

int RoboHeroHost::run(int argc, char **argv)
{
    string customConfig = "";
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -c, --config <file>   Use custom configuration file (default: ~/.robohero)\n");
            printf("  -v, --version         Show version information\n");
            printf("  -h, --help            Show this help message\n");
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            printf("RoboHero Host v%s (built %s@%s %s)\n",
                   MYPROJECT_VERSION_STRING, MYPROJECT_WHOAMI,
                   MYPROJECT_HOSTNAME, MYPROJECT_DATE);
            return 0;
        } else if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            customConfig = argv[++i];
        }
    }

    // Initialize Configuration (creates ~/.robohero with defaults if missing)
    Configuration &cfg = Configuration::instance();
    cfg.init(customConfig);

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Initialize ncurses UI
    initNcurses();

    // Welcome banner in middle pane
    logf("RoboHero Host Terminal v%s", MYPROJECT_VERSION_STRING);
    logf("Loaded configuration: %s", cfg.getConfigPath().c_str());
    log("Type 'help' for command list. Up/Down/PgUp/PgDn scrolls output.");

    // Setup MQTT callbacks
    Mqtt &mqtt = Mqtt::instance();
    mqtt.onConnection([this](bool connected, const string &info) {
        logf("[MQTT] %s", info.c_str());
        (void) connected;
    });

    mqtt.onTelemetry([this](const TelemetryData &telem) {
        (void) telem;
        // Top window will automatically redraw with latest values
    });

    // Auto-connect to broker on startup
    mqtt.start();

    _running.store(true);

    while (_running.load()) {
        drawTop();
        drawMiddle();
        drawBottom();

        int ch = wgetch(_winBottom);
        if (ch != ERR) {
            handleKey(ch);
        }

        this_thread::sleep_for(chrono::milliseconds(20));
    }

    mqtt.stop();
    cleanupNcurses();
    printf("RoboHero Host terminated.\n");
    return 0;
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
