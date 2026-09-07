/*
 * ParametricGait.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_PARAMETRIC_GAIT_HXX
#define ROBOHERO_MOTION_PARAMETRIC_GAIT_HXX

#include <string>
#include <array>
#include "MotionSequence.hxx"

class ParametricGait
{
public:
    struct GaitParameters
    {
        std::string mode = "forward"; // "forward", "backward", "turn_left", "turn_right", "sidestep_left", "sidestep_right"
        double strideLengthMm = 40.0;
        double stepHeightMm = 25.0;
        double swayAmplitudeMm = 18.0;
        double turnAngleDeg = 10.0;
        int stepDurationMs = 600;
        double doubleSupportRatio = 0.20;
        double torsoPitchDeg = 3.0;
        double armSwingDeg = 20.0;
        double elbowFlexDeg = 25.0;
        std::string armMode = "natural_swing"; // "natural_swing", "guard", "held"
        int cycleCount = 2; // Number of complete 2-step cycles
    };

    static MotionSequence generateGait(const GaitParameters &params);

    static MotionSequence generateGesture(const std::string &gestureType);

    // Analytical 5-DOF Leg Inverse Kinematics
    // Inputs: Foot sole target (x, y, z) in meters relative to hip joint, target roll and pitch
    // Outputs: outAngles[0]=ankle_roll, [1]=ankle_pitch, [2]=knee_pitch, [3]=hip_pitch, [4]=hip_roll
    static bool solveLegIk(bool isLeftLeg, double x, double y, double z,
                           double rollRad, double pitchRad,
                           std::array<double, 5> &outAngles);

    // Analytical 3-DOF Arm Inverse Kinematics
    // Inputs: Hand target (x, y, z) in meters relative to shoulder
    // Outputs: outAngles[0]=shoulder_pitch, [1]=shoulder_roll, [2]=elbow
    static bool solveArmIk(bool isLeftArm, double x, double y, double z,
                           std::array<double, 3> &outAngles);

    // Link dimensions (meters) matching robohero.urdf
    static constexpr double THIGH_LENGTH = 0.048;  // Hip to Knee
    static constexpr double SHIN_LENGTH  = 0.048;  // Knee to Ankle
    static constexpr double ANKLE_HEIGHT = 0.024;  // Ankle to Foot Sole
    static constexpr double UPPER_ARM_LENGTH = 0.044; // Shoulder to Elbow
    static constexpr double FOREARM_LENGTH   = 0.044; // Elbow to Hand
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
