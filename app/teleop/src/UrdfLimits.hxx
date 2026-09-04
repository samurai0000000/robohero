/*
 * UrdfLimits.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_URDF_LIMITS_HXX
#define ROBOHERO_URDF_LIMITS_HXX

#include <map>
#include <string>

class UrdfLimits
{
public:
    struct JointLimit
    {
        std::string name;
        int channel;
        double lower; // radians
        double upper; // radians
        double defaultAngle;
    };

    static UrdfLimits &instance();

    bool init(const std::string &urdfResourcePath = ":/urdf/robohero.urdf");

    bool hasLimit(int channel) const;
    JointLimit getLimit(int channel) const;
    const std::map<int, JointLimit> &getAllLimits() const;

    double clamp(int channel, double angleRad, double safetyMarginDeg = 0.0) const;
    int angleToPwm(int channel, double angleRad, double safetyMarginDeg = 0.0) const;

    static constexpr int SERVOMIN = 104;
    static constexpr int SERVOMAX = 512;
    static constexpr int TOTAL_SERVOS = 17;

private:
    UrdfLimits();
    UrdfLimits(const UrdfLimits &) = delete;
    UrdfLimits &operator=(const UrdfLimits &) = delete;

    void populateFallbackLimits();

    static UrdfLimits _self;
    std::map<int, JointLimit> _limits;
    bool _initialized;
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
