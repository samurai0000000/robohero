/*
 * PoseEstimator.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_POSE_ESTIMATOR_HXX
#define ROBOHERO_POSE_ESTIMATOR_HXX

#include <opencv2/core.hpp>
#include <QMetaType>
#include <array>
#include <string>
#include <vector>
#include <memory>

class PoseEstimator
{
public:
    struct Keypoint
    {
        float x;    // Normalized [0, 1]
        float y;    // Normalized [0, 1]
        float conf; // Confidence [0, 1]
    };

    struct PersonPose
    {
        std::array<Keypoint, 17> keypoints;
        float boxX1 = 0.0f;
        float boxY1 = 0.0f;
        float boxX2 = 0.0f;
        float boxY2 = 0.0f;
        float score = 0.0f;
        bool valid = false;
    };

    PoseEstimator();
    ~PoseEstimator();

    bool init(const std::string &modelPath = ":/models/yolov8n-pose.onnx",
              const std::string &preferredProvider = "auto");

    PersonPose infer(const cv::Mat &bgrFrame,
                     float confThreshold = 0.50f,
                     float kptThreshold = 0.40f,
                     const std::string &targetSelection = "largest");

    std::string getActiveProviderName() const;
    std::vector<std::string> getAvailableProviders() const;
    bool isModelLoaded() const;

    // COCO 17 Landmark Indices
    enum CocoJoint
    {
        NOSE = 0,
        LEFT_EYE = 1,
        RIGHT_EYE = 2,
        LEFT_EAR = 3,
        RIGHT_EAR = 4,
        LEFT_SHOULDER = 5,
        RIGHT_SHOULDER = 6,
        LEFT_ELBOW = 7,
        RIGHT_ELBOW = 8,
        LEFT_WRIST = 9,
        RIGHT_WRIST = 10,
        LEFT_HIP = 11,
        RIGHT_HIP = 12,
        LEFT_KNEE = 13,
        RIGHT_KNEE = 14,
        LEFT_ANKLE = 15,
        RIGHT_ANKLE = 16
    };

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

Q_DECLARE_METATYPE(PoseEstimator::Keypoint)
Q_DECLARE_METATYPE(PoseEstimator::PersonPose)

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
