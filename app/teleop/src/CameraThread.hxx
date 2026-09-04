/*
 * CameraThread.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_CAMERA_THREAD_HXX
#define ROBOHERO_CAMERA_THREAD_HXX

#include <QThread>
#include <QImage>
#include <QMutex>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <memory>
#include <atomic>
#include "PoseEstimator.hxx"

class CameraThread : public QThread
{
    Q_OBJECT

public:
    CameraThread(std::shared_ptr<PoseEstimator> estimator, QObject *parent = nullptr);
    ~CameraThread() override;

    void setCameraConfig(int deviceIndex, int width, int height, int targetFps,
                         bool flipHorizontal);
    void setInferenceParams(float confThreshold, float kptThreshold,
                            const std::string &targetSelection,
                            bool enableSkeletonOverlay);
    void stopCapture();
    bool isRunning() const;
    double getActualFps() const;

signals:
    void frameReady(const QImage &image, const PoseEstimator::PersonPose &pose);
    void cameraError(const QString &errorMsg);

protected:
    void run() override;

private:
    std::shared_ptr<PoseEstimator> _estimator;
    std::atomic<bool> _running;
    QMutex _configMutex;

    int _deviceIndex;
    int _width;
    int _height;
    int _targetFps;
    bool _flipHorizontal;

    float _confThreshold;
    float _kptThreshold;
    std::string _targetSelection;
    bool _enableSkeletonOverlay;

    double _actualFps;

    void drawSkeleton(cv::Mat &frame, const PoseEstimator::PersonPose &pose);
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
