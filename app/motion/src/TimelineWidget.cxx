/*
 * TimelineWidget.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "TimelineWidget.hxx"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QStyleOptionSlider>
#include <cmath>

// ==================== TimelineTrackWidget ====================

TimelineTrackWidget::TimelineTrackWidget(QWidget *parent)
    : QWidget(parent)
    , _durationMs(1000)
    , _playheadMs(0)
    , _selectedIndex(-1)
{
    setMinimumHeight(28);
    setMaximumHeight(36);
    setCursor(Qt::PointingHandCursor);
}

void TimelineTrackWidget::setDuration(int durationMs)
{
    _durationMs = std::max(10, durationMs);
    update();
}

void TimelineTrackWidget::setPlayhead(int timeMs)
{
    _playheadMs = timeMs;
    update();
}

void TimelineTrackWidget::setKeyframes(const std::vector<MotionSequence::Keyframe> &keyframes)
{
    _keyframes = keyframes;
    update();
}

void TimelineTrackWidget::setSelectedKeyframe(int index)
{
    _selectedIndex = index;
    update();
}

void TimelineTrackWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Background
    p.fillRect(rect(), QColor(22, 22, 26));

    // Ruler lines
    p.setPen(QPen(QColor(50, 50, 60), 1));
    int stepMs = 500;
    if (_durationMs > 10000) stepMs = 2000;
    else if (_durationMs > 5000) stepMs = 1000;
    else if (_durationMs < 1500) stepMs = 200;

    for (int t = 0; t <= _durationMs; t += stepMs) {
        int x = static_cast<int>(static_cast<double>(t) / _durationMs * (w - 16)) + 8;
        p.drawLine(x, h - 8, x, h);
    }

    // Keyframe diamonds
    for (size_t i = 0; i < _keyframes.size(); ++i) {
        int t = _keyframes[i].timeMs;
        int x = static_cast<int>(static_cast<double>(t) / _durationMs * (w - 16)) + 8;
        int centerY = h / 2;

        QPolygon diamond;
        diamond << QPoint(x, centerY - 6)
                << QPoint(x + 6, centerY)
                << QPoint(x, centerY + 6)
                << QPoint(x - 6, centerY);

        if (static_cast<int>(i) == _selectedIndex) {
            p.setBrush(QColor(0, 255, 136));
            p.setPen(QPen(QColor(255, 255, 255), 1.5));
        } else {
            p.setBrush(QColor(0, 173, 181));
            p.setPen(QPen(QColor(10, 10, 15), 1));
        }
        p.drawPolygon(diamond);
    }

    // Playhead line
    int playX = static_cast<int>(static_cast<double>(_playheadMs) / _durationMs * (w - 16)) + 8;
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(playX, 0, playX, h);

    // Playhead head inverted triangle
    QPolygon head;
    head << QPoint(playX - 4, 0)
         << QPoint(playX + 4, 0)
         << QPoint(playX, 6);
    p.setBrush(QColor(255, 80, 80));
    p.drawPolygon(head);
}

void TimelineTrackWidget::mousePressEvent(QMouseEvent *event)
{
    int w = width();
    int x = event->pos().x();
    double ratio = static_cast<double>(x - 8) / (w - 16);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int clickedMs = static_cast<int>(ratio * _durationMs);

    // Check if clicked close to a keyframe
    int closestIdx = -1;
    int closestDistPx = 12; // Snap radius in px

    for (size_t i = 0; i < _keyframes.size(); ++i) {
        int kfX = static_cast<int>(static_cast<double>(_keyframes[i].timeMs) / _durationMs * (w - 16)) + 8;
        int dist = std::abs(kfX - x);
        if (dist < closestDistPx) {
            closestDistPx = dist;
            closestIdx = static_cast<int>(i);
        }
    }

    if (closestIdx >= 0) {
        _selectedIndex = closestIdx;
        emit keyframeClicked(closestIdx, _keyframes[closestIdx].timeMs);
    } else {
        _selectedIndex = -1;
        emit trackClicked(clickedMs);
    }
    update();
}

// ==================== TimelineWidget ====================

TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget(parent)
    , _durationMs(1600)
    , _currentTimeMs(0)
    , _playing(false)
    , _selectedKeyframeIndex(-1)
    , _playBtn(nullptr)
    , _stopBtn(nullptr)
    , _loopCheck(nullptr)
    , _syncCheck(nullptr)
    , _addKfBtn(nullptr)
    , _delKfBtn(nullptr)
    , _timeLabel(nullptr)
    , _slider(nullptr)
    , _trackWidget(nullptr)
{
    setupUi();
}

TimelineWidget::~TimelineWidget()
{
}

void TimelineWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(4);

    // Top Row: Transport controls
    auto *controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(8);

    _playBtn = new QPushButton("▶ Play", this);
    _playBtn->setFixedWidth(80);
    _playBtn->setStyleSheet("background-color: #00ADB5; color: #ffffff; font-weight: bold; padding: 4px 8px; border-radius: 4px;");
    connect(_playBtn, &QPushButton::clicked, this, &TimelineWidget::onPlayBtnClicked);
    controlsLayout->addWidget(_playBtn);

    _stopBtn = new QPushButton("⏹ Stop", this);
    _stopBtn->setFixedWidth(75);
    _stopBtn->setStyleSheet("background-color: #444450; color: #ffffff; padding: 4px 8px; border-radius: 4px;");
    connect(_stopBtn, &QPushButton::clicked, this, &TimelineWidget::onStopBtnClicked);
    controlsLayout->addWidget(_stopBtn);

    _loopCheck = new QCheckBox("🔁 Loop", this);
    _loopCheck->setChecked(true);
    connect(_loopCheck, &QCheckBox::toggled, this, &TimelineWidget::onLoopCheckToggled);
    controlsLayout->addWidget(_loopCheck);

    _syncCheck = new QCheckBox("⚡ Sync to Robot", this);
    _syncCheck->setToolTip("Live-stream current timeline pose to connected robot via MQTT");
    connect(_syncCheck, &QCheckBox::toggled, this, &TimelineWidget::onSyncCheckToggled);
    controlsLayout->addWidget(_syncCheck);

    controlsLayout->addSpacing(16);

    _addKfBtn = new QPushButton("+ Add Keyframe", this);
    _addKfBtn->setToolTip("Insert keyframe at current timeline position");
    connect(_addKfBtn, &QPushButton::clicked, this, &TimelineWidget::onAddKeyframeBtnClicked);
    controlsLayout->addWidget(_addKfBtn);

    _delKfBtn = new QPushButton("- Delete Keyframe", this);
    _delKfBtn->setToolTip("Remove selected keyframe");
    _delKfBtn->setEnabled(false);
    connect(_delKfBtn, &QPushButton::clicked, this, &TimelineWidget::onDeleteKeyframeBtnClicked);
    controlsLayout->addWidget(_delKfBtn);

    controlsLayout->addStretch();

    _timeLabel = new QLabel("00:00.000 / 00:01.600", this);
    _timeLabel->setStyleSheet("font-family: monospace; font-size: 13px; font-weight: bold; color: #00ADB5; padding: 2px 8px;");
    controlsLayout->addWidget(_timeLabel);

    mainLayout->addLayout(controlsLayout);

    // Track Dope Sheet (Keyframe Diamonds)
    _trackWidget = new TimelineTrackWidget(this);
    connect(_trackWidget, &TimelineTrackWidget::keyframeClicked, this, &TimelineWidget::onTrackKeyframeClicked);
    connect(_trackWidget, &TimelineTrackWidget::trackClicked, this, [this](int ms) {
        setTime(ms);
        emit timeChanged(ms);
    });
    mainLayout->addWidget(_trackWidget);

    // Horizontal Scrub Slider
    _slider = new QSlider(Qt::Horizontal, this);
    _slider->setRange(0, _durationMs);
    _slider->setValue(0);
    _slider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: #26262e; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #00ADB5; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #ffffff; border: 1px solid #00ADB5; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #00ADB5; border-color: #ffffff; }"
    );
    connect(_slider, &QSlider::valueChanged, this, &TimelineWidget::onSliderValueChanged);
    mainLayout->addWidget(_slider);

    updateTimeLabel();
}

void TimelineWidget::setDuration(int durationMs)
{
    _durationMs = std::max(10, durationMs);
    _slider->blockSignals(true);
    _slider->setRange(0, _durationMs);
    _slider->blockSignals(false);
    _trackWidget->setDuration(_durationMs);
    updateTimeLabel();
}

void TimelineWidget::setTime(int timeMs)
{
    _currentTimeMs = std::max(0, std::min(timeMs, _durationMs));
    _slider->blockSignals(true);
    _slider->setValue(_currentTimeMs);
    _slider->blockSignals(false);
    _trackWidget->setPlayhead(_currentTimeMs);
    updateTimeLabel();
}

void TimelineWidget::setKeyframes(const std::vector<MotionSequence::Keyframe> &keyframes)
{
    _keyframes = keyframes;
    _trackWidget->setKeyframes(keyframes);
    _selectedKeyframeIndex = -1;
    _delKfBtn->setEnabled(false);
}

void TimelineWidget::setPlaying(bool playing)
{
    _playing = playing;
    if (_playing) {
        _playBtn->setText("⏸ Pause");
        _playBtn->setStyleSheet("background-color: #ffaa00; color: #111; font-weight: bold; padding: 4px 8px; border-radius: 4px;");
    } else {
        _playBtn->setText("▶ Play");
        _playBtn->setStyleSheet("background-color: #00ADB5; color: #ffffff; font-weight: bold; padding: 4px 8px; border-radius: 4px;");
    }
}

bool TimelineWidget::isLoop() const
{
    return _loopCheck->isChecked();
}

void TimelineWidget::setLoop(bool loop)
{
    _loopCheck->setChecked(loop);
}

bool TimelineWidget::isSyncToRobot() const
{
    return _syncCheck->isChecked();
}

void TimelineWidget::setSyncToRobot(bool sync)
{
    _syncCheck->setChecked(sync);
}

void TimelineWidget::onSliderValueChanged(int value)
{
    _currentTimeMs = value;
    _trackWidget->setPlayhead(_currentTimeMs);
    updateTimeLabel();
    emit timeChanged(_currentTimeMs);
}

void TimelineWidget::onPlayBtnClicked()
{
    setPlaying(!_playing);
    emit playToggled(_playing);
}

void TimelineWidget::onStopBtnClicked()
{
    setPlaying(false);
    setTime(0);
    emit stopClicked();
    emit timeChanged(0);
}

void TimelineWidget::onLoopCheckToggled(bool checked)
{
    emit loopToggled(checked);
}

void TimelineWidget::onSyncCheckToggled(bool checked)
{
    emit syncToRobotToggled(checked);
}

void TimelineWidget::onAddKeyframeBtnClicked()
{
    emit addKeyframeRequested(_currentTimeMs);
}

void TimelineWidget::onDeleteKeyframeBtnClicked()
{
    if (_selectedKeyframeIndex >= 0) {
        emit deleteKeyframeRequested(_selectedKeyframeIndex);
        _selectedKeyframeIndex = -1;
        _delKfBtn->setEnabled(false);
        _trackWidget->setSelectedKeyframe(-1);
    }
}

void TimelineWidget::onTrackKeyframeClicked(int keyframeIndex, int timeMs)
{
    _selectedKeyframeIndex = keyframeIndex;
    _delKfBtn->setEnabled(keyframeIndex >= 0);
    setTime(timeMs);
    emit keyframeSelected(keyframeIndex);
    emit timeChanged(timeMs);
}

void TimelineWidget::updateTimeLabel()
{
    int curSec = _currentTimeMs / 1000;
    int curMs = _currentTimeMs % 1000;
    int curMin = curSec / 60;
    curSec %= 60;

    int durSec = _durationMs / 1000;
    int durMs = _durationMs % 1000;
    int durMin = durSec / 60;
    durSec %= 60;

    _timeLabel->setText(QString("%1:%2.%3 / %4:%5.%6")
                        .arg(curMin, 2, 10, QChar('0'))
                        .arg(curSec, 2, 10, QChar('0'))
                        .arg(curMs, 3, 10, QChar('0'))
                        .arg(durMin, 2, 10, QChar('0'))
                        .arg(durSec, 2, 10, QChar('0'))
                        .arg(durMs, 3, 10, QChar('0')));
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
