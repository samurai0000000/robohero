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
    // Hardcoded fallback limits from robohero.urdf specification
    _limits[0]  = {"left_ankle_roll",      0,  -0.4363,  1.3090, 0.0};
    _limits[1]  = {"left_ankle_pitch",     1,  -0.7854,  1.5708, 0.0};
    _limits[2]  = {"left_knee_pitch",      2,  -0.6981,  1.5708, 0.0};
    _limits[3]  = {"left_hip_pitch",       3,  -0.6981,  1.5708, 0.0};
    _limits[4]  = {"left_hip_roll",        4,  -0.3491,  1.6057, 0.0};
    _limits[5]  = {"left_shoulder_pitch",  5,  -1.8117,  1.3055, 0.0};
    _limits[6]  = {"left_shoulder_roll",   6,   0.0000,  4.7298, 0.0};
    _limits[7]  = {"left_elbow",           7,  -1.3090,  1.4835, 0.0};
    _limits[8]  = {"right_elbow",          8,  -1.4835,  0.9599, 0.0};
    _limits[9]  = {"right_shoulder_roll",  9,  -4.3284,  0.0000, 0.0};
    _limits[10] = {"right_shoulder_pitch", 10, -2.4906,  2.0054, 0.0};
    _limits[11] = {"right_hip_roll",       11, -1.7453,  0.3491, 0.0};
    _limits[12] = {"right_hip_pitch",      12, -0.6981,  1.5708, 0.0};
    _limits[13] = {"right_knee_pitch",     13, -0.6981,  1.5708, 0.0};
    _limits[14] = {"right_ankle_pitch",    14, -0.8727,  1.5708, 0.0};
    _limits[15] = {"right_ankle_roll",     15, -0.4363,  1.2217, 0.0};
    _limits[16] = {"head_yaw",             16, -0.9756,  0.9233, 0.0};
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
        {"left_ankle_roll", 0},
        {"left_ankle_pitch", 1},
        {"left_knee_pitch", 2},
        {"left_hip_pitch", 3},
        {"left_hip_roll", 4},
        {"left_shoulder_pitch", 5},
        {"left_shoulder_roll", 6},
        {"left_elbow", 7},
        {"right_elbow", 8},
        {"right_shoulder_roll", 9},
        {"right_shoulder_pitch", 10},
        {"right_hip_roll", 11},
        {"right_hip_pitch", 12},
        {"right_knee_pitch", 13},
        {"right_ankle_pitch", 14},
        {"right_ankle_roll", 15},
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
    return _limits.find(channel) != _limits.end();
}

UrdfLimits::JointLimit UrdfLimits::getLimit(int channel) const
{
    auto it = _limits.find(channel);
    if (it != _limits.end()) {
        return it->second;
    }
    return {"unknown", channel, -M_PI, M_PI, 0.0};
}

const std::map<int, UrdfLimits::JointLimit> &UrdfLimits::getAllLimits() const
{
    return _limits;
}

double UrdfLimits::clamp(int channel, double angleRad, double safetyMarginDeg) const
{
    auto it = _limits.find(channel);
    if (it == _limits.end()) {
        return angleRad;
    }

    double marginRad = (safetyMarginDeg * M_PI) / 180.0;
    double safeLower = it->second.lower + marginRad;
    double safeUpper = it->second.upper - marginRad;

    if (safeLower > safeUpper) {
        safeLower = it->second.lower;
        safeUpper = it->second.upper;
    }

    return std::max(safeLower, std::min(safeUpper, angleRad));
}

int UrdfLimits::angleToPwm(int channel, double angleRad, double safetyMarginDeg) const
{
    auto it = _limits.find(channel);
    if (it == _limits.end()) {
        return (SERVOMIN + SERVOMAX) / 2;
    }

    double safeAngle = clamp(channel, angleRad, safetyMarginDeg);
    double lower = it->second.lower;
    double upper = it->second.upper;

    if (std::abs(upper - lower) < 1e-6) {
        return (SERVOMIN + SERVOMAX) / 2;
    }

    double fraction = (safeAngle - lower) / (upper - lower);
    fraction = std::max(0.0, std::min(1.0, fraction));

    int pwm = static_cast<int>(std::round(SERVOMIN + fraction * (SERVOMAX - SERVOMIN)));
    return std::max(SERVOMIN, std::min(SERVOMAX, pwm));
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
