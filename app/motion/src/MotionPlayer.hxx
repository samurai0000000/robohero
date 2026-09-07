/*
 * MotionPlayer.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_PLAYER_HXX
#define ROBOHERO_MOTION_PLAYER_HXX

#include <QObject>
#include <QTimer>
#include <array>
#include "MotionSequence.hxx"
#include "MqttClient.hxx"

class MotionPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MotionPlayer(QObject *parent = nullptr);
    ~MotionPlayer() override;

    void setMqttClient(MqttClient *client);
    void setMotion(const MotionSequence &sequence);
    const MotionSequence &motion() const { return _sequence; }

    void play();
    void pause();
    void stop(bool smoothReturn = true);
    void seek(int timeMs, bool syncRobot = false);

    bool isPlaying() const { return _isPlaying; }
    int currentTimeMs() const { return _currentTimeMs; }
    int durationMs() const { return _sequence.durationMs(); }

    bool isLoop() const { return _loop; }
    void setLoop(bool loop) { _loop = loop; }

    bool isSyncToRobot() const { return _syncToRobot; }
    void setSyncToRobot(bool sync) { _syncToRobot = sync; }

    const std::array<double, 17> &currentAngles() const { return _currentAngles; }
    const std::array<int, 17> &currentPwm() const { return _currentPwm; }

signals:
    void playheadChanged(int timeMs);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void poseUpdated(const std::array<double, 17> &angles, const std::array<int, 17> &pwm);

private slots:
    void onTick();

private:
    void evaluateCurrentTime();
    void transmitPose();
    void startBlendToStandby();

    MotionSequence _sequence;
    MqttClient *_mqttClient;
    QTimer _timer;

    bool _isPlaying;
    bool _loop;
    bool _syncToRobot;
    int _currentTimeMs;

    std::array<double, 17> _currentAngles;
    std::array<int, 17> _currentPwm;

    // Smooth return to Standby
    bool _blendingToStandby;
    int _blendStep;
    int _totalBlendSteps;
    std::array<double, 17> _blendStartAngles;
    std::array<double, 17> _standbyAngles;
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
