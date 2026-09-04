/*
 * PoseEstimator.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "PoseEstimator.hxx"
#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>
#include <QFile>
#include <QByteArray>
#include <iostream>
#include <algorithm>
#include <cmath>

class PoseEstimator::Impl
{
public:
    Ort::Env _env{ORT_LOGGING_LEVEL_WARNING, "RoboHeroPoseEstimator"};
    Ort::SessionOptions _sessionOptions;
    std::unique_ptr<Ort::Session> _session;
    Ort::MemoryInfo _memoryInfo = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    std::string _activeProvider = "CPU";
    std::vector<std::string> _availableProviders;
    bool _modelLoaded = false;

    // Input/Output metadata
    std::string _inputName;
    std::string _outputName;
    std::vector<int64_t> _inputShape{1, 3, 640, 640};
    std::vector<int64_t> _outputShape;

    // Preprocessing buffer
    std::vector<float> _inputTensorValues;

    Impl()
    {
        _inputTensorValues.resize(1 * 3 * 640 * 640);
        _availableProviders = Ort::GetAvailableProviders();
    }
};

PoseEstimator::PoseEstimator()
    : _impl(std::make_unique<Impl>())
{
}

PoseEstimator::~PoseEstimator()
{
}

bool PoseEstimator::init(const std::string &modelPath,
                         const std::string &preferredProvider)
{
    try {
        _impl->_sessionOptions = Ort::SessionOptions();
        _impl->_sessionOptions.SetIntraOpNumThreads(4);
        _impl->_sessionOptions.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);

        std::string chosenProvider = "CPU";
        const auto &avail = _impl->_availableProviders;

        bool tryGpu = (preferredProvider == "auto" ||
                       preferredProvider == "cuda" ||
                       preferredProvider == "directml");

        if (tryGpu) {
#if defined(_WIN32)
            // On Windows, try DirectML first
            if (std::find(avail.begin(), avail.end(), "DmlExecutionProvider") != avail.end()) {
                // DirectML execution provider
                try {
                    // OrtSessionOptionsAppendExecutionProvider_DML(_impl->_sessionOptions, 0);
                    chosenProvider = "DirectML";
                } catch (...) {
                    chosenProvider = "CPU";
                }
            }
#endif
            if (chosenProvider == "CPU" &&
                std::find(avail.begin(), avail.end(), "CUDAExecutionProvider") != avail.end()) {
                try {
                    OrtCUDAProviderOptions cudaOptions;
                    cudaOptions.device_id = 0;
                    _impl->_sessionOptions.AppendExecutionProvider_CUDA(cudaOptions);
                    chosenProvider = "CUDA";
                } catch (...) {
                    chosenProvider = "CPU";
                }
            }
        }

        _impl->_activeProvider = chosenProvider;

        // Read model from Qt resource or file
        QByteArray modelData;
        QString qPath = QString::fromStdString(modelPath);
        QFile file(qPath);
        if (file.open(QIODevice::ReadOnly)) {
            modelData = file.readAll();
            file.close();
        } else {
            std::cerr << "PoseEstimator: Failed to open model file: "
                      << modelPath << std::endl;
            return false;
        }

        if (modelData.isEmpty()) {
            std::cerr << "PoseEstimator: Model data is empty: "
                      << modelPath << std::endl;
            return false;
        }

        _impl->_session = std::make_unique<Ort::Session>(
            _impl->_env,
            modelData.constData(),
            modelData.size(),
            _impl->_sessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;

        // Query input names
        auto inputNamePtr = _impl->_session->GetInputNameAllocated(0, allocator);
        _impl->_inputName = inputNamePtr.get();

        // Query output names
        auto outputNamePtr = _impl->_session->GetOutputNameAllocated(0, allocator);
        _impl->_outputName = outputNamePtr.get();

        _impl->_modelLoaded = true;
        std::cout << "PoseEstimator: Loaded model successfully with provider: "
                  << _impl->_activeProvider << std::endl;
        return true;
    } catch (const Ort::Exception &e) {
        std::cerr << "PoseEstimator: ONNX Runtime Exception: "
                  << e.what() << std::endl;
        _impl->_modelLoaded = false;
        return false;
    } catch (const std::exception &e) {
        std::cerr << "PoseEstimator: Exception: " << e.what() << std::endl;
        _impl->_modelLoaded = false;
        return false;
    }
}

PoseEstimator::PersonPose PoseEstimator::infer(const cv::Mat &bgrFrame,
                                              float confThreshold,
                                              float kptThreshold,
                                              const std::string &targetSelection)
{
    PersonPose result;
    result.valid = false;

    if (!_impl->_modelLoaded || bgrFrame.empty()) {
        return result;
    }

    const int targetW = 640;
    const int targetH = 640;
    const int srcW = bgrFrame.cols;
    const int srcH = bgrFrame.rows;

    // Letterbox scaling
    float scale = std::min(static_cast<float>(targetW) / srcW,
                           static_cast<float>(targetH) / srcH);
    int newW = static_cast<int>(std::round(srcW * scale));
    int newH = static_cast<int>(std::round(srcH * scale));
    int padX = (targetW - newW) / 2;
    int padY = (targetH - newH) / 2;

    cv::Mat resized;
    cv::resize(bgrFrame, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    cv::Mat letterboxed(targetH, targetW, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(letterboxed(cv::Rect(padX, padY, newW, newH)));

    // BGR -> RGB and normalize to CHW [1, 3, 640, 640]
    float *tensorPtr = _impl->_inputTensorValues.data();
    float *rChannel = tensorPtr;
    float *gChannel = tensorPtr + targetW * targetH;
    float *bChannel = tensorPtr + 2 * targetW * targetH;

    for (int y = 0; y < targetH; ++y) {
        const cv::Vec3b *rowPtr = letterboxed.ptr<cv::Vec3b>(y);
        int rowOffset = y * targetW;
        for (int x = 0; x < targetW; ++x) {
            const cv::Vec3b &bgr = rowPtr[x];
            int idx = rowOffset + x;
            rChannel[idx] = bgr[2] / 255.0f;
            gChannel[idx] = bgr[1] / 255.0f;
            bChannel[idx] = bgr[0] / 255.0f;
        }
    }

    // Prepare ONNX tensor
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        _impl->_memoryInfo,
        _impl->_inputTensorValues.data(),
        _impl->_inputTensorValues.size(),
        _impl->_inputShape.data(),
        _impl->_inputShape.size());

    const char *inputNames[] = {_impl->_inputName.c_str()};
    const char *outputNames[] = {_impl->_outputName.c_str()};

    try {
        auto outputTensors = _impl->_session->Run(
            Ort::RunOptions{nullptr},
            inputNames, &inputTensor, 1,
            outputNames, 1);

        if (outputTensors.empty()) {
            return result;
        }

        float *outData = outputTensors.front().GetTensorMutableData<float>();
        // Shape: [1, 56, 8400]
        const int numChannels = 56;
        const int numAnchors = 8400;

        int bestAnchor = -1;
        float bestMetric = -1.0f;

        for (int i = 0; i < numAnchors; ++i) {
            float score = outData[4 * numAnchors + i];
            if (score < confThreshold) {
                continue;
            }

            float w = outData[2 * numAnchors + i];
            float h = outData[3 * numAnchors + i];
            float metric = 0.0f;

            if (targetSelection == "largest") {
                metric = w * h;
            } else { // "highest_confidence"
                metric = score;
            }

            if (metric > bestMetric) {
                bestMetric = metric;
                bestAnchor = i;
            }
        }

        if (bestAnchor < 0) {
            return result;
        }

        // Decode best candidate
        float cx = outData[0 * numAnchors + bestAnchor];
        float cy = outData[1 * numAnchors + bestAnchor];
        float bw = outData[2 * numAnchors + bestAnchor];
        float bh = outData[3 * numAnchors + bestAnchor];
        float score = outData[4 * numAnchors + bestAnchor];

        // Map box back to original image [0, 1]
        result.boxX1 = std::clamp((cx - bw * 0.5f - padX) / scale / srcW, 0.0f, 1.0f);
        result.boxY1 = std::clamp((cy - bh * 0.5f - padY) / scale / srcH, 0.0f, 1.0f);
        result.boxX2 = std::clamp((cx + bw * 0.5f - padX) / scale / srcW, 0.0f, 1.0f);
        result.boxY2 = std::clamp((cy + bh * 0.5f - padY) / scale / srcH, 0.0f, 1.0f);
        result.score = score;
        result.valid = true;

        // Decode 17 keypoints
        for (int j = 0; j < 17; ++j) {
            float kx = outData[(5 + j * 3 + 0) * numAnchors + bestAnchor];
            float ky = outData[(5 + j * 3 + 1) * numAnchors + bestAnchor];
            float kc = outData[(5 + j * 3 + 2) * numAnchors + bestAnchor];

            result.keypoints[j].x = std::clamp((kx - padX) / scale / srcW, 0.0f, 1.0f);
            result.keypoints[j].y = std::clamp((ky - padY) / scale / srcH, 0.0f, 1.0f);
            result.keypoints[j].conf = kc;
        }

    } catch (const Ort::Exception &e) {
        std::cerr << "PoseEstimator: Run error: " << e.what() << std::endl;
        result.valid = false;
    }

    return result;
}

std::string PoseEstimator::getActiveProviderName() const
{
    return _impl->_activeProvider;
}

std::vector<std::string> PoseEstimator::getAvailableProviders() const
{
    return _impl->_availableProviders;
}

bool PoseEstimator::isModelLoaded() const
{
    return _impl->_modelLoaded;
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
