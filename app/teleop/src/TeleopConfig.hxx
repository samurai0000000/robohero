/*
 * TeleopConfig.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_TELEOP_CONFIG_HXX
#define ROBOHERO_TELEOP_CONFIG_HXX

#include <libconfig.h++>
#include <string>

class TeleopConfig
{
public:
    struct CameraConfig
    {
        int deviceIndex = 0;
        int width = 640;
        int height = 480;
        int targetFps = 30;
        bool mirrorVideo = true;
    };

    struct AiConfig
    {
        std::string modelSource = "embedded"; // "embedded" or "custom"
        std::string customModelPath = "";
        std::string provider = "auto";       // "auto", "dml", "cuda", "cpu"
        float confThreshold = 0.50f;
        float kptThreshold = 0.40f;
        std::string targetSelection = "largest"; // "largest", "center", "confidence"
    };

    struct RetargetConfig
    {
        float smoothingAlpha = 0.35f;
        float deadbandDeg = 1.0f;
        float maxVelocityDegS = 180.0f;
        std::string mode = "upper_body";     // "upper_body" or "full_body"
        std::string stancePreset = "neutral";
    };

    struct SafetyConfig
    {
        float safetyMarginDeg = 2.0f;
        int txRateHz = 30;
        int watchdogTimeoutMs = 500;
    };

    struct MqttConfig
    {
        std::string host = "localhost";
        int port = 1883;
        std::string username = "";
        std::string password = "";
        std::string robotId = "robohero";
        int keepalive = 60;
        bool autoConnect = true;
    };

    struct ViewConfig
    {
        bool showRobotMeshes = true;
        bool showSkeleton = true;
        bool showGrid = true;
        bool darkTheme = true;
    };

    static TeleopConfig &instance();

    bool load(const std::string &path = "");
    bool save(const std::string &path = "");

    const std::string &getConfigPath() const;

    CameraConfig camera;
    AiConfig ai;
    RetargetConfig retarget;
    SafetyConfig safety;
    MqttConfig mqtt;
    ViewConfig view;

    static std::string getDefaultConfigPath();

private:
    TeleopConfig();
    TeleopConfig(const TeleopConfig &) = delete;
    TeleopConfig &operator=(const TeleopConfig &) = delete;

    void populateDefaults();
    void syncFromConfig();
    void syncToConfig();

    static TeleopConfig _self;
    libconfig::Config _cfg;
    std::string _configPath;
    bool _loaded;
};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
