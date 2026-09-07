/*
 * TimelineWidget.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_TIMELINE_WIDGET_HXX
#define ROBOHERO_MOTION_TIMELINE_WIDGET_HXX

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <vector>
#include "MotionSequence.hxx"

class TimelineTrackWidget;

class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr);
    ~TimelineWidget() override;

    void setDuration(int durationMs);
    int duration() const { return _durationMs; }

    void setTime(int timeMs);
    int currentTime() const { return _currentTimeMs; }

    void setKeyframes(const std::vector<MotionSequence::Keyframe> &keyframes);
    void setPlaying(bool playing);
    bool isPlaying() const { return _playing; }

    bool isLoop() const;
    void setLoop(bool loop);

    bool isSyncToRobot() const;
    void setSyncToRobot(bool sync);

signals:
    void timeChanged(int timeMs);
    void playToggled(bool playing);
    void stopClicked();
    void loopToggled(bool loop);
    void syncToRobotToggled(bool sync);
    void addKeyframeRequested(int timeMs);
    void deleteKeyframeRequested(int keyframeIndex);
    void keyframeSelected(int keyframeIndex);

private slots:
    void onSliderValueChanged(int value);
    void onPlayBtnClicked();
    void onStopBtnClicked();
    void onLoopCheckToggled(bool checked);
    void onSyncCheckToggled(bool checked);
    void onAddKeyframeBtnClicked();
    void onDeleteKeyframeBtnClicked();
    void onTrackKeyframeClicked(int keyframeIndex, int timeMs);

private:
    void setupUi();
    void updateTimeLabel();

    int _durationMs;
    int _currentTimeMs;
    bool _playing;
    int _selectedKeyframeIndex;
    std::vector<MotionSequence::Keyframe> _keyframes;

    QPushButton *_playBtn;
    QPushButton *_stopBtn;
    QCheckBox *_loopCheck;
    QCheckBox *_syncCheck;
    QPushButton *_addKfBtn;
    QPushButton *_delKfBtn;
    QLabel *_timeLabel;
    QSlider *_slider;
    TimelineTrackWidget *_trackWidget;
};

// Dope sheet / track visualizer widget
class TimelineTrackWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineTrackWidget(QWidget *parent = nullptr);

    void setDuration(int durationMs);
    void setPlayhead(int timeMs);
    void setKeyframes(const std::vector<MotionSequence::Keyframe> &keyframes);
    void setSelectedKeyframe(int index);

signals:
    void keyframeClicked(int index, int timeMs);
    void trackClicked(int timeMs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int _durationMs;
    int _playheadMs;
    int _selectedIndex;
    std::vector<MotionSequence::Keyframe> _keyframes;
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
