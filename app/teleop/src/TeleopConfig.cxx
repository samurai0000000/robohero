/*
 * TeleopConfig.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "TeleopConfig.hxx"
#include <cstdlib>
#include <sys/stat.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

TeleopConfig TeleopConfig::_self;

TeleopConfig &TeleopConfig::instance()
{
    return _self;
}

TeleopConfig::TeleopConfig()
    : _loaded(false)
{
    populateDefaults();
}

std::string TeleopConfig::getDefaultConfigPath()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string dir = std::string(path) + "\\robohero";
        CreateDirectoryA(dir.c_str(), NULL);
        return dir + "\\teleop.cfg";
    }
    return "teleop.cfg";
#else
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    if (home) {
        std::string dir = std::string(home) + "/.config/robohero";
        mkdir(dir.c_str(), 0755);
        return dir + "/teleop.cfg";
    }
    return "teleop.cfg";
#endif
}

void TeleopConfig::populateDefaults()
{
    camera = CameraConfig();
    ai = AiConfig();
    retarget = RetargetConfig();
    safety = SafetyConfig();
    mqtt = MqttConfig();
    view = ViewConfig();
}

const std::string &TeleopConfig::getConfigPath() const
{
    return _configPath;
}

bool TeleopConfig::load(const std::string &path)
{
    if (path.empty()) {
        _configPath = getDefaultConfigPath();
    } else {
        _configPath = path;
    }

    try {
        _cfg.readFile(_configPath.c_str());
        syncFromConfig();
        _loaded = true;
        return true;
    } catch (const libconfig::FileIOException &) {
        // File does not exist yet; populate defaults and save
        populateDefaults();
        save(_configPath);
        _loaded = true;
        return true;
    } catch (const libconfig::ParseException &ex) {
        std::cerr << "Config parse error at " << ex.getFile() << ":"
                  << ex.getLine() << " - " << ex.getError() << std::endl;
        return false;
    }
}

bool TeleopConfig::save(const std::string &path)
{
    if (!path.empty()) {
        _configPath = path;
    }
    if (_configPath.empty()) {
        _configPath = getDefaultConfigPath();
    }

    syncToConfig();

    try {
        _cfg.writeFile(_configPath.c_str());
        return true;
    } catch (const libconfig::FileIOException &ex) {
        std::cerr << "Failed to write config file " << _configPath << std::endl;
        return false;
    }
}

void TeleopConfig::syncFromConfig()
{
    libconfig::Setting &root = _cfg.getRoot();

    if (root.exists("camera")) {
        libconfig::Setting &s = root["camera"];
        s.lookupValue("deviceIndex", camera.deviceIndex);
        s.lookupValue("width", camera.width);
        s.lookupValue("height", camera.height);
        s.lookupValue("targetFps", camera.targetFps);
        s.lookupValue("mirrorVideo", camera.mirrorVideo);
    }

    if (root.exists("ai")) {
        libconfig::Setting &s = root["ai"];
        s.lookupValue("modelSource", ai.modelSource);
        s.lookupValue("customModelPath", ai.customModelPath);
        s.lookupValue("provider", ai.provider);
        s.lookupValue("confThreshold", ai.confThreshold);
        s.lookupValue("kptThreshold", ai.kptThreshold);
        s.lookupValue("targetSelection", ai.targetSelection);
    }

    if (root.exists("retarget")) {
        libconfig::Setting &s = root["retarget"];
        s.lookupValue("smoothingAlpha", retarget.smoothingAlpha);
        s.lookupValue("deadbandDeg", retarget.deadbandDeg);
        s.lookupValue("maxVelocityDegS", retarget.maxVelocityDegS);
        s.lookupValue("mode", retarget.mode);
        s.lookupValue("stancePreset", retarget.stancePreset);
    }

    if (root.exists("safety")) {
        libconfig::Setting &s = root["safety"];
        s.lookupValue("safetyMarginDeg", safety.safetyMarginDeg);
        s.lookupValue("txRateHz", safety.txRateHz);
        s.lookupValue("watchdogTimeoutMs", safety.watchdogTimeoutMs);
    }

    if (root.exists("mqtt")) {
        libconfig::Setting &s = root["mqtt"];
        s.lookupValue("host", mqtt.host);
        s.lookupValue("port", mqtt.port);
        s.lookupValue("robotId", mqtt.robotId);
        s.lookupValue("keepalive", mqtt.keepalive);
        s.lookupValue("autoConnect", mqtt.autoConnect);
    }

    if (root.exists("view")) {
        libconfig::Setting &s = root["view"];
        s.lookupValue("showRobotMeshes", view.showRobotMeshes);
        s.lookupValue("showSkeleton", view.showSkeleton);
        s.lookupValue("showGrid", view.showGrid);
        s.lookupValue("darkTheme", view.darkTheme);
    }
}

void TeleopConfig::syncToConfig()
{
    libconfig::Setting &root = _cfg.getRoot();

    auto ensureGroup = [&root](const char *name) -> libconfig::Setting & {
        if (!root.exists(name)) {
            return root.add(name, libconfig::Setting::TypeGroup);
        }
        return root[name];
    };

    auto setOrAddString = [](libconfig::Setting &s, const char *key, const std::string &val) {
        if (!s.exists(key)) {
            s.add(key, libconfig::Setting::TypeString) = val;
        } else {
            s[key] = val;
        }
    };

    auto setOrAddInt = [](libconfig::Setting &s, const char *key, int val) {
        if (!s.exists(key)) {
            s.add(key, libconfig::Setting::TypeInt) = val;
        } else {
            s[key] = val;
        }
    };

    auto setOrAddFloat = [](libconfig::Setting &s, const char *key, float val) {
        if (!s.exists(key)) {
            s.add(key, libconfig::Setting::TypeFloat) = val;
        } else {
            s[key] = val;
        }
    };

    auto setOrAddBool = [](libconfig::Setting &s, const char *key, bool val) {
        if (!s.exists(key)) {
            s.add(key, libconfig::Setting::TypeBoolean) = val;
        } else {
            s[key] = val;
        }
    };

    libconfig::Setting &cam = ensureGroup("camera");
    setOrAddInt(cam, "deviceIndex", camera.deviceIndex);
    setOrAddInt(cam, "width", camera.width);
    setOrAddInt(cam, "height", camera.height);
    setOrAddInt(cam, "targetFps", camera.targetFps);
    setOrAddBool(cam, "mirrorVideo", camera.mirrorVideo);

    libconfig::Setting &aiGrp = ensureGroup("ai");
    setOrAddString(aiGrp, "modelSource", ai.modelSource);
    setOrAddString(aiGrp, "customModelPath", ai.customModelPath);
    setOrAddString(aiGrp, "provider", ai.provider);
    setOrAddFloat(aiGrp, "confThreshold", ai.confThreshold);
    setOrAddFloat(aiGrp, "kptThreshold", ai.kptThreshold);
    setOrAddString(aiGrp, "targetSelection", ai.targetSelection);

    libconfig::Setting &ret = ensureGroup("retarget");
    setOrAddFloat(ret, "smoothingAlpha", retarget.smoothingAlpha);
    setOrAddFloat(ret, "deadbandDeg", retarget.deadbandDeg);
    setOrAddFloat(ret, "maxVelocityDegS", retarget.maxVelocityDegS);
    setOrAddString(ret, "mode", retarget.mode);
    setOrAddString(ret, "stancePreset", retarget.stancePreset);

    libconfig::Setting &saf = ensureGroup("safety");
    setOrAddFloat(saf, "safetyMarginDeg", safety.safetyMarginDeg);
    setOrAddInt(saf, "txRateHz", safety.txRateHz);
    setOrAddInt(saf, "watchdogTimeoutMs", safety.watchdogTimeoutMs);

    libconfig::Setting &mq = ensureGroup("mqtt");
    setOrAddString(mq, "host", mqtt.host);
    setOrAddInt(mq, "port", mqtt.port);
    setOrAddString(mq, "robotId", mqtt.robotId);
    setOrAddInt(mq, "keepalive", mqtt.keepalive);
    setOrAddBool(mq, "autoConnect", mqtt.autoConnect);

    libconfig::Setting &vw = ensureGroup("view");
    setOrAddBool(vw, "showRobotMeshes", view.showRobotMeshes);
    setOrAddBool(vw, "showSkeleton", view.showSkeleton);
    setOrAddBool(vw, "showGrid", view.showGrid);
    setOrAddBool(vw, "darkTheme", view.darkTheme);
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
