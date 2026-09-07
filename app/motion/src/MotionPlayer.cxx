/*
 * MotionPlayer.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MotionPlayer.hxx"
#include "UrdfLimits.hxx"
#include <cmath>

MotionPlayer::MotionPlayer(QObject *parent)
    : QObject(parent)
    , _mqttClient(nullptr)
    , _isPlaying(false)
    , _loop(true)
    , _syncToRobot(false)
    , _currentTimeMs(0)
    , _blendingToStandby(false)
    , _blendStep(0)
    , _totalBlendSteps(15) // 300 ms at 50 Hz
{
    const auto &limits = UrdfLimits::instance();
    for (int i = 0; i < 17; ++i) {
        double defAngle = limits.getLimit(i).defaultAngle;
        _standbyAngles[i] = defAngle;
        _currentAngles[i] = defAngle;
        _currentPwm[i] = limits.angleToPwm(i, defAngle);
    }

    connect(&_timer, &QTimer::timeout, this, &MotionPlayer::onTick);
}

MotionPlayer::~MotionPlayer()
{
    _timer.stop();
}

void MotionPlayer::setMqttClient(MqttClient *client)
{
    _mqttClient = client;
}

void MotionPlayer::setMotion(const MotionSequence &sequence, bool resetTime)
{
    bool wasPlaying = _isPlaying;
    if (_isPlaying) {
        pause();
    }

    _sequence = sequence;
    _loop = sequence.isLoop();
    if (resetTime) {
        _currentTimeMs = 0;
    } else {
        _currentTimeMs = std::max(0, std::min(_currentTimeMs, _sequence.durationMs()));
    }
    evaluateCurrentTime();

    if (wasPlaying) {
        play();
    }
}

void MotionPlayer::play()
{
    if (_isPlaying) return;

    _blendingToStandby = false;
    _isPlaying = true;
    _timer.start(20); // 50 Hz
    emit playbackStarted();
}

void MotionPlayer::pause()
{
    if (!_isPlaying) return;

    _isPlaying = false;
    _timer.stop();
    emit playbackPaused();
}

void MotionPlayer::stop(bool smoothReturn)
{
    _isPlaying = false;

    if (smoothReturn && _currentTimeMs > 0) {
        startBlendToStandby();
    } else {
        _timer.stop();
        _currentTimeMs = 0;
        evaluateCurrentTime();
        emit playheadChanged(0);
        emit playbackStopped();
    }
}

void MotionPlayer::seek(int timeMs, bool syncRobot)
{
    _blendingToStandby = false;
    _currentTimeMs = std::max(0, std::min(timeMs, _sequence.durationMs()));
    evaluateCurrentTime();

    if (syncRobot || _syncToRobot) {
        transmitPose();
    }
}

void MotionPlayer::setCurrentJointAngle(int channel, double angle, int pwm)
{
    if (channel >= 0 && channel < 17) {
        _currentAngles[channel] = angle;
        _currentPwm[channel] = pwm;
    }
}

void MotionPlayer::setCurrentPose(const std::array<double, 17> &angles,
                                  const std::array<int, 17> &pwm)
{
    _currentAngles = angles;
    _currentPwm = pwm;
}

void MotionPlayer::evaluateCurrentTime()
{
    if (_sequence.keyframeCount() == 0) {
        const auto &limits = UrdfLimits::instance();
        for (int i = 0; i < 17; ++i) {
            _currentAngles[i] = limits.getLimit(i).defaultAngle;
            _currentPwm[i] = limits.angleToPwm(i, _currentAngles[i]);
        }
    } else {
        _sequence.evaluate(_currentTimeMs, _currentAngles, _currentPwm);
    }

    emit poseUpdated(_currentAngles, _currentPwm);
}

void MotionPlayer::transmitPose()
{
    if (_mqttClient && _mqttClient->isConnected()) {
        _mqttClient->sendPwm(_currentPwm, _mqttClient->controlTopic());
    }
}

void MotionPlayer::startBlendToStandby()
{
    _blendingToStandby = true;
    _blendStep = 0;
    _blendStartAngles = _currentAngles;

    const auto &limits = UrdfLimits::instance();
    for (int i = 0; i < 17; ++i) {
        _standbyAngles[i] = limits.getLimit(i).defaultAngle;
    }

    if (!_timer.isActive()) {
        _timer.start(20);
    }
}

void MotionPlayer::onTick()
{
    if (_blendingToStandby) {
        _blendStep++;
        double alpha = static_cast<double>(_blendStep) / _totalBlendSteps;
        if (alpha > 1.0) alpha = 1.0;

        const auto &limits = UrdfLimits::instance();
        for (int i = 0; i < 17; ++i) {
            _currentAngles[i] = (1.0 - alpha) * _blendStartAngles[i] + alpha * _standbyAngles[i];
            _currentPwm[i] = limits.angleToPwm(i, _currentAngles[i]);
        }

        emit poseUpdated(_currentAngles, _currentPwm);
        if (_syncToRobot) {
            transmitPose();
        }

        if (_blendStep >= _totalBlendSteps) {
            _blendingToStandby = false;
            _timer.stop();
            _currentTimeMs = 0;
            emit playheadChanged(0);
            emit playbackStopped();
        }
        return;
    }

    if (!_isPlaying) {
        _timer.stop();
        return;
    }

    int duration = _sequence.durationMs();
    if (duration <= 0) {
        pause();
        return;
    }

    _currentTimeMs += 20;

    if (_currentTimeMs >= duration) {
        if (_loop) {
            _currentTimeMs = _currentTimeMs % duration;
        } else {
            _currentTimeMs = duration;
            evaluateCurrentTime();
            emit playheadChanged(_currentTimeMs);
            _isPlaying = false;
            startBlendToStandby();
            return;
        }
    }

    evaluateCurrentTime();
    emit playheadChanged(_currentTimeMs);

    if (_syncToRobot) {
        transmitPose();
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
