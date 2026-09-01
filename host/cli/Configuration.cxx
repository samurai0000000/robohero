/*
 * Configuration.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Configuration.hxx"

using namespace std;

Configuration Configuration::_self;

Configuration &Configuration::instance()
{
    return _self;
}

Configuration::Configuration()
    : _configPath(getDefaultConfigPath()), _loaded(false),
      _mqttHost("localhost"), _mqttPort(1883), _mqttUsername(""),
      _mqttPassword(""), _mqttClientId("robohero-host"),
      _mqttRobotId("robohero"), _mqttKeepalive(60)
{
}

string Configuration::expandHome(const string &path)
{
    if (path.empty()) {
        return "";
    }
    if (path[0] != '~') {
        return path;
    }

    const char *home = getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            home = pw->pw_dir;
        }
    }

    if (home == nullptr) {
        return path;
    }

    if (path == "~") {
        return string(home);
    }
    if (path.length() >= 2 && (path[1] == '/' || path[1] == '\\')) {
        return string(home) + path.substr(1);
    }

    return path;
}

string Configuration::getDefaultConfigPath()
{
    return expandHome("~/.robohero");
}

const string &Configuration::getConfigPath() const
{
    return _configPath;
}

void Configuration::populateDefaults()
{
    _mqttHost = "localhost";
    _mqttPort = 1883;
    _mqttUsername = "";
    _mqttPassword = "";
    _mqttClientId = "robohero-host";
    _mqttRobotId = "robohero";
    _mqttKeepalive = 60;
}

void Configuration::syncToConfig()
{
    libconfig::Setting &root = _cfg.getRoot();

    libconfig::Setting *mqtt = nullptr;
    if (root.exists("mqtt")) {
        mqtt = &root["mqtt"];
    } else {
        mqtt = &root.add("mqtt", libconfig::Setting::TypeGroup);
    }

    auto setOrAddString = [](libconfig::Setting &group, const char *name,
                             const string &val) {
        if (group.exists(name)) {
            group[name] = val;
        } else {
            group.add(name, libconfig::Setting::TypeString) = val;
        }
    };

    auto setOrAddInt = [](libconfig::Setting &group, const char *name,
                          int val) {
        if (group.exists(name)) {
            group[name] = val;
        } else {
            group.add(name, libconfig::Setting::TypeInt) = val;
        }
    };

    setOrAddString(*mqtt, "host", _mqttHost);
    setOrAddInt(*mqtt, "port", _mqttPort);
    setOrAddString(*mqtt, "username", _mqttUsername);
    setOrAddString(*mqtt, "password", _mqttPassword);
    setOrAddString(*mqtt, "client_id", _mqttClientId);
    setOrAddString(*mqtt, "robot_id", _mqttRobotId);
    setOrAddInt(*mqtt, "keepalive", _mqttKeepalive);
}

void Configuration::syncFromConfig()
{
    const libconfig::Setting &root = _cfg.getRoot();
    if (!root.exists("mqtt")) {
        return;
    }

    const libconfig::Setting &mqtt = root["mqtt"];

    string strVal;
    int intVal;

    if (mqtt.lookupValue("host", strVal)) {
        _mqttHost = strVal;
    }
    if (mqtt.lookupValue("port", intVal)) {
        _mqttPort = intVal;
    }
    if (mqtt.lookupValue("username", strVal)) {
        _mqttUsername = strVal;
    }
    if (mqtt.lookupValue("password", strVal)) {
        _mqttPassword = strVal;
    }
    if (mqtt.lookupValue("client_id", strVal)) {
        _mqttClientId = strVal;
    }
    if (mqtt.lookupValue("robot_id", strVal)) {
        _mqttRobotId = strVal;
    }
    if (mqtt.lookupValue("keepalive", intVal)) {
        _mqttKeepalive = intVal;
    }
}

bool Configuration::init(const string &configPath)
{
    if (!configPath.empty()) {
        _configPath = expandHome(configPath);
    } else if (_configPath.empty()) {
        _configPath = getDefaultConfigPath();
    }

    struct stat st;
    if (stat(_configPath.c_str(), &st) != 0) {
        // Config file does not exist, populate with defaults and save
        populateDefaults();
        syncToConfig();
        return save(_configPath);
    }

    return load(_configPath);
}

bool Configuration::load(const string &configPath)
{
    string path = configPath.empty() ? _configPath : expandHome(configPath);
    _configPath = path;

    try {
        _cfg.readFile(path.c_str());
        syncFromConfig();
        _loaded = true;
        return true;
    } catch (const libconfig::FileIOException &e) {
        populateDefaults();
        syncToConfig();
        save(path);
        _loaded = true;
        return true;
    } catch (const libconfig::ParseException &e) {
        cerr << "Config parse error at " << e.getFile() << ":"
             << e.getLine() << " - " << e.getError() << endl;
        return false;
    }
}

bool Configuration::save(const string &configPath)
{
    string path = configPath.empty() ? _configPath : expandHome(configPath);
    _configPath = path;

    syncToConfig();

    try {
        _cfg.writeFile(path.c_str());
        _loaded = true;
        return true;
    } catch (const libconfig::FileIOException &e) {
        cerr << "Failed to write config file: " << path << endl;
        return false;
    }
}

string Configuration::getMqttHost() const
{
    return _mqttHost;
}

int Configuration::getMqttPort() const
{
    return _mqttPort;
}

string Configuration::getMqttUsername() const
{
    return _mqttUsername;
}

string Configuration::getMqttPassword() const
{
    return _mqttPassword;
}

string Configuration::getMqttClientId() const
{
    return _mqttClientId;
}

string Configuration::getMqttRobotId() const
{
    return _mqttRobotId;
}

int Configuration::getMqttKeepalive() const
{
    return _mqttKeepalive;
}

void Configuration::setMqttHost(const string &host)
{
    _mqttHost = host;
}

void Configuration::setMqttPort(int port)
{
    _mqttPort = port;
}

void Configuration::setMqttUsername(const string &user)
{
    _mqttUsername = user;
}

void Configuration::setMqttPassword(const string &pass)
{
    _mqttPassword = pass;
}

void Configuration::setMqttClientId(const string &cid)
{
    _mqttClientId = cid;
}

void Configuration::setMqttRobotId(const string &rid)
{
    _mqttRobotId = rid;
}

void Configuration::setMqttKeepalive(int keepalive)
{
    _mqttKeepalive = keepalive;
}

bool Configuration::lookupString(const string &path, string &val) const
{
    return _cfg.lookupValue(path, val);
}

bool Configuration::lookupInt(const string &path, int &val) const
{
    return _cfg.lookupValue(path, val);
}

bool Configuration::lookupBool(const string &path, bool &val) const
{
    return _cfg.lookupValue(path, val);
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
