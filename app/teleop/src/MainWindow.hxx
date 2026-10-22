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
#include "SelfUpdateDialog.hxx"
#include "Version.hxx"

class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(
        std::shared_ptr<selfupdate::SelfUpdateController> updateController,
        QWidget *parent = nullptr);
    ~MainWindow() override;

    static selfupdate::AppConfig defaultSelfUpdateConfig();

    selfupdate::SelfUpdateController *selfUpdateController() const;
    std::shared_ptr<selfupdate::SelfUpdateController> selfUpdateControllerShared() const;
    void setSelfUpdateController(
        std::shared_ptr<selfupdate::SelfUpdateController> controller);
    const selfupdate::UpdateState &currentSelfUpdateState() const;
    void checkForAppUpdates(bool isManual = false);

    QString updateStatusBadgeText() const;
    bool isUpdateStatusBadgeVisible() const;

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
    void onCheckForUpdatesClicked();
    void onAboutClicked();

private:
    void setupUi();
    void setupMenusAndToolbars();
    void setupTelemetryTable();
    void applyTheme();
    void restartCamera();
    void connectMqtt();
    void updateAiProviderBadges();
    void bindSelfUpdateController();
    void onSelfUpdateStateChanged(const selfupdate::UpdateState &state);

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
    QLabel *_aiStatusBadge;
    QLabel *_aiProviderBadge;
    QLabel *_fpsLabel;
    QLabel *_statusMessageLabel;
    QPushButton *_updateStatusBadge;

    // Self-Update Controller
    std::shared_ptr<selfupdate::SelfUpdateController> _updateController;
    selfupdate::UpdateState _currentUpdateState;
    bool _startupCheckPending;
    bool _teleopActive;
    QTimer _txTimer;
    std::array<int, 17> _commandPwm;
    std::array<double, 17> _commandAngles;
    std::array<bool, 17> _commandMask;
    std::array<int, 17> _telemPwm;
    std::array<double, 17> _telemAngles;
    bool _hasPoseData;
    qint64 _lastPoseTimestamp;

    // Cached applied settings for change detection
    std::string _lastAppliedAiProvider;
    std::string _lastAppliedModelSource;
    std::string _lastAppliedCustomModelPath;
    std::string _lastAppliedMqttHost;
    int _lastAppliedMqttPort;
    std::string _lastAppliedMqttUser;
    std::string _lastAppliedMqttPass;
    std::string _lastAppliedMqttRobotId;
    int _lastAppliedMqttKeepalive;
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
