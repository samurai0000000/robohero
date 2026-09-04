/*
 * CameraThread.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "CameraThread.hxx"
#include <opencv2/imgproc.hpp>
#include <QElapsedTimer>
#include <iostream>
#include <chrono>
#include <cstdio>

CameraThread::CameraThread(std::shared_ptr<PoseEstimator> estimator, QObject *parent)
    : QThread(parent)
    , _estimator(estimator)
    , _running(false)
    , _deviceIndex(0)
    , _width(640)
    , _height(480)
    , _targetFps(30)
    , _flipHorizontal(true)
    , _confThreshold(0.50f)
    , _kptThreshold(0.40f)
    , _targetSelection("largest")
    , _enableSkeletonOverlay(true)
    , _actualFps(0.0)
{
}

CameraThread::~CameraThread()
{
    stopCapture();
    wait();
}

void CameraThread::setCameraConfig(int deviceIndex, int width, int height,
                                   int targetFps, bool flipHorizontal)
{
    QMutexLocker locker(&_configMutex);
    _deviceIndex = deviceIndex;
    _width = width;
    _height = height;
    _targetFps = targetFps;
    _flipHorizontal = flipHorizontal;
}

void CameraThread::setInferenceParams(float confThreshold, float kptThreshold,
                                      const std::string &targetSelection,
                                      bool enableSkeletonOverlay)
{
    QMutexLocker locker(&_configMutex);
    _confThreshold = confThreshold;
    _kptThreshold = kptThreshold;
    _targetSelection = targetSelection;
    _enableSkeletonOverlay = enableSkeletonOverlay;
}

void CameraThread::stopCapture()
{
    _running = false;
}

bool CameraThread::isRunning() const
{
    return _running;
}

double CameraThread::getActualFps() const
{
    return _actualFps;
}

void CameraThread::run()
{
    _running = true;

    int devIdx = 0;
    int reqW = 640;
    int reqH = 480;
    int fps = 30;
    bool flipH = true;

    {
        QMutexLocker locker(&_configMutex);
        devIdx = _deviceIndex;
        reqW = _width;
        reqH = _height;
        fps = _targetFps;
        flipH = _flipHorizontal;
    }

    cv::VideoCapture cap;
#ifdef _WIN32
    bool capOpened = cap.open(devIdx, cv::CAP_DSHOW);
#else
    bool capOpened = cap.open(devIdx);
#endif
    if (capOpened) {
        cap.set(cv::CAP_PROP_FRAME_WIDTH, reqW);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, reqH);
        cap.set(cv::CAP_PROP_FPS, fps);
    } else {
        emit cameraError(QString("Failed to open camera device %1. Using synthetic test feed.").arg(devIdx));
    }

    QElapsedTimer timer;
    timer.start();
    int frameCount = 0;
    float syntheticAngle = 0.0f;

    cv::Mat rawFrame;

    while (_running) {
        auto frameStart = std::chrono::steady_clock::now();

        bool gotFrame = false;
        if (capOpened) {
            gotFrame = cap.read(rawFrame);
        }

        if (!gotFrame || rawFrame.empty()) {
            // Synthetic test pattern when physical camera is unavailable
            rawFrame = cv::Mat(reqH, reqW, CV_8UC3, cv::Scalar(40, 40, 45));

            // Draw grid lines
            for (int y = 0; y < reqH; y += 40) {
                cv::line(rawFrame, cv::Point(0, y), cv::Point(reqW, y),
                         cv::Scalar(55, 55, 60), 1);
            }
            for (int x = 0; x < reqW; x += 40) {
                cv::line(rawFrame, cv::Point(x, 0), cv::Point(x, reqH),
                         cv::Scalar(55, 55, 60), 1);
            }

            cv::putText(rawFrame, "RoboHero Camera Test Feed",
                        cv::Point(30, 40), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                        cv::Scalar(0, 220, 220), 2);
            cv::putText(rawFrame, "No hardware camera detected / opened",
                        cv::Point(30, 75), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(180, 180, 180), 1);

            // Animate a synthetic person stick figure for interactive retargeting test
            syntheticAngle += 0.04f;
            float cx = reqW * 0.5f;
            float cy = reqH * 0.5f;
            float armLen = 80.0f;
            float leftArmY = cy - 20.0f + std::sin(syntheticAngle) * 50.0f;
            float rightArmY = cy - 20.0f + std::cos(syntheticAngle) * 50.0f;

            // Synthetic head
            cv::circle(rawFrame, cv::Point(cx, cy - 80), 25, cv::Scalar(0, 255, 200), 2);
            // Torso
            cv::line(rawFrame, cv::Point(cx, cy - 55), cv::Point(cx, cy + 50),
                     cv::Scalar(0, 255, 200), 2);
            // Arms
            cv::line(rawFrame, cv::Point(cx, cy - 30), cv::Point(cx - armLen, leftArmY),
                     cv::Scalar(255, 200, 0), 2);
            cv::line(rawFrame, cv::Point(cx, cy - 30), cv::Point(cx + armLen, rightArmY),
                     cv::Scalar(0, 200, 255), 2);
            // Legs
            cv::line(rawFrame, cv::Point(cx, cy + 50), cv::Point(cx - 40, cy + 130),
                     cv::Scalar(0, 255, 200), 2);
            cv::line(rawFrame, cv::Point(cx, cy + 50), cv::Point(cx + 40, cy + 130),
                     cv::Scalar(0, 255, 200), 2);
        } else {
            if (flipH) {
                cv::flip(rawFrame, rawFrame, 1);
            }
        }

        // Copy active parameters
        float confThresh = _confThreshold;
        float kptThresh = _kptThreshold;
        std::string targetSel = _targetSelection;
        bool overlay = _enableSkeletonOverlay;

        PoseEstimator::PersonPose pose;
        if (_estimator && _estimator->isModelLoaded()) {
            pose = _estimator->infer(rawFrame, confThresh, kptThresh, targetSel);
        }

        if (overlay && pose.valid) {
            drawSkeleton(rawFrame, pose);
        }

        // Convert cv::Mat to QImage
        cv::Mat rgbMat;
        cv::cvtColor(rawFrame, rgbMat, cv::COLOR_BGR2RGB);
        QImage qimg(rgbMat.data, rgbMat.cols, rgbMat.rows,
                    static_cast<int>(rgbMat.step), QImage::Format_RGB888);

        emit frameReady(qimg.copy(), pose);

        // Frame rate calculation
        frameCount++;
        if (timer.elapsed() >= 1000) {
            _actualFps = frameCount * 1000.0 / timer.elapsed();
            frameCount = 0;
            timer.restart();
        }

        // Regulate frame rate
        int frameIntervalMs = 1000 / (fps > 0 ? fps : 30);
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - frameStart).count();
        if (elapsedMs < frameIntervalMs) {
            msleep(static_cast<unsigned long>(frameIntervalMs - elapsedMs));
        }
    }

    if (cap.isOpened()) {
        cap.release();
    }
}

void CameraThread::drawSkeleton(cv::Mat &frame, const PoseEstimator::PersonPose &pose)
{
    int w = frame.cols;
    int h = frame.rows;

    // Draw bounding box
    cv::Rect box(
        static_cast<int>(pose.boxX1 * w),
        static_cast<int>(pose.boxY1 * h),
        static_cast<int>((pose.boxX2 - pose.boxX1) * w),
        static_cast<int>((pose.boxY2 - pose.boxY1) * h));
    cv::rectangle(frame, box, cv::Scalar(0, 200, 255), 2);

    char scoreStr[32];
    std::snprintf(scoreStr, sizeof(scoreStr), "Person: %.2f", pose.score);
    cv::putText(frame, scoreStr, cv::Point(box.x, std::max(20, box.y - 8)),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 200, 255), 2);

    // Skeleton bones (pairs of joint indices)
    static const std::vector<std::pair<int, int>> bones = {
        {PoseEstimator::NOSE, PoseEstimator::LEFT_EYE},
        {PoseEstimator::NOSE, PoseEstimator::RIGHT_EYE},
        {PoseEstimator::LEFT_EYE, PoseEstimator::LEFT_EAR},
        {PoseEstimator::RIGHT_EYE, PoseEstimator::RIGHT_EAR},
        {PoseEstimator::LEFT_SHOULDER, PoseEstimator::RIGHT_SHOULDER},
        {PoseEstimator::LEFT_SHOULDER, PoseEstimator::LEFT_ELBOW},
        {PoseEstimator::LEFT_ELBOW, PoseEstimator::LEFT_WRIST},
        {PoseEstimator::RIGHT_SHOULDER, PoseEstimator::RIGHT_ELBOW},
        {PoseEstimator::RIGHT_ELBOW, PoseEstimator::RIGHT_WRIST},
        {PoseEstimator::LEFT_SHOULDER, PoseEstimator::LEFT_HIP},
        {PoseEstimator::RIGHT_SHOULDER, PoseEstimator::RIGHT_HIP},
        {PoseEstimator::LEFT_HIP, PoseEstimator::RIGHT_HIP},
        {PoseEstimator::LEFT_HIP, PoseEstimator::LEFT_KNEE},
        {PoseEstimator::LEFT_KNEE, PoseEstimator::LEFT_ANKLE},
        {PoseEstimator::RIGHT_HIP, PoseEstimator::RIGHT_KNEE},
        {PoseEstimator::RIGHT_KNEE, PoseEstimator::RIGHT_ANKLE}
    };

    // Draw lines
    for (const auto &bone : bones) {
        const auto &k1 = pose.keypoints[bone.first];
        const auto &k2 = pose.keypoints[bone.second];

        if (k1.conf >= _kptThreshold && k2.conf >= _kptThreshold) {
            cv::Point p1(static_cast<int>(k1.x * w), static_cast<int>(k1.y * h));
            cv::Point p2(static_cast<int>(k2.x * w), static_cast<int>(k2.y * h));

            float avgConf = (k1.conf + k2.conf) * 0.5f;
            cv::Scalar color;
            if (avgConf >= 0.65f) {
                color = cv::Scalar(0, 255, 0);     // Green
            } else if (avgConf >= 0.40f) {
                color = cv::Scalar(0, 200, 255);   // Yellow
            } else {
                color = cv::Scalar(0, 0, 255);     // Red
            }

            cv::line(frame, p1, p2, color, 2, cv::LINE_AA);
        }
    }

    // Draw keypoint dots
    for (int i = 0; i < 17; ++i) {
        const auto &k = pose.keypoints[i];
        if (k.conf >= _kptThreshold) {
            cv::Point p(static_cast<int>(k.x * w), static_cast<int>(k.y * h));
            cv::circle(frame, p, 4, cv::Scalar(255, 255, 255), -1, cv::LINE_AA);
            cv::circle(frame, p, 5, cv::Scalar(0, 140, 255), 1, cv::LINE_AA);
        }
    }
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
