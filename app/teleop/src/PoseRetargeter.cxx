/*
 * PoseRetargeter.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "PoseRetargeter.hxx"
#include "UrdfLimits.hxx"
#include <Eigen/Dense>
#include <cmath>
#include <algorithm>

PoseRetargeter::PoseRetargeter()
    : _hasPrevPose(false)
{
    _prevAngles.fill(0.0);
}

PoseRetargeter::~PoseRetargeter()
{
}

void PoseRetargeter::reset()
{
    _hasPrevPose = false;
    _prevAngles.fill(0.0);
}

double PoseRetargeter::computeAngle2D(float ax, float ay,
                                      float bx, float by,
                                      float cx, float cy)
{
    Eigen::Vector2f v1(ax - bx, ay - by);
    Eigen::Vector2f v2(cx - bx, cy - by);

    float norm1 = v1.norm();
    float norm2 = v2.norm();

    if (norm1 < 1e-4f || norm2 < 1e-4f) {
        return 0.0;
    }

    float dot = v1.dot(v2) / (norm1 * norm2);
    dot = std::clamp(dot, -1.0f, 1.0f);

    return std::acos(dot);
}

PoseRetargeter::RetargetResult PoseRetargeter::process(
    const PoseEstimator::PersonPose &pose,
    bool mirrorMode,
    float smoothingAlpha,
    float deadbandDeg,
    float safetyMarginDeg)
{
    RetargetResult res;
    res.valid = false;
    res.jointAnglesRad.fill(0.0);
    for (int i = 0; i < 17; ++i) {
        res.servoPwm[i] = UrdfLimits::instance().getCalibration(i).center;
    }

    if (!pose.valid) {
        return res;
    }

    // Keypoint mapping based on mirrorMode
    int kptLShoulder = mirrorMode ? PoseEstimator::LEFT_SHOULDER : PoseEstimator::RIGHT_SHOULDER;
    int kptLElbow    = mirrorMode ? PoseEstimator::LEFT_ELBOW    : PoseEstimator::RIGHT_ELBOW;
    int kptLWrist    = mirrorMode ? PoseEstimator::LEFT_WRIST    : PoseEstimator::RIGHT_WRIST;
    int kptLHip      = mirrorMode ? PoseEstimator::LEFT_HIP      : PoseEstimator::RIGHT_HIP;
    int kptLKnee     = mirrorMode ? PoseEstimator::LEFT_KNEE     : PoseEstimator::RIGHT_KNEE;
    int kptLAnkle    = mirrorMode ? PoseEstimator::LEFT_ANKLE    : PoseEstimator::RIGHT_ANKLE;

    int kptRShoulder = mirrorMode ? PoseEstimator::RIGHT_SHOULDER : PoseEstimator::LEFT_SHOULDER;
    int kptRElbow    = mirrorMode ? PoseEstimator::RIGHT_ELBOW    : PoseEstimator::LEFT_ELBOW;
    int kptRWrist    = mirrorMode ? PoseEstimator::RIGHT_WRIST    : PoseEstimator::LEFT_WRIST;
    int kptRHip      = mirrorMode ? PoseEstimator::RIGHT_HIP      : PoseEstimator::LEFT_HIP;
    int kptRKnee     = mirrorMode ? PoseEstimator::RIGHT_KNEE     : PoseEstimator::LEFT_KNEE;
    int kptRAnkle    = mirrorMode ? PoseEstimator::RIGHT_ANKLE    : PoseEstimator::LEFT_ANKLE;

    const auto &kpts = pose.keypoints;
    std::array<double, 17> rawAngles;
    rawAngles.fill(0.0);

    // 1. Head Yaw (Channel 16)
    if (kpts[PoseEstimator::NOSE].conf > 0.3f &&
        kpts[PoseEstimator::LEFT_EAR].conf > 0.3f &&
        kpts[PoseEstimator::RIGHT_EAR].conf > 0.3f) {
        float earMidX = (kpts[PoseEstimator::LEFT_EAR].x + kpts[PoseEstimator::RIGHT_EAR].x) * 0.5f;
        float earDist = std::abs(kpts[PoseEstimator::LEFT_EAR].x - kpts[PoseEstimator::RIGHT_EAR].x) + 1e-4f;
        float noseOffset = (kpts[PoseEstimator::NOSE].x - earMidX) / earDist;
        if (!mirrorMode) {
            noseOffset = -noseOffset;
        }
        rawAngles[16] = -noseOffset * 1.2; // Scale factor for head yaw
    }

    // 2. Left Arm: Shoulder Pitch (5), Shoulder Roll (6), Elbow (7)
    if (kpts[kptLShoulder].conf > 0.35f && kpts[kptLElbow].conf > 0.35f) {
        float sx = kpts[kptLShoulder].x;
        float sy = kpts[kptLShoulder].y;
        float ex = kpts[kptLElbow].x;
        float ey = kpts[kptLElbow].y;
        float hx = kpts[kptLHip].x;
        float hy = kpts[kptLHip].y;

        // Shoulder Roll: angle between torso vector (S -> H) and arm vector (S -> E)
        if (kpts[kptLHip].conf > 0.3f) {
            double rollAngle = computeAngle2D(hx, hy, sx, sy, ex, ey);
            rawAngles[6] = rollAngle;
        }

        // Left Elbow: angle at elbow (S - E - W)
        if (kpts[kptLWrist].conf > 0.35f) {
            float wx = kpts[kptLWrist].x;
            float wy = kpts[kptLWrist].y;
            double elbowAngle = computeAngle2D(sx, sy, ex, ey, wx, wy);
            // Inverted so 0 rad is arm extended (180 deg)
            rawAngles[7] = (M_PI - elbowAngle);
        }

        // Left Shoulder Pitch: vertical elevation
        double armElevation = std::atan2(ey - sy, ex - sx);
        rawAngles[5] = armElevation - M_PI_2;
    }

    // 3. Right Arm: Shoulder Roll (9), Shoulder Pitch (10), Elbow (8)
    if (kpts[kptRShoulder].conf > 0.35f && kpts[kptRElbow].conf > 0.35f) {
        float sx = kpts[kptRShoulder].x;
        float sy = kpts[kptRShoulder].y;
        float ex = kpts[kptRElbow].x;
        float ey = kpts[kptRElbow].y;
        float hx = kpts[kptRHip].x;
        float hy = kpts[kptRHip].y;

        // Shoulder Roll: angle between torso vector (S -> H) and arm vector (S -> E)
        if (kpts[kptRHip].conf > 0.3f) {
            double rollAngle = computeAngle2D(hx, hy, sx, sy, ex, ey);
            rawAngles[9] = -rollAngle;
        }

        // Right Elbow: angle at elbow (S - E - W)
        if (kpts[kptRWrist].conf > 0.35f) {
            float wx = kpts[kptRWrist].x;
            float wy = kpts[kptRWrist].y;
            double elbowAngle = computeAngle2D(sx, sy, ex, ey, wx, wy);
            rawAngles[8] = -(M_PI - elbowAngle);
        }

        // Right Shoulder Pitch
        double armElevation = std::atan2(ey - sy, sx - ex);
        rawAngles[10] = armElevation - M_PI_2;
    }

    // 4. Legs (Channels 2, 3: Left Knee/Hip, 12, 13: Right Knee/Hip)
    if (kpts[kptLHip].conf > 0.4f && kpts[kptLKnee].conf > 0.4f) {
        if (kpts[kptLAnkle].conf > 0.4f) {
            double kneeAngle = computeAngle2D(kpts[kptLHip].x, kpts[kptLHip].y,
                                              kpts[kptLKnee].x, kpts[kptLKnee].y,
                                              kpts[kptLAnkle].x, kpts[kptLAnkle].y);
            rawAngles[2] = M_PI - kneeAngle;
        }
    }

    if (kpts[kptRHip].conf > 0.4f && kpts[kptRKnee].conf > 0.4f) {
        if (kpts[kptRAnkle].conf > 0.4f) {
            double kneeAngle = computeAngle2D(kpts[kptRHip].x, kpts[kptRHip].y,
                                              kpts[kptRKnee].x, kpts[kptRKnee].y,
                                              kpts[kptRAnkle].x, kpts[kptRAnkle].y);
            rawAngles[13] = M_PI - kneeAngle;
        }
    }

    // Smoothing, Deadband, Clamping, and PWM calculation
    double deadbandRad = (deadbandDeg * M_PI) / 180.0;
    const auto &urdf = UrdfLimits::instance();

    for (int ch = 0; ch < 17; ++ch) {
        double target = rawAngles[ch];

        if (_hasPrevPose) {
            double diff = std::abs(target - _prevAngles[ch]);
            if (diff < deadbandRad) {
                target = _prevAngles[ch];
            } else {
                target = smoothingAlpha * target + (1.0 - smoothingAlpha) * _prevAngles[ch];
            }
        }

        // Clamp to URDF physical limits with safety margin
        double clamped = urdf.clamp(ch, target, safetyMarginDeg);
        res.jointAnglesRad[ch] = clamped;
        res.servoPwm[ch] = urdf.angleToPwm(ch, clamped, safetyMarginDeg);
        _prevAngles[ch] = clamped;
    }

    _hasPrevPose = true;
    res.valid = true;
    return res;
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
