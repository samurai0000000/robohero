/*
 * MotionSequence.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MotionSequence.hxx"
#include "UrdfLimits.hxx"
#include <QFile>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <iostream>

MotionSequence::MotionSequence()
    : _name("New Motion")
    , _description("")
    , _durationMs(1000)
    , _loop(false)
    , _fps(50)
    , _generatorType("manual")
{
}

MotionSequence::~MotionSequence()
{
}

void MotionSequence::clear()
{
    _name = "New Motion";
    _description.clear();
    _durationMs = 1000;
    _loop = false;
    _fps = 50;
    _generatorType = "manual";
    _generatorParams = QJsonObject();
    _keyframes.clear();
}

int MotionSequence::addKeyframe(int timeMs, const std::array<int, 17> &pwm,
                                const std::string &easing)
{
    Keyframe kf;
    kf.timeMs = timeMs;
    kf.easing = easing;
    kf.pwm = pwm;
    for (int ch = 0; ch < 17; ++ch) {
        kf.angles[ch] = UrdfLimits::instance().pwmToAngle(ch, pwm[ch]);
    }
    _keyframes.push_back(kf);
    sortKeyframes();
    if (timeMs > _durationMs) {
        _durationMs = timeMs;
    }
    return static_cast<int>(_keyframes.size()) - 1;
}

int MotionSequence::addKeyframe(int timeMs, const std::array<double, 17> &angles,
                                const std::string &easing)
{
    Keyframe kf;
    kf.timeMs = timeMs;
    kf.easing = easing;
    kf.angles = angles;
    for (int ch = 0; ch < 17; ++ch) {
        kf.pwm[ch] = UrdfLimits::instance().angleToPwm(ch, angles[ch]);
    }
    _keyframes.push_back(kf);
    sortKeyframes();
    if (timeMs > _durationMs) {
        _durationMs = timeMs;
    }
    return static_cast<int>(_keyframes.size()) - 1;
}

bool MotionSequence::removeKeyframe(int index)
{
    if (index >= 0 && index < static_cast<int>(_keyframes.size())) {
        _keyframes.erase(_keyframes.begin() + index);
        return true;
    }
    return false;
}

void MotionSequence::sortKeyframes()
{
    std::sort(_keyframes.begin(), _keyframes.end(),
              [](const Keyframe &a, const Keyframe &b) {
                  return a.timeMs < b.timeMs;
              });
}

double MotionSequence::applyEasing(double t, const std::string &easing)
{
    t = std::clamp(t, 0.0, 1.0);
    if (easing == "linear") {
        return t;
    } else if (easing == "cubic_in") {
        return t * t * t;
    } else if (easing == "cubic_out") {
        double p = 1.0 - t;
        return 1.0 - (p * p * p);
    } else { // "cubic_in_out" default
        if (t < 0.5) {
            return 4.0 * t * t * t;
        } else {
            double p = -2.0 * t + 2.0;
            return 1.0 - (p * p * p) / 2.0;
        }
    }
}

bool MotionSequence::evaluate(int timeMs, std::array<double, 17> &outAngles,
                              std::array<int, 17> &outPwm) const
{
    if (_keyframes.empty()) {
        for (int ch = 0; ch < 17; ++ch) {
            auto cal = UrdfLimits::instance().getCalibration(ch);
            outPwm[ch] = cal.center;
            outAngles[ch] = 0.0;
        }
        return false;
    }

    if (_keyframes.size() == 1) {
        outAngles = _keyframes[0].angles;
        outPwm = _keyframes[0].pwm;
        return true;
    }

    int t = timeMs;
    if (_loop && _durationMs > 0) {
        t = t % _durationMs;
    }

    if (t <= _keyframes.front().timeMs) {
        outAngles = _keyframes.front().angles;
        outPwm = _keyframes.front().pwm;
        return true;
    }

    if (t >= _keyframes.back().timeMs) {
        outAngles = _keyframes.back().angles;
        outPwm = _keyframes.back().pwm;
        return true;
    }

    // Find bounding keyframes [i, i+1]
    size_t idx = 0;
    for (size_t i = 0; i < _keyframes.size() - 1; ++i) {
        if (t >= _keyframes[i].timeMs && t <= _keyframes[i + 1].timeMs) {
            idx = i;
            break;
        }
    }

    const Keyframe &kf0 = _keyframes[idx];
    const Keyframe &kf1 = _keyframes[idx + 1];

    double dt = static_cast<double>(kf1.timeMs - kf0.timeMs);
    double u = (dt > 0.0) ? (static_cast<double>(t - kf0.timeMs) / dt) : 0.0;
    double easedU = applyEasing(u, kf1.easing.empty() ? "cubic_in_out" : kf1.easing);

    for (int ch = 0; ch < 17; ++ch) {
        double angle = kf0.angles[ch] + (kf1.angles[ch] - kf0.angles[ch]) * easedU;
        angle = UrdfLimits::instance().clamp(ch, angle);
        outAngles[ch] = angle;
        outPwm[ch] = UrdfLimits::instance().angleToPwm(ch, angle);
    }

    return true;
}

QJsonObject MotionSequence::toJson() const
{
    QJsonObject root;
    root["version"] = "1.0";
    root["name"] = QString::fromStdString(_name);
    root["description"] = QString::fromStdString(_description);
    root["duration_ms"] = _durationMs;
    root["loop"] = _loop;
    root["fps"] = _fps;

    QJsonObject genObj;
    genObj["type"] = QString::fromStdString(_generatorType);
    genObj["parameters"] = _generatorParams;
    root["generator"] = genObj;

    QJsonArray kfArray;
    for (const auto &kf : _keyframes) {
        QJsonObject kfObj;
        kfObj["time_ms"] = kf.timeMs;
        kfObj["easing"] = QString::fromStdString(kf.easing);

        QJsonObject chObj;
        for (int ch = 0; ch < 17; ++ch) {
            chObj[QString::number(ch)] = kf.pwm[ch];
        }
        kfObj["channels"] = chObj;
        kfArray.append(kfObj);
    }
    root["keyframes"] = kfArray;

    return root;
}

bool MotionSequence::fromJson(const QJsonObject &root)
{
    _name = root.value("name").toString("New Motion").toStdString();
    _description = root.value("description").toString("").toStdString();
    _durationMs = root.value("duration_ms").toInt(1000);
    _loop = root.value("loop").toBool(false);
    _fps = root.value("fps").toInt(50);

    QJsonObject genObj = root.value("generator").toObject();
    _generatorType = genObj.value("type").toString("manual").toStdString();
    _generatorParams = genObj.value("parameters").toObject();

    _keyframes.clear();
    QJsonArray kfArray = root.value("keyframes").toArray();
    for (int i = 0; i < kfArray.size(); ++i) {
        QJsonObject kfObj = kfArray[i].toObject();
        Keyframe kf;
        kf.timeMs = kfObj.value("time_ms").toInt(0);
        kf.easing = kfObj.value("easing").toString("cubic_in_out").toStdString();

        QJsonObject chObj = kfObj.value("channels").toObject();
        for (int ch = 0; ch < 17; ++ch) {
            QString key = QString::number(ch);
            int defCenter = UrdfLimits::instance().getCalibration(ch).center;
            kf.pwm[ch] = chObj.value(key).toInt(defCenter);
            kf.angles[ch] = UrdfLimits::instance().pwmToAngle(ch, kf.pwm[ch]);
        }
        _keyframes.push_back(kf);
    }

    sortKeyframes();
    return true;
}

bool MotionSequence::saveToFile(const std::string &filePath) const
{
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool MotionSequence::loadFromFile(const std::string &filePath)
{
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    return fromJson(doc.object());
}

MotionSequence MotionSequence::createDefaultStandby()
{
    MotionSequence seq;
    seq.setName("Standby Neutral");
    seq.setDescription("Canonical neutral upright posture with balanced support");
    seq.setDurationMs(1000);
    seq.setLoop(false);

    std::array<int, 17> centerPwm{};
    for (int ch = 0; ch < 17; ++ch) {
        centerPwm[ch] = UrdfLimits::instance().getCalibration(ch).center;
    }

    seq.addKeyframe(0, centerPwm, "linear");
    seq.addKeyframe(1000, centerPwm, "linear");
    return seq;
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
