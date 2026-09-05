/*
 * UrdfLimits.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "UrdfLimits.hxx"
#include <QFile>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
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
    _calibrations.clear();
    _limits.clear();
    for (int i = 0; i < TOTAL_SERVOS; ++i) {
        ChannelCalibration cal = {
            "joint_" + std::to_string(i),
            i,
            135,
            1.0,
            (M_PI / 180.0),
            -M_PI,
            M_PI
        };
        _calibrations[i] = cal;
        _limits[i] = {cal.name, i, cal.lower, cal.upper, 0.0};
    }
}

bool UrdfLimits::loadCalibrationJson(const std::string &calibrationResourcePath)
{
    QFile file(QString::fromStdString(calibrationResourcePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFile fallbackFile(":/model/calibration.json");
        if (!fallbackFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << "UrdfLimits: could not open calibration JSON: "
                      << calibrationResourcePath << std::endl;
            return false;
        }
        file.setFileName(":/model/calibration.json");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        std::cerr << "UrdfLimits: JSON parse error in " << calibrationResourcePath
                  << ": " << parseError.errorString().toStdString() << std::endl;
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject channels = root.value("channels").toObject();
    if (channels.isEmpty()) {
        std::cerr << "UrdfLimits: missing 'channels' in " << calibrationResourcePath << std::endl;
        return false;
    }

    for (int i = 0; i < TOTAL_SERVOS; ++i) {
        QString key = QString::number(i);
        if (!channels.contains(key)) {
            continue;
        }

        QJsonObject chObj = channels.value(key).toObject();
        std::string name = chObj.value("name").toString().toStdString();
        int center = chObj.value("center").toInt(135);
        double sign = chObj.value("sign").toDouble(1.0);
        double radPerPwm = chObj.value("rad_per_pwm").toDouble(M_PI / 180.0);

        QJsonObject urdfLimits = chObj.value("urdf_limits").toObject();
        double lower = urdfLimits.value("lower").toDouble(-M_PI);
        double upper = urdfLimits.value("upper").toDouble(M_PI);

        ChannelCalibration cal = {
            name,
            i,
            center,
            sign,
            radPerPwm,
            lower,
            upper
        };
        _calibrations[i] = cal;
        _limits[i] = {name, i, lower, upper, 0.0};
    }

    return true;
}

bool UrdfLimits::init(const std::string &urdfResourcePath,
                      const std::string &calibrationResourcePath)
{
    populateFallbackLimits();

    // 1. Load canonical calibration parameters from model/calibration.json
    bool calibOk = loadCalibrationJson(calibrationResourcePath);
    if (!calibOk) {
        std::cerr << "UrdfLimits: warning, using fallback calibration." << std::endl;
    }

    // 2. Parse URDF XML to verify or override exact URDF limits
    QFile file(QString::fromStdString(urdfResourcePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.setFileName(":/model/robohero.urdf");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            file.setFileName(":/urdf/robohero.urdf");
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                std::cerr << "UrdfLimits: could not open URDF: " << urdfResourcePath << std::endl;
                _initialized = true;
                return calibOk;
            }
        }
    }

    QByteArray xmlData = file.readAll();
    file.close();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xmlData.constData(), xmlData.size());
    if (!result) {
        std::cerr << "UrdfLimits: XML parse failed: " << result.description() << std::endl;
        _initialized = true;
        return calibOk;
    }

    pugi::xml_node robot = doc.child("robot");
    if (!robot) {
        std::cerr << "UrdfLimits: missing <robot> tag in URDF." << std::endl;
        _initialized = true;
        return calibOk;
    }

    // Map joint names to channels from loaded calibrations
    std::map<std::string, int> nameToChannel;
    for (const auto &pair : _calibrations) {
        nameToChannel[pair.second.name] = pair.first;
    }

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
                _calibrations[ch].lower = lower;
                _calibrations[ch].upper = upper;
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
    return {"unknown", channel, -M_PI, M_PI, 0.0};
}

const std::map<int, UrdfLimits::JointLimit> &UrdfLimits::getAllLimits() const
{
    return _limits;
}

bool UrdfLimits::hasCalibration(int channel) const
{
    return _calibrations.find(channel) != _calibrations.end();
}

UrdfLimits::ChannelCalibration UrdfLimits::getCalibration(int channel) const
{
    auto it = _calibrations.find(channel);
    if (it != _calibrations.end()) {
        return it->second;
    }
    return {"unknown", channel, 135, 1.0, (M_PI / 180.0), -M_PI, M_PI};
}

const std::map<int, UrdfLimits::ChannelCalibration> &UrdfLimits::getAllCalibrations() const
{
    return _calibrations;
}

double UrdfLimits::clamp(int channel, double angleRad, double safetyMarginDeg) const
{
    if (channel < 0 || channel >= TOTAL_SERVOS) {
        return angleRad;
    }

    const auto cal = getCalibration(channel);
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

    const auto cal = getCalibration(channel);
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

    const auto cal = getCalibration(channel);
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
