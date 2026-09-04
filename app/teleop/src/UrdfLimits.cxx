/*
 * UrdfLimits.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "UrdfLimits.hxx"
#include <QFile>
#include <QByteArray>
#include <pugixml.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

UrdfLimits UrdfLimits::_self;

static const UrdfLimits::ChannelCalibration CHANNEL_MAP[17] = {
    {"left_ankle_roll_joint",      0,  160,  1.0, (M_PI / 180.0), -0.4363,  1.3090},
    {"left_ankle_pitch_joint",     1,  161, -1.0, (M_PI / 180.0), -0.7854,  1.5708},
    {"left_knee_pitch_joint",      2,  141, -1.0, (M_PI / 180.0), -0.6981,  1.5708},
    {"left_hip_pitch_joint",       3,  168,  1.0, (M_PI / 180.0), -0.6981,  1.5708},
    {"left_hip_roll_joint",        4,  158, -1.0, (M_PI / 180.0), -0.3491,  1.6057},
    {"left_shoulder_pitch_joint",  5,  158,  1.0, 0.01823869,     -1.8117,  1.3055},
    {"left_shoulder_roll_joint",   6,  252, -1.0, 0.01649336,      0.0000,  4.7298},
    {"left_elbow_joint",           7,  159,  1.0, (M_PI / 180.0), -1.3090,  1.4835},
    {"right_elbow_joint",          8,  163,  1.0, (M_PI / 180.0), -1.4835,  0.9599},
    {"right_shoulder_roll_joint",  9,   69, -1.0, 0.01692969,     -4.3284,  0.0000},
    {"right_shoulder_pitch_joint", 10, 163, -1.0, 0.02068215,     -2.4906,  2.0054},
    {"right_hip_roll_joint",       11, 161, -1.0, (M_PI / 180.0), -1.7453,  0.3491},
    {"right_hip_pitch_joint",      12, 129, -1.0, (M_PI / 180.0), -0.6981,  1.5708},
    {"right_knee_pitch_joint",     13, 150,  1.0, (M_PI / 180.0), -0.6981,  1.5708},
    {"right_ankle_pitch_joint",    14, 165,  1.0, (M_PI / 180.0), -0.8727,  1.5708},
    {"right_ankle_roll_joint",     15, 162,  1.0, (M_PI / 180.0), -0.4363,  1.2217},
    {"head_yaw_joint",             16,  90,  1.0, 0.02827433,     -0.9756,  0.9233},
};

UrdfLimits &UrdfLimits::instance()
{
    return _self;
}

UrdfLimits::UrdfLimits()
    : _initialized(false)
{
    populateFallbackLimits();
}

void UrdfLimits::populateFallbackLimits()
{
    for (int i = 0; i < TOTAL_SERVOS; ++i) {
        _limits[i] = {CHANNEL_MAP[i].name, CHANNEL_MAP[i].channel,
                      CHANNEL_MAP[i].lower, CHANNEL_MAP[i].upper, 0.0};
    }
}

bool UrdfLimits::init(const std::string &urdfResourcePath)
{
    populateFallbackLimits();

    QFile file(QString::fromStdString(urdfResourcePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "UrdfLimits: could not open " << urdfResourcePath
                  << ", using fallback limits." << std::endl;
        _initialized = true;
        return true;
    }

    QByteArray xmlData = file.readAll();
    file.close();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xmlData.constData(), xmlData.size());
    if (!result) {
        std::cerr << "UrdfLimits: XML parse failed: " << result.description() << std::endl;
        _initialized = true;
        return false;
    }

    pugi::xml_node robot = doc.child("robot");
    if (!robot) {
        std::cerr << "UrdfLimits: missing <robot> tag in URDF." << std::endl;
        _initialized = true;
        return false;
    }

    // Map joint names to PCA9685 servo channels
    const std::map<std::string, int> nameToChannel = {
        {"left_ankle_roll_joint", 0},
        {"left_ankle_roll", 0},
        {"left_ankle_pitch_joint", 1},
        {"left_ankle_pitch", 1},
        {"left_knee_pitch_joint", 2},
        {"left_knee_pitch", 2},
        {"left_hip_pitch_joint", 3},
        {"left_hip_pitch", 3},
        {"left_hip_roll_joint", 4},
        {"left_hip_roll", 4},
        {"left_shoulder_pitch_joint", 5},
        {"left_shoulder_pitch", 5},
        {"left_shoulder_roll_joint", 6},
        {"left_shoulder_roll", 6},
        {"left_elbow_joint", 7},
        {"left_elbow", 7},
        {"right_elbow_joint", 8},
        {"right_elbow", 8},
        {"right_shoulder_roll_joint", 9},
        {"right_shoulder_roll", 9},
        {"right_shoulder_pitch_joint", 10},
        {"right_shoulder_pitch", 10},
        {"right_hip_roll_joint", 11},
        {"right_hip_roll", 11},
        {"right_hip_pitch_joint", 12},
        {"right_hip_pitch", 12},
        {"right_knee_pitch_joint", 13},
        {"right_knee_pitch", 13},
        {"right_ankle_pitch_joint", 14},
        {"right_ankle_pitch", 14},
        {"right_ankle_roll_joint", 15},
        {"right_ankle_roll", 15},
        {"head_yaw_joint", 16},
        {"head_yaw", 16}
    };

    for (pugi::xml_node joint = robot.child("joint"); joint; joint = joint.next_sibling("joint")) {
        std::string jName = joint.attribute("name").as_string();
        auto it = nameToChannel.find(jName);
        if (it != nameToChannel.end()) {
            int ch = it->second;
            pugi::xml_node limitNode = joint.child("limit");
            if (limitNode) {
                double lower = limitNode.attribute("lower").as_double(_limits[ch].lower);
                double upper = limitNode.attribute("upper").as_double(_limits[ch].upper);
                _limits[ch] = {jName, ch, lower, upper, 0.0};
            }
        }
    }

    _initialized = true;
    return true;
}

bool UrdfLimits::hasLimit(int channel) const
{
    return channel >= 0 && channel < TOTAL_SERVOS;
}

UrdfLimits::JointLimit UrdfLimits::getLimit(int channel) const
{
    auto it = _limits.find(channel);
    if (it != _limits.end()) {
        return it->second;
    }
    if (channel >= 0 && channel < TOTAL_SERVOS) {
        const auto &cal = CHANNEL_MAP[channel];
        return {cal.name, channel, cal.lower, cal.upper, 0.0};
    }
    return {"unknown", channel, -M_PI, M_PI, 0.0};
}

const std::map<int, UrdfLimits::JointLimit> &UrdfLimits::getAllLimits() const
{
    return _limits;
}

bool UrdfLimits::hasCalibration(int channel) const
{
    return channel >= 0 && channel < TOTAL_SERVOS;
}

UrdfLimits::ChannelCalibration UrdfLimits::getCalibration(int channel) const
{
    if (channel >= 0 && channel < TOTAL_SERVOS) {
        return CHANNEL_MAP[channel];
    }
    return {"unknown", channel, 135, 1.0, (M_PI / 180.0), -M_PI, M_PI};
}

double UrdfLimits::clamp(int channel, double angleRad, double safetyMarginDeg) const
{
    if (channel < 0 || channel >= TOTAL_SERVOS) {
        return angleRad;
    }

    const auto &cal = CHANNEL_MAP[channel];
    double marginRad = (safetyMarginDeg * M_PI) / 180.0;
    double minLim = std::min(cal.lower, cal.upper) + marginRad;
    double maxLim = std::max(cal.lower, cal.upper) - marginRad;

    if (minLim > maxLim) {
        minLim = std::min(cal.lower, cal.upper);
        maxLim = std::max(cal.lower, cal.upper);
    }

    return std::max(minLim, std::min(maxLim, angleRad));
}

int UrdfLimits::angleToPwm(int channel, double angleRad, double safetyMarginDeg) const
{
    if (channel < 0 || channel >= TOTAL_SERVOS) {
        return 135;
    }

    const auto &cal = CHANNEL_MAP[channel];
    double marginRad = (safetyMarginDeg * M_PI) / 180.0;
    double minLim = std::min(cal.lower, cal.upper) + marginRad;
    double maxLim = std::max(cal.lower, cal.upper) - marginRad;
    if (minLim > maxLim) {
        minLim = std::min(cal.lower, cal.upper);
        maxLim = std::max(cal.lower, cal.upper);
    }

    double clampedAngle = std::max(minLim, std::min(maxLim, angleRad));
    int pwm = static_cast<int>(std::round(cal.center + (clampedAngle / (cal.radPerPwm * cal.sign))));
    return pwm;
}

double UrdfLimits::pwmToAngle(int channel, int pwm) const
{
    if (channel < 0 || channel >= TOTAL_SERVOS) {
        return 0.0;
    }

    const auto &cal = CHANNEL_MAP[channel];
    double rad = (static_cast<double>(pwm) - cal.center) * cal.radPerPwm * cal.sign;
    double minLim = std::min(cal.lower, cal.upper);
    double maxLim = std::max(cal.lower, cal.upper);
    return std::max(minLim, std::min(maxLim, rad));
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
