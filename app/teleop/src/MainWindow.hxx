/*
 * MainWindow.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MAIN_WINDOW_HXX
#define ROBOHERO_MAIN_WINDOW_HXX

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <memory>
#include "PoseEstimator.hxx"
#include "PoseRetargeter.hxx"
#include "CameraThread.hxx"
#include "UrdfViewerWidget.hxx"
#include "MqttClient.hxx"

class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onFrameReady(const QImage &image, const PoseEstimator::PersonPose &pose);
    void onCameraError(const QString &msg);
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttError(const QString &msg);
    void onMqttMessageSent(int bytes);
    void onTelemetryReceived(const std::array<int, 17> &pwmValues,
                             const std::array<bool, 17> &validMask);

    void onTeleopToggleClicked();
    void onCenterClicked();
    void onRelaxClicked();
    void onStopClicked();
    void onSettingsClicked();
    void onSettingsApplied();
    void onTxTimerTimeout();

private:
    void setupUi();
    void setupMenusAndToolbars();
    void setupTelemetryTable();
    void applyTheme();
    void restartCamera();
    void connectMqtt();

    // Core components
    std::shared_ptr<PoseEstimator> _estimator;
    std::unique_ptr<PoseRetargeter> _retargeter;
    CameraThread *_cameraThread;
    UrdfViewerWidget *_urdfViewer;
    MqttClient *_mqttClient;
    SettingsDialog *_settingsDialog;

    // UI Widgets
    QLabel *_cameraViewLabel;
    QTableWidget *_telemetryTable;
    QPushButton *_teleopToggleBtn;
    QPushButton *_centerBtn;
    QPushButton *_relaxBtn;
    QPushButton *_stopBtn;
    QLabel *_mqttStatusBadge;
    QLabel *_fpsLabel;
    QLabel *_statusMessageLabel;

    // State & Timers
    bool _teleopActive;
    QTimer _txTimer;
    std::array<int, 17> _commandPwm;
    std::array<double, 17> _commandAngles;
    std::array<int, 17> _telemPwm;
    std::array<double, 17> _telemAngles;
    bool _hasPoseData;
    qint64 _lastPoseTimestamp;
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
