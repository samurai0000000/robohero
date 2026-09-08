/*
 * MainWindow.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_MAIN_WINDOW_HXX
#define ROBOHERO_MOTION_MAIN_WINDOW_HXX

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QSplitter>
#include <QTabWidget>
#include <array>
#include <memory>

#include "UrdfViewerWidget.hxx"
#include "TimelineWidget.hxx"
#include "MotionLibraryWidget.hxx"
#include "MotionPlayer.hxx"
#include "MqttClient.hxx"

class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // Playback and timeline
    void onPlayheadChanged(int timeMs);
    void onPoseUpdated(const std::array<double, 17> &angles, const std::array<int, 17> &pwm);
    void onTimelineTimeChanged(int timeMs);
    void onPlayToggled(bool playing);
    void onStopClicked();
    void onLoopToggled(bool loop);
    void onSyncToRobotToggled(bool sync);
    void onAddKeyframeRequested(int timeMs);
    void onDeleteKeyframeRequested(int index);
    void onKeyframeSelected(int index);
    void onTimelineDurationChanged(int durationMs);

    // Motion Library
    void onMotionLoadRequested(const QString &filePath);
    void onMotionPlayOnRobotRequested(const QString &filePath);
    void onNewMotionRequested();
    void onSaveMotionRequested();

    // Parametric Gait & Gestures
    void onGenerateGaitClicked();
    void onGenerateGestureClicked(const QString &gestureType);

    // Sliders
    void onSliderValueChanged(int channel, int value);

    // Viewport & Mode
    void onSplitViewToggled();
    void onViewPresetClicked(const QString &preset);

    // MQTT & Hardware controls
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttError(const QString &msg);
    void onTelemetryReceived(const std::array<int, 17> &pwmValues,
                             const std::array<bool, 17> &validMask);
    void onCenterClicked();
    void onRelaxClicked();
    void onStopHardwareClicked();
    void onSettingsClicked();
    void onSettingsApplied();

private:
    void setupUi();
    void setupToolbar();
    void setupStatusBar();
    void setupJointSliders(QWidget *parent);
    void setupGeneratorTab(QWidget *parent);
    void applyTheme();
    void updateJointSliders(const std::array<double, 17> &angles, const std::array<int, 17> &pwm);
    void connectMqtt();

    // Core engines
    UrdfViewerWidget *_viewerMain;
    UrdfViewerWidget *_viewerTelem;
    TimelineWidget *_timelineWidget;
    MotionLibraryWidget *_libraryWidget;
    MotionPlayer *_player;
    MqttClient *_mqttClient;
    SettingsDialog *_settingsDialog;

    // Viewport split
    bool _splitViewMode;
    QWidget *_telemContainer;
    QPushButton *_splitViewBtn;
    QCheckBox *_ghostCheckBox;

    // Joint Sliders
    struct JointControl
    {
        QLabel *nameLabel;
        QLabel *valLabel;
        QSlider *slider;
    };
    std::array<JointControl, 17> _jointControls;
    bool _updatingSlidersFromEngine;

    // Gait Generator Controls
    QComboBox *_gaitModeCombo;
    QDoubleSpinBox *_strideSpin;
    QDoubleSpinBox *_stepHeightSpin;
    QDoubleSpinBox *_swaySpin;
    QDoubleSpinBox *_turnSpin;
    QSpinBox *_durationSpin;
    QDoubleSpinBox *_torsoPitchSpin;
    QDoubleSpinBox *_armSwingSpin;
    QComboBox *_armModeCombo;
    QSpinBox *_cycleSpin;
    QPushButton *_genGaitBtn;

    // Status Badges
    QLabel *_statusMessageLabel;
    QLabel *_mqttStatusBadge;
    QLabel *_comStabilityBadge;

    // Current Motion Data
    MotionSequence _currentSequence;
    QString _currentFilePath;

    // Telemetry state
    std::array<int, 17> _telemPwm;
    std::array<double, 17> _telemAngles;
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
