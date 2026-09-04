/*
 * PoseRetargeter.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_POSE_RETARGETER_HXX
#define ROBOHERO_POSE_RETARGETER_HXX

#include "PoseEstimator.hxx"
#include <array>

class PoseRetargeter
{
public:
    struct RetargetResult
    {
        std::array<double, 17> jointAnglesRad;
        std::array<int, 17> servoPwm;
        bool valid = false;
    };

    PoseRetargeter();
    ~PoseRetargeter();

    void reset();

    RetargetResult process(const PoseEstimator::PersonPose &pose,
                           bool mirrorMode,
                           float smoothingAlpha,
                           float deadbandDeg,
                           float safetyMarginDeg);

private:
    std::array<double, 17> _prevAngles;
    bool _hasPrevPose;

    double computeAngle2D(float ax, float ay,
                          float bx, float by,
                          float cx, float cy);
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
