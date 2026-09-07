/*
 * MotionSequence.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_SEQUENCE_HXX
#define ROBOHERO_MOTION_SEQUENCE_HXX

#include <string>
#include <vector>
#include <array>
#include <QJsonObject>
#include <QJsonDocument>

class MotionSequence
{
public:
    struct Keyframe
    {
        int timeMs;
        std::string easing; // "linear", "cubic_in_out", "cubic_in", "cubic_out"
        std::array<int, 17> pwm;
        std::array<double, 17> angles;
    };

    MotionSequence();
    ~MotionSequence();

    void clear();

    const std::string &name() const { return _name; }
    void setName(const std::string &name) { _name = name; }

    const std::string &description() const { return _description; }
    void setDescription(const std::string &desc) { _description = desc; }

    int durationMs() const { return _durationMs; }
    void setDurationMs(int ms) { _durationMs = ms; }

    bool isLoop() const { return _loop; }
    void setLoop(bool loop) { _loop = loop; }

    int fps() const { return _fps; }
    void setFps(int fps) { _fps = fps; }

    const std::string &generatorType() const { return _generatorType; }
    void setGeneratorType(const std::string &type) { _generatorType = type; }

    const QJsonObject &generatorParams() const { return _generatorParams; }
    void setGeneratorParams(const QJsonObject &params) { _generatorParams = params; }

    const std::vector<Keyframe> &keyframes() const { return _keyframes; }
    int keyframeCount() const { return static_cast<int>(_keyframes.size()); }

    int addKeyframe(int timeMs, const std::array<int, 17> &pwm,
                    const std::string &easing = "cubic_in_out");
    int addKeyframe(int timeMs, const std::array<double, 17> &angles,
                    const std::string &easing = "cubic_in_out");
    bool removeKeyframe(int index);
    void sortKeyframes();

    bool evaluate(int timeMs, std::array<double, 17> &outAngles,
                  std::array<int, 17> &outPwm) const;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &obj);

    bool saveToFile(const std::string &filePath) const;
    bool loadFromFile(const std::string &filePath);

    static MotionSequence createDefaultStandby();

private:
    std::string _name;
    std::string _description;
    int _durationMs;
    bool _loop;
    int _fps;
    std::string _generatorType;
    QJsonObject _generatorParams;
    std::vector<Keyframe> _keyframes;

    static double interpolateHermite(double p0, double p1, double m0, double m1, double t);
    static double applyEasing(double t, const std::string &easing);
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
