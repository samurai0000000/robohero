/*
 * ParametricGait.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "ParametricGait.hxx"
#include "UrdfLimits.hxx"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline double clampVal(double val, double minVal, double maxVal)
{
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

}

bool ParametricGait::solveLegIk(bool isLeftLeg, double x, double y, double z,
                                double rollRad, double pitchRad,
                                std::array<double, 5> &outAngles)
{
    // Link dimensions matching URDF
    constexpr double L1 = THIGH_LENGTH; // 0.048 m
    constexpr double L2 = SHIN_LENGTH;  // 0.048 m
    constexpr double H_ankle = ANKLE_HEIGHT; // 0.024 m

    // Ankle joint center offset from foot sole
    double xa = x - (-H_ankle * std::sin(pitchRad));
    double za = z - (-H_ankle * std::cos(pitchRad) * std::cos(rollRad));
    double ya = y - (-H_ankle * std::sin(rollRad));

    // Lateral Hip Roll angle to shift leg in Y-Z plane
    double hipRoll = std::atan2(ya, -za);

    // Keep foot sole parallel to target roll orientation:
    // q_ankle_roll + q_hip_roll = rollRad  ==>  q_ankle_roll = rollRad - q_hip_roll
    double ankleRoll = rollRad - hipRoll;

    // Projected length in the tilted sagittal plane
    double zPrime = -std::sqrt(ya * ya + za * za);
    double xPrime = xa;

    // Planar 2-link leg extension D from hip pitch to ankle pitch
    double D = std::sqrt(xPrime * xPrime + zPrime * zPrime);
    double maxReach = (L1 + L2) * 0.9999;
    double minReach = std::abs(L1 - L2) + 0.002;
    D = clampVal(D, minReach, maxReach);

    // Knee pitch via Law of Cosines
    // cos(alpha) = (L1^2 + L2^2 - D^2) / (2 * L1 * L2)
    double cosAlpha = (L1 * L1 + L2 * L2 - D * D) / (2.0 * L1 * L2);
    cosAlpha = clampVal(cosAlpha, -1.0, 1.0);
    double alpha = std::acos(cosAlpha);
    // Knee flexion angle: 0 at straight leg, bends backward (positive in URDF)
    double kneePitch = M_PI - alpha;

    // Angle of leg vector in sagittal plane from vertical
    double gamma = std::atan2(xPrime, -zPrime);

    // Angle between thigh and leg vector D:
    // cos(beta) = (L1^2 + D^2 - L2^2) / (2 * L1 * D)
    double cosBeta = (L1 * L1 + D * D - L2 * L2) / (2.0 * L1 * D);
    cosBeta = clampVal(cosBeta, -1.0, 1.0);
    double beta = std::acos(cosBeta);

    // In URDF, both hip_pitch_joint and knee_pitch_joint have axis [0, 1, 0].
    // By right-hand rule about +Y (left):
    // A vector in -Z rotating by +theta moves toward -X (backward).
    // A vector in -Z rotating by -theta moves toward +X (forward).
    // Knee flexion bends backward (toward -X), so kneePitch must be POSITIVE (>= 0).
    // Forward hip reach (foot at +X) requires thigh to tilt forward, so hipPitch must be NEGATIVE!
    double hipPitch = -(gamma + beta);

    // Foot pitch orientation:
    // hipPitch + kneePitch + anklePitch = pitchRad
    // => anklePitch = pitchRad - hipPitch - kneePitch
    double anklePitch = pitchRad - hipPitch - kneePitch;

    // UrdfLimits clamping
    int chAnkleRoll  = isLeftLeg ? 0  : 15;
    int chAnklePitch = isLeftLeg ? 1  : 14;
    int chKneePitch  = isLeftLeg ? 2  : 13;
    int chHipPitch   = isLeftLeg ? 3  : 12;
    int chHipRoll    = isLeftLeg ? 4  : 11;

    const auto &limits = UrdfLimits::instance();
    outAngles[0] = limits.clamp(chAnkleRoll, ankleRoll);
    outAngles[1] = limits.clamp(chAnklePitch, anklePitch);
    outAngles[2] = limits.clamp(chKneePitch, std::max(0.0, kneePitch));
    outAngles[3] = limits.clamp(chHipPitch, hipPitch);
    outAngles[4] = limits.clamp(chHipRoll, hipRoll);

    return true;
}

bool ParametricGait::solveArmIk(bool isLeftArm, double x, double y, double z,
                                std::array<double, 3> &outAngles)
{
    constexpr double L_upper = UPPER_ARM_LENGTH; // 0.044 m
    constexpr double L_fore  = FOREARM_LENGTH;   // 0.044 m

    double R = std::sqrt(x * x + y * y + z * z);
    double maxReach = (L_upper + L_fore) * 0.999;
    double minReach = 0.010;
    R = clampVal(R, minReach, maxReach);

    // Elbow flexion via Law of Cosines
    double cosElbow = (L_upper * L_upper + L_fore * L_fore - R * R) / (2.0 * L_upper * L_fore);
    cosElbow = clampVal(cosElbow, -1.0, 1.0);
    double elbowAngle = M_PI - std::acos(cosElbow);
    double signedElbow = isLeftArm ? -elbowAngle : elbowAngle;

    // Shoulder Pitch and Roll
    double pitchAngle = std::atan2(x, -z);
    double rollAngle = isLeftArm ? std::atan2(y, std::sqrt(x * x + z * z))
                                 : std::atan2(-y, std::sqrt(x * x + z * z));

    int chPitch = isLeftArm ? 5 : 10;
    int chRoll  = isLeftArm ? 6 : 9;
    int chElbow = isLeftArm ? 7 : 8;

    const auto &limits = UrdfLimits::instance();
    outAngles[0] = limits.clamp(chPitch, pitchAngle);
    outAngles[1] = limits.clamp(chRoll, rollAngle);
    outAngles[2] = limits.clamp(chElbow, signedElbow);

    return true;
}

MotionSequence ParametricGait::generateGait(const GaitParameters &params)
{
    MotionSequence seq;
    seq.setName("Parametric Biped Gait");
    seq.setDescription("Synthesized 5-DOF bipedal walking gait with analytical IK and sway balancing");
    seq.setGeneratorType("parametric_biped");
    seq.setLoop(true);
    seq.setFps(50);

    QJsonObject pObj;
    pObj["mode"] = QString::fromStdString(params.mode);
    pObj["stride_length_mm"] = params.strideLengthMm;
    pObj["step_height_mm"] = params.stepHeightMm;
    pObj["sway_amplitude_mm"] = params.swayAmplitudeMm;
    pObj["turn_angle_deg"] = params.turnAngleDeg;
    pObj["step_duration_ms"] = params.stepDurationMs;
    pObj["double_support_ratio"] = params.doubleSupportRatio;
    pObj["torso_pitch_deg"] = params.torsoPitchDeg;
    pObj["arm_swing_deg"] = params.armSwingDeg;
    pObj["cycle_count"] = params.cycleCount;
    seq.setGeneratorParams(pObj);

    // Nominal leg length under slight default knee flexion for shock absorption
    constexpr double nominalZ = -(THIGH_LENGTH + SHIN_LENGTH + ANKLE_HEIGHT - 0.002); // ~ -0.118 m
    double strideM = (params.strideLengthMm / 1000.0);
    double stepHeightM = (params.stepHeightMm / 1000.0);
    double swayM = (params.swayAmplitudeMm / 1000.0);
    double torsoPitchRad = params.torsoPitchDeg * (M_PI / 180.0);
    double armSwingRad = params.armSwingDeg * (M_PI / 180.0);
    double elbowRad = params.elbowFlexDeg * (M_PI / 180.0);
    double turnRad = params.turnAngleDeg * (M_PI / 180.0);

    if (params.mode == "backward") {
        strideM = -strideM;
    }

    int stepMs = std::max(200, params.stepDurationMs);
    int cycleMs = 2 * stepMs;
    int totalMs = std::max(1, params.cycleCount) * cycleMs;
    seq.setDurationMs(totalMs);

    // Sample keyframes at 40ms intervals (25 keyframes/sec) for smooth interpolation
    int sampleIntervalMs = 40;
    int numSamples = totalMs / sampleIntervalMs;

    for (int i = 0; i <= numSamples; ++i) {
        int t = i * sampleIntervalMs;
        if (t > totalMs) t = totalMs;

        double normCycle = std::fmod(static_cast<double>(t), static_cast<double>(cycleMs)) / static_cast<double>(cycleMs);
        double phaseRad = 2.0 * M_PI * normCycle;

        // Pelvis sway: shifts laterally over the stance foot
        // When phaseRad in [0, pi]: sway toward Left (+Y)
        // When phaseRad in [pi, 2pi]: sway toward Right (-Y)
        double pelvisSwayY = swayM * std::sin(phaseRad);

        // Foot targets relative to hip joints
        double leftFootX = 0.0, leftFootY = 0.0, leftFootZ = nominalZ;
        double rightFootX = 0.0, rightFootY = 0.0, rightFootZ = nominalZ;
        double leftRoll = 0.0, rightRoll = 0.0;
        double leftPitch = -torsoPitchRad, rightPitch = -torsoPitchRad;

        // Sidestep mode
        double leftFootLatOffset = 0.0;
        double rightFootLatOffset = 0.0;
        if (params.mode == "sidestep_left") {
            leftFootLatOffset = swayM * 0.5;
        } else if (params.mode == "sidestep_right") {
            rightFootLatOffset = -swayM * 0.5;
        }

        // Turning mode adjustments
        double headPan = 0.0;
        if (params.mode == "turn_left") {
            headPan = turnRad * 0.5 * std::sin(phaseRad);
        } else if (params.mode == "turn_right") {
            headPan = -turnRad * 0.5 * std::sin(phaseRad);
        }

        if (normCycle < 0.5) {
            // Phase 1: Left Leg Stance (Support), Right Leg Swing
            double subPhase = normCycle / 0.5; // [0.0, 1.0]

            // Stance foot (Left) moves backward relative to pelvis
            leftFootX = (strideM * 0.5) - (strideM * subPhase);
            leftFootZ = nominalZ;
            leftFootY = -pelvisSwayY;

            // Swing foot (Right) follows cycloid trajectory forward
            double cycloidProgress = subPhase - (std::sin(2.0 * M_PI * subPhase) / (2.0 * M_PI));
            rightFootX = -(strideM * 0.5) + (strideM * cycloidProgress);
            rightFootZ = nominalZ + stepHeightM * std::sin(M_PI * subPhase);
            rightFootY = -pelvisSwayY + rightFootLatOffset;

            if (params.mode == "turn_left") {
                rightFootX += strideM * 0.2;
            } else if (params.mode == "turn_right") {
                rightFootX -= strideM * 0.2;
            }
        } else {
            // Phase 2: Right Leg Stance (Support), Left Leg Swing
            double subPhase = (normCycle - 0.5) / 0.5; // [0.0, 1.0]

            // Stance foot (Right) moves backward relative to pelvis
            rightFootX = (strideM * 0.5) - (strideM * subPhase);
            rightFootZ = nominalZ;
            rightFootY = -pelvisSwayY;

            // Swing foot (Left) follows cycloid trajectory forward
            double cycloidProgress = subPhase - (std::sin(2.0 * M_PI * subPhase) / (2.0 * M_PI));
            leftFootX = -(strideM * 0.5) + (strideM * cycloidProgress);
            leftFootZ = nominalZ + stepHeightM * std::sin(M_PI * subPhase);
            leftFootY = -pelvisSwayY + leftFootLatOffset;

            if (params.mode == "turn_left") {
                leftFootX -= strideM * 0.2;
            } else if (params.mode == "turn_right") {
                leftFootX += strideM * 0.2;
            }
        }

        // Solve 5-DOF IK for Left Leg
        std::array<double, 5> leftLegAngles;
        solveLegIk(true, leftFootX, leftFootY, leftFootZ, leftRoll, leftPitch, leftLegAngles);

        // Solve 5-DOF IK for Right Leg
        std::array<double, 5> rightLegAngles;
        solveLegIk(false, rightFootX, rightFootY, rightFootZ, rightRoll, rightPitch, rightLegAngles);

        // Arm counter-balancing
        double armOscillation = std::sin(phaseRad);
        double leftShoulderPitch = -armSwingRad * armOscillation;
        double rightShoulderPitch = armSwingRad * armOscillation;
        double shoulderRollRest = 12.0 * (M_PI / 180.0);

        if (params.armMode == "guard") {
            leftShoulderPitch = -45.0 * (M_PI / 180.0);
            rightShoulderPitch = -45.0 * (M_PI / 180.0);
            elbowRad = 70.0 * (M_PI / 180.0);
            shoulderRollRest = 20.0 * (M_PI / 180.0);
        } else if (params.armMode == "held") {
            leftShoulderPitch = 0.0;
            rightShoulderPitch = 0.0;
            elbowRad = 10.0 * (M_PI / 180.0);
        }

        const auto &limits = UrdfLimits::instance();
        std::array<double, 17> angles;

        // Left Leg: Ch 0..4
        angles[0] = leftLegAngles[0]; // Ankle Roll
        angles[1] = leftLegAngles[1]; // Ankle Pitch
        angles[2] = leftLegAngles[2]; // Knee Pitch
        angles[3] = leftLegAngles[3]; // Hip Pitch
        angles[4] = leftLegAngles[4]; // Hip Roll

        // Left Arm: Ch 5..7
        angles[5] = limits.clamp(5, leftShoulderPitch);
        angles[6] = limits.clamp(6, shoulderRollRest);
        angles[7] = limits.clamp(7, -elbowRad);  // -elbowRad bends left forearm inwards toward torso

        // Right Arm: Ch 8..10
        angles[8] = limits.clamp(8, elbowRad);   // +elbowRad bends right forearm inwards toward torso
        angles[9] = limits.clamp(9, -shoulderRollRest);
        angles[10] = limits.clamp(10, rightShoulderPitch);

        // Right Leg: Ch 11..15
        angles[11] = rightLegAngles[4]; // Hip Roll
        angles[12] = rightLegAngles[3]; // Hip Pitch
        angles[13] = rightLegAngles[2]; // Knee Pitch
        angles[14] = rightLegAngles[1]; // Ankle Pitch
        angles[15] = rightLegAngles[0]; // Ankle Roll

        // Head Yaw: Ch 16
        angles[16] = limits.clamp(16, headPan);

        seq.addKeyframe(t, angles, "cubic_in_out");
    }

    return seq;
}

MotionSequence ParametricGait::generateGesture(const std::string &gestureType)
{
    MotionSequence seq;
    seq.setGeneratorType("gesture");
    seq.setLoop(false);
    seq.setFps(50);

    QJsonObject pObj;
    pObj["gesture"] = QString::fromStdString(gestureType);
    seq.setGeneratorParams(pObj);

    const auto &limits = UrdfLimits::instance();

    // Default standby angles
    std::array<double, 17> standbyAngles;
    for (int i = 0; i < 17; ++i) {
        standbyAngles[i] = limits.getLimit(i).defaultAngle;
    }

    if (gestureType == "wave") {
        seq.setName("Friendly Wave");
        seq.setDescription("Right arm raises and waves hand back and forth smoothly");
        seq.setDurationMs(2800);

        // Keyframe 0: Standby
        seq.addKeyframe(0, standbyAngles);

        // Keyframe 1 (500ms): Raise Right Arm
        auto kf1 = standbyAngles;
        kf1[10] = limits.clamp(10, -45.0 * (M_PI / 180.0));  // Shoulder Pitch up/forward
        kf1[9]  = limits.clamp(9, -75.0 * (M_PI / 180.0));   // Shoulder Roll out
        kf1[8]  = limits.clamp(8, 65.0 * (M_PI / 180.0));    // Elbow flexed
        kf1[16] = limits.clamp(16, -15.0 * (M_PI / 180.0));  // Head looks slightly right
        seq.addKeyframe(500, kf1);

        // Keyframe 2 (900ms): Wave Left
        auto kf2 = kf1;
        kf2[8] = limits.clamp(8, 35.0 * (M_PI / 180.0));
        seq.addKeyframe(900, kf2);

        // Keyframe 3 (1300ms): Wave Right
        auto kf3 = kf1;
        kf3[8] = limits.clamp(8, 75.0 * (M_PI / 180.0));
        seq.addKeyframe(1300, kf3);

        // Keyframe 4 (1700ms): Wave Left
        seq.addKeyframe(1700, kf2);

        // Keyframe 5 (2100ms): Wave Right
        seq.addKeyframe(2100, kf3);

        // Keyframe 6 (2800ms): Return to Standby
        seq.addKeyframe(2800, standbyAngles);

    } else if (gestureType == "bow") {
        seq.setName("Respectful Bow");
        seq.setDescription("Torso pitches forward with slight knee flex, holds, and returns");
        seq.setDurationMs(3000);

        seq.addKeyframe(0, standbyAngles);

        // Bowing pose at 1000ms
        auto bowPose = standbyAngles;
        bowPose[3]  = limits.clamp(3, -28.0 * (M_PI / 180.0));  // Left Hip Pitch forward
        bowPose[12] = limits.clamp(12, -28.0 * (M_PI / 180.0)); // Right Hip Pitch forward
        bowPose[2]  = limits.clamp(2, 10.0 * (M_PI / 180.0));   // Left Knee flex backward
        bowPose[13] = limits.clamp(13, 10.0 * (M_PI / 180.0));  // Right Knee flex backward
        bowPose[1]  = limits.clamp(1, 18.0 * (M_PI / 180.0));   // Left Ankle Pitch toes up
        bowPose[14] = limits.clamp(14, 18.0 * (M_PI / 180.0));  // Right Ankle Pitch toes up
        bowPose[5]  = limits.clamp(5, 25.0 * (M_PI / 180.0));   // Left Arm back
        bowPose[10] = limits.clamp(10, 25.0 * (M_PI / 180.0));  // Right Arm back
        seq.addKeyframe(1000, bowPose);

        // Hold bow until 2000ms
        seq.addKeyframe(2000, bowPose);

        // Return to Standby by 3000ms
        seq.addKeyframe(3000, standbyAngles);

    } else if (gestureType == "clap") {
        seq.setName("Applause Clap");
        seq.setDescription("Both arms come forward to clap hands in rhythm");
        seq.setDurationMs(2400);

        seq.addKeyframe(0, standbyAngles);

        // Raise arms forward
        auto readyPose = standbyAngles;
        readyPose[5]  = limits.clamp(5, -50.0 * (M_PI / 180.0));  // Left Pitch forward
        readyPose[10] = limits.clamp(10, -50.0 * (M_PI / 180.0)); // Right Pitch forward
        readyPose[6]  = limits.clamp(6, 25.0 * (M_PI / 180.0));   // Left Roll in
        readyPose[9]  = limits.clamp(9, -25.0 * (M_PI / 180.0));  // Right Roll in
        readyPose[7]  = limits.clamp(7, -45.0 * (M_PI / 180.0));  // Left Elbow inwards
        readyPose[8]  = limits.clamp(8, 45.0 * (M_PI / 180.0));   // Right Elbow inwards
        seq.addKeyframe(600, readyPose);

        // Clap in / out cycle 1
        auto clapIn = readyPose;
        clapIn[6] = limits.clamp(6, 12.0 * (M_PI / 180.0));
        clapIn[9] = limits.clamp(9, -12.0 * (M_PI / 180.0));
        auto clapOut = readyPose;
        clapOut[6] = limits.clamp(6, 35.0 * (M_PI / 180.0));
        clapOut[9] = limits.clamp(9, -35.0 * (M_PI / 180.0));

        seq.addKeyframe(900, clapIn);
        seq.addKeyframe(1200, clapOut);
        seq.addKeyframe(1500, clapIn);
        seq.addKeyframe(1800, clapOut);

        // Return
        seq.addKeyframe(2400, standbyAngles);

    } else if (gestureType == "squat") {
        seq.setName("Athletic Squat");
        seq.setDescription("Symmetrical deep squat bending knees and hips, then rising");
        seq.setDurationMs(2000);

        seq.addKeyframe(0, standbyAngles);

        // Deep squat pose at 1000ms
        auto squatPose = standbyAngles;
        squatPose[3]  = limits.clamp(3, -35.0 * (M_PI / 180.0));  // Left Hip Pitch forward
        squatPose[12] = limits.clamp(12, -35.0 * (M_PI / 180.0)); // Right Hip Pitch forward
        squatPose[2]  = limits.clamp(2, 60.0 * (M_PI / 180.0));   // Left Knee Pitch backward
        squatPose[13] = limits.clamp(13, 60.0 * (M_PI / 180.0));  // Right Knee Pitch backward
        squatPose[1]  = limits.clamp(1, 25.0 * (M_PI / 180.0));   // Left Ankle Pitch toes up
        squatPose[14] = limits.clamp(14, 25.0 * (M_PI / 180.0));  // Right Ankle Pitch toes up
        squatPose[5]  = limits.clamp(5, -35.0 * (M_PI / 180.0));  // Arms forward for balance
        squatPose[10] = limits.clamp(10, -35.0 * (M_PI / 180.0));
        seq.addKeyframe(1000, squatPose);

        // Return to Standby
        seq.addKeyframe(2000, standbyAngles);

    } else { // default "guard"
        seq.setName("Martial Arts Guard");
        seq.setDescription("Defensive ready stance with fists raised and knees flexed");
        seq.setDurationMs(2000);

        seq.addKeyframe(0, standbyAngles);

        auto guardPose = standbyAngles;
        guardPose[5]  = limits.clamp(5, -45.0 * (M_PI / 180.0));  // Arms up/forward
        guardPose[10] = limits.clamp(10, -45.0 * (M_PI / 180.0));
        guardPose[6]  = limits.clamp(6, 20.0 * (M_PI / 180.0));
        guardPose[9]  = limits.clamp(9, -20.0 * (M_PI / 180.0));
        guardPose[7]  = limits.clamp(7, 75.0 * (M_PI / 180.0));
        guardPose[8]  = limits.clamp(8, 75.0 * (M_PI / 180.0));
        guardPose[2]  = limits.clamp(2, 20.0 * (M_PI / 180.0));   // Knee slight bend
        guardPose[13] = limits.clamp(13, 20.0 * (M_PI / 180.0));
        guardPose[3]  = limits.clamp(3, -10.0 * (M_PI / 180.0));  // Hip slight forward
        guardPose[12] = limits.clamp(12, -10.0 * (M_PI / 180.0));
        guardPose[1]  = limits.clamp(1, 10.0 * (M_PI / 180.0));   // Foot flat
        guardPose[14] = limits.clamp(14, 10.0 * (M_PI / 180.0));

        seq.addKeyframe(800, guardPose);
        seq.addKeyframe(1500, guardPose);
        seq.addKeyframe(2000, standbyAngles);
    }

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
