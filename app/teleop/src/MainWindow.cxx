/*
 * MainWindow.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MainWindow.hxx"
#include "SettingsDialog.hxx"
#include "TeleopConfig.hxx"
#include "UrdfLimits.hxx"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QIcon>
#include <QMessageBox>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , _cameraThread(nullptr)
    , _urdfViewer(nullptr)
    , _mqttClient(nullptr)
    , _settingsDialog(nullptr)
    , _cameraViewLabel(nullptr)
    , _telemetryTable(nullptr)
    , _teleopToggleBtn(nullptr)
    , _centerBtn(nullptr)
    , _relaxBtn(nullptr)
    , _stopBtn(nullptr)
    , _mqttStatusBadge(nullptr)
    , _fpsLabel(nullptr)
    , _statusMessageLabel(nullptr)
    , _teleopActive(false)
    , _hasPoseData(false)
    , _lastPoseTimestamp(0)
{
    // 1. Initialize Robot URDF Limits from embedded resource
    UrdfLimits::instance().init(":/urdf/robohero.urdf");

    for (int ch = 0; ch < 17; ++ch) {
        int center = UrdfLimits::instance().getCalibration(ch).center;
        double angle = UrdfLimits::instance().pwmToAngle(ch, center);
        _commandPwm[ch] = center;
        _commandAngles[ch] = angle;
        _telemPwm[ch] = center;
        _telemAngles[ch] = angle;
    }

    setWindowTitle("RoboHero Teleoperation Dashboard");
    setWindowIcon(QIcon(":/icons/robohero_head.png"));
    resize(1280, 800);

    TeleopConfig::instance().load();
    const auto &cfg = TeleopConfig::instance();

    // 2. Initialize AI Pose Estimator
    _estimator = std::make_shared<PoseEstimator>();
    _estimator->init(":/models/yolov8n-pose.onnx", cfg.ai.provider);

    // 3. Initialize Retargeter
    _retargeter = std::make_unique<PoseRetargeter>();

    // 4. Initialize MQTT Client
    _mqttClient = new MqttClient(this);
    connect(_mqttClient, &MqttClient::connected, this, &MainWindow::onMqttConnected,
            Qt::QueuedConnection);
    connect(_mqttClient, &MqttClient::disconnected, this, &MainWindow::onMqttDisconnected,
            Qt::QueuedConnection);
    connect(_mqttClient, &MqttClient::connectionError, this, &MainWindow::onMqttError,
            Qt::QueuedConnection);
    connect(_mqttClient, &MqttClient::messageSent, this, &MainWindow::onMqttMessageSent,
            Qt::QueuedConnection);
    connect(_mqttClient, &MqttClient::telemetryReceived, this, &MainWindow::onTelemetryReceived,
            Qt::QueuedConnection);

    // 5. Setup UI and Telemetry
    setupUi();
    setupMenusAndToolbars();
    setupTelemetryTable();
    applyTheme();

    // 6. Camera Thread setup
    _cameraThread = new CameraThread(_estimator, this);
    connect(_cameraThread, &CameraThread::frameReady, this, &MainWindow::onFrameReady);
    connect(_cameraThread, &CameraThread::cameraError, this, &MainWindow::onCameraError);
    restartCamera();

    // 7. Transmission Rate Timer
    int intervalMs = 1000 / (cfg.safety.txRateHz > 0 ? cfg.safety.txRateHz : 30);
    connect(&_txTimer, &QTimer::timeout, this, &MainWindow::onTxTimerTimeout);
    _txTimer.start(intervalMs);

    // 8. Auto-connect MQTT if enabled
    if (cfg.mqtt.autoConnect) {
        connectMqtt();
    }
}

MainWindow::~MainWindow()
{
    if (_cameraThread) {
        _cameraThread->stopCapture();
        _cameraThread->wait();
    }
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Vertical splitter between viewports and telemetry table
    auto *vSplitter = new QSplitter(Qt::Vertical, centralWidget);

    // Horizontal splitter between Camera feed and 3D URDF viewer
    auto *hSplitter = new QSplitter(Qt::Horizontal, vSplitter);

    // Left Viewport: Camera Feed
    auto *cameraContainer = new QWidget(hSplitter);
    auto *camLayout = new QVBoxLayout(cameraContainer);
    camLayout->setContentsMargins(4, 4, 4, 4);

    auto *camHeaderLayout = new QHBoxLayout();
    auto *camTitle = new QLabel("<b>RGB Camera Feed & Pose Estimation</b>", cameraContainer);
    _fpsLabel = new QLabel("FPS: --", cameraContainer);
    _fpsLabel->setStyleSheet("color: #00ADB5; font-weight: bold;");
    camHeaderLayout->addWidget(camTitle);
    camHeaderLayout->addStretch();
    camHeaderLayout->addWidget(_fpsLabel);
    camLayout->addLayout(camHeaderLayout);

    _cameraViewLabel = new QLabel("Waiting for camera feed...", cameraContainer);
    _cameraViewLabel->setAlignment(Qt::AlignCenter);
    _cameraViewLabel->setMinimumSize(480, 360);
    _cameraViewLabel->setStyleSheet("background-color: #1a1a1e; color: #e0e0e0; border: 1px solid #333; border-radius: 4px;");
    camLayout->addWidget(_cameraViewLabel);
    hSplitter->addWidget(cameraContainer);

    // Right Viewport: 3D URDF OpenGL Viewer
    auto *urdfContainer = new QWidget(hSplitter);
    auto *urdfLayout = new QVBoxLayout(urdfContainer);
    urdfLayout->setContentsMargins(4, 4, 4, 4);

    auto *urdfTitle = new QLabel("<b>3D URDF Digital Twin (Forward Kinematics)</b>", urdfContainer);
    urdfLayout->addWidget(urdfTitle);

    _urdfViewer = new UrdfViewerWidget(urdfContainer);
    _urdfViewer->setMinimumSize(480, 360);
    urdfLayout->addWidget(_urdfViewer);
    hSplitter->addWidget(urdfContainer);

    vSplitter->addWidget(hSplitter);

    // Bottom Container: Telemetry table
    auto *telemetryContainer = new QWidget(vSplitter);
    auto *telemLayout = new QVBoxLayout(telemetryContainer);
    telemLayout->setContentsMargins(4, 4, 4, 4);

    auto *telemTitle = new QLabel("<b>17-Channel Servo Telemetry & Safety Limit Monitor</b>", telemetryContainer);
    telemLayout->addWidget(telemTitle);

    _telemetryTable = new QTableWidget(telemetryContainer);
    telemLayout->addWidget(_telemetryTable);
    vSplitter->addWidget(telemetryContainer);

    // Splitter initial sizes
    vSplitter->setSizes({500, 240});
    hSplitter->setSizes({500, 500});

    mainLayout->addWidget(vSplitter);

    // Status bar labels
    _mqttStatusBadge = new QLabel("MQTT: Disconnected", this);
    _mqttStatusBadge->setStyleSheet("color: #ff5555; padding-right: 15px; font-weight: bold;");
    _statusMessageLabel = new QLabel("Ready. Teleoperation is STANDBY.", this);
    _statusMessageLabel->setObjectName("statusMessage");
    _statusMessageLabel->setStyleSheet("color: #e0e0e0;");

    statusBar()->addPermanentWidget(_mqttStatusBadge);
    statusBar()->addWidget(_statusMessageLabel);
}

void MainWindow::setupMenusAndToolbars()
{
    // Menu Bar
    auto *fileMenu = menuBar()->addMenu("&File");
    auto *settingsAction = fileMenu->addAction("&Settings...");
    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction("E&xit");

    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto *robotMenu = menuBar()->addMenu("&Robot");
    auto *centerAction = robotMenu->addAction("&Center All Servos");
    auto *relaxAction = robotMenu->addAction("&Relax (Torque Off)");
    auto *stopAction = robotMenu->addAction("&Emergency Stop");

    connect(centerAction, &QAction::triggered, this, &MainWindow::onCenterClicked);
    connect(relaxAction, &QAction::triggered, this, &MainWindow::onRelaxClicked);
    connect(stopAction, &QAction::triggered, this, &MainWindow::onStopClicked);

    auto *helpMenu = menuBar()->addMenu("&Help");
    auto *aboutAction = helpMenu->addAction("&About RoboHero Teleop");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About RoboHero Teleop",
                           "<h3>RoboHero Desktop Teleoperation</h3>"
                           "<p>Real-time human pose teleoperation with YOLOv8, URDF kinematics, and MQTT control.</p>"
                           "<p>Copyright &copy; 2026, Charles Chiou</p>");
    });

    // Tool Bar
    auto *toolBar = addToolBar("Controls");
    toolBar->setMovable(false);

    _teleopToggleBtn = new QPushButton("Teleoperation: STANDBY", this);
    _teleopToggleBtn->setCheckable(true);
    _teleopToggleBtn->setMinimumHeight(36);
    _teleopToggleBtn->setStyleSheet("font-weight: bold; padding: 6px 16px; background-color: #444; color: #fff; border-radius: 4px;");
    connect(_teleopToggleBtn, &QPushButton::clicked, this, &MainWindow::onTeleopToggleClicked);
    toolBar->addWidget(_teleopToggleBtn);

    toolBar->addSeparator();

    _centerBtn = new QPushButton("Center (Neutral)", this);
    _centerBtn->setStyleSheet("padding: 6px 12px;");
    connect(_centerBtn, &QPushButton::clicked, this, &MainWindow::onCenterClicked);
    toolBar->addWidget(_centerBtn);

    _relaxBtn = new QPushButton("Relax (Torque Off)", this);
    _relaxBtn->setStyleSheet("padding: 6px 12px;");
    connect(_relaxBtn, &QPushButton::clicked, this, &MainWindow::onRelaxClicked);
    toolBar->addWidget(_relaxBtn);

    _stopBtn = new QPushButton("EMERGENCY STOP", this);
    _stopBtn->setStyleSheet("background-color: #8b0000; color: #fff; font-weight: bold; padding: 6px 12px; border-radius: 4px;");
    connect(_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    toolBar->addWidget(_stopBtn);

    toolBar->addSeparator();

    auto *settingsBtn = new QPushButton("Settings", this);
    settingsBtn->setStyleSheet("padding: 6px 12px;");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    toolBar->addWidget(settingsBtn);
}

void MainWindow::setupTelemetryTable()
{
    _telemetryTable->setColumnCount(6);
    _telemetryTable->setRowCount(17);
    _telemetryTable->setHorizontalHeaderLabels(
        {"Channel", "Joint Name", "Target (deg)", "PWM", "Safe Lower (deg)", "Safe Upper (deg)"});

    const auto &limits = UrdfLimits::instance().getAllLimits();

    for (int ch = 0; ch < 17; ++ch) {
        auto chItem = new QTableWidgetItem(QString::number(ch));
        chItem->setTextAlignment(Qt::AlignCenter);
        _telemetryTable->setItem(ch, 0, chItem);

        std::string jName = "channel_" + std::to_string(ch);
        double lDeg = -90.0;
        double uDeg = 90.0;

        auto it = limits.find(ch);
        if (it != limits.end()) {
            jName = it->second.name;
            lDeg = it->second.lower * 180.0 / M_PI;
            uDeg = it->second.upper * 180.0 / M_PI;
        }

        auto nameItem = new QTableWidgetItem(QString::fromStdString(jName));
        _telemetryTable->setItem(ch, 1, nameItem);

        auto degItem = new QTableWidgetItem("0.0");
        degItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        _telemetryTable->setItem(ch, 2, degItem);

        int defaultPwm = UrdfLimits::instance().getCalibration(ch).center;
        auto pwmItem = new QTableWidgetItem(QString::number(defaultPwm));
        pwmItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        _telemetryTable->setItem(ch, 3, pwmItem);

        auto lowItem = new QTableWidgetItem(QString::number(lDeg, 'f', 1));
        lowItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        _telemetryTable->setItem(ch, 4, lowItem);

        auto upItem = new QTableWidgetItem(QString::number(uDeg, 'f', 1));
        upItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        _telemetryTable->setItem(ch, 5, upItem);
    }

    _telemetryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    _telemetryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _telemetryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void MainWindow::applyTheme()
{
    const auto &cfg = TeleopConfig::instance();
    if (cfg.view.darkTheme) {
        setStyleSheet(
            "QMainWindow { background-color: #1e1e24; color: #e0e0e0; }"
            "QWidget { color: #e0e0e0; }"
            "QLabel { color: #e0e0e0; }"
            "QToolBar { background-color: #26262e; color: #e0e0e0; border-bottom: 1px solid #333; spacing: 8px; padding: 4px; }"
            "QToolBar QLabel { color: #e0e0e0; }"
            "QMenuBar { background-color: #26262e; color: #e0e0e0; border-bottom: 1px solid #333; }"
            "QMenuBar::item { color: #e0e0e0; }"
            "QMenuBar::item:selected { background-color: #383842; color: #ffffff; }"
            "QMenu { background-color: #26262e; color: #e0e0e0; border: 1px solid #444; }"
            "QMenu::item { color: #e0e0e0; }"
            "QMenu::item:selected { background-color: #00ADB5; color: #ffffff; }"
            "QTableWidget { background-color: #16161a; alternate-background-color: #1f1f24; color: #ddd; gridline-color: #333; border: 1px solid #333; }"
            "QHeaderView::section { background-color: #282830; color: #e0e0e0; padding: 4px; border: 1px solid #333; font-weight: bold; }"
            "QStatusBar { background-color: #1a1a1e; color: #e0e0e0; }"
            "QStatusBar QLabel#statusMessage { color: #e0e0e0; background: transparent; }"
            "QPushButton { background-color: #33333d; color: #fff; border: 1px solid #4a4a55; border-radius: 4px; padding: 5px 12px; }"
            "QPushButton:hover { background-color: #3d3d4a; border-color: #00ADB5; }"
            "QPushButton:pressed { background-color: #222228; }"
            "QCheckBox { color: #e0e0e0; }"
            "QRadioButton { color: #e0e0e0; }"
            "QGroupBox { color: #e0e0e0; border: 1px solid #444; margin-top: 8px; }"
            "QGroupBox::title { color: #e0e0e0; }"
            "QTabWidget::pane { border: 1px solid #444; background-color: #1e1e24; }"
            "QTabBar::tab { color: #e0e0e0; background-color: #26262e; padding: 6px 12px; border: 1px solid #444; }"
            "QTabBar::tab:selected { color: #ffffff; background-color: #383842; }"
            "QLineEdit { color: #e0e0e0; background-color: #16161a; border: 1px solid #444; padding: 3px; }"
            "QSpinBox, QDoubleSpinBox, QAbstractSpinBox { color: #e0e0e0; background-color: #16161a; border: 1px solid #444; }"
            "QComboBox { color: #e0e0e0; background-color: #16161a; border: 1px solid #444; }"
            "QComboBox QAbstractItemView { color: #e0e0e0; background-color: #26262e; }"
            "QDialog { background-color: #1e1e24; color: #e0e0e0; }"
            "QMessageBox { background-color: #26262e; color: #e0e0e0; }"
            "QMessageBox QLabel { color: #e0e0e0; }"
            "QSplitter::handle { background-color: #333; }"
        );
        _statusMessageLabel->setStyleSheet("color: #e0e0e0;");
        _cameraViewLabel->setStyleSheet("background-color: #1a1a1e; color: #e0e0e0; border: 1px solid #333; border-radius: 4px;");
    } else {
        setStyleSheet(
            "QStatusBar { color: #222222; }"
            "QStatusBar QLabel#statusMessage { color: #222222; background: transparent; }"
        );
        _statusMessageLabel->setStyleSheet("color: #222222;");
    }
}

void MainWindow::restartCamera()
{
    const auto &cfg = TeleopConfig::instance();
    if (_cameraThread) {
        _cameraThread->stopCapture();
        _cameraThread->wait();
        _cameraThread->setCameraConfig(cfg.camera.deviceIndex, cfg.camera.width,
                                       cfg.camera.height, cfg.camera.targetFps,
                                       cfg.camera.mirrorVideo);
        _cameraThread->setInferenceParams(cfg.ai.confThreshold, cfg.ai.kptThreshold,
                                          cfg.ai.targetSelection, cfg.view.showSkeleton);
        _cameraThread->start();
    }
}

void MainWindow::connectMqtt()
{
    const auto &cfg = TeleopConfig::instance();
    _mqttClient->disconnectFromBroker();
    _mqttClient->setRobotId(cfg.mqtt.robotId);
    _mqttStatusBadge->setText(QString("MQTT: Connecting to %1:%2...").arg(QString::fromStdString(cfg.mqtt.host)).arg(cfg.mqtt.port));
    _mqttStatusBadge->setStyleSheet("color: #ffaa00; padding-right: 15px; font-weight: bold;");

    _mqttClient->connectToBroker(cfg.mqtt.host, cfg.mqtt.port,
                                 cfg.mqtt.username, cfg.mqtt.password,
                                 cfg.mqtt.keepalive);
}

void MainWindow::onFrameReady(const QImage &image, const PoseEstimator::PersonPose &pose)
{
    // Update camera view
    _cameraViewLabel->setPixmap(
        QPixmap::fromImage(image).scaled(_cameraViewLabel->size(),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));

    // Update FPS
    if (_cameraThread) {
        _fpsLabel->setText(QString("FPS: %1").arg(_cameraThread->getActualFps(), 0, 'f', 1));
    }

    if (pose.valid) {
        const auto &cfg = TeleopConfig::instance();
        auto result = _retargeter->process(pose, cfg.camera.mirrorVideo,
                                           cfg.retarget.smoothingAlpha,
                                           cfg.retarget.deadbandDeg,
                                           cfg.safety.safetyMarginDeg);

        if (result.valid) {
            _commandAngles = result.jointAnglesRad;
            _commandPwm = result.servoPwm;
            _hasPoseData = true;
            _lastPoseTimestamp = QDateTime::currentMSecsSinceEpoch();
        }
    }
}

void MainWindow::onCameraError(const QString &msg)
{
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::onMqttConnected()
{
    const auto &cfg = TeleopConfig::instance();
    _mqttStatusBadge->setText(QString("MQTT: Connected (%1:%2)").arg(QString::fromStdString(cfg.mqtt.host)).arg(cfg.mqtt.port));
    _mqttStatusBadge->setStyleSheet("color: #00ff88; padding-right: 15px; font-weight: bold;");
    _statusMessageLabel->setText("MQTT broker connected. Ready for teleoperation.");
}

void MainWindow::onMqttDisconnected()
{
    _mqttStatusBadge->setText("MQTT: Disconnected");
    _mqttStatusBadge->setStyleSheet("color: #ff5555; padding-right: 15px; font-weight: bold;");
    _statusMessageLabel->setText("MQTT broker disconnected.");
}

void MainWindow::onMqttError(const QString &msg)
{
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::onMqttMessageSent(int bytes)
{
    (void) bytes;
}

void MainWindow::onTelemetryReceived(const std::array<int, 17> &pwmValues,
                                     const std::array<bool, 17> &validMask)
{
    bool updated = false;
    for (int ch = 0; ch < 17; ++ch) {
        if (validMask[ch]) {
            _telemPwm[ch] = pwmValues[ch];
            _telemAngles[ch] = UrdfLimits::instance().pwmToAngle(ch, pwmValues[ch]);
            updated = true;

            double deg = _telemAngles[ch] * 180.0 / M_PI;
            auto degItem = _telemetryTable->item(ch, 2);
            if (degItem) {
                degItem->setText(QString::number(deg, 'f', 1));
            }

            auto pwmItem = _telemetryTable->item(ch, 3);
            if (pwmItem) {
                pwmItem->setText(QString::number(_telemPwm[ch]));
            }
        }
    }

    if (updated && _urdfViewer) {
        _urdfViewer->setJointAngles(_telemAngles);
        _urdfViewer->setJointPwm(_telemPwm);
    }
}

void MainWindow::onTeleopToggleClicked()
{
    _teleopActive = _teleopToggleBtn->isChecked();

    if (_teleopActive) {
        _teleopToggleBtn->setText("Teleoperation: ACTIVE (STREAMING)");
        _teleopToggleBtn->setStyleSheet("font-weight: bold; padding: 6px 16px; background-color: #008855; color: #fff; border-radius: 4px;");
        _mqttClient->resetPwmTxCache();
        _statusMessageLabel->setText("Teleoperation ACTIVE. Streaming motion commands to robot.");
    } else {
        _teleopToggleBtn->setText("Teleoperation: STANDBY");
        _teleopToggleBtn->setStyleSheet("font-weight: bold; padding: 6px 16px; background-color: #444; color: #fff; border-radius: 4px;");
        _statusMessageLabel->setText("Teleoperation STANDBY. Motion transmission paused.");
    }
}

void MainWindow::onCenterClicked()
{
    if (!_mqttClient->sendCenter(_mqttClient->controlTopic())) {
        _statusMessageLabel->setText(_mqttClient->lastError());
        return;
    }
    _statusMessageLabel->setText("Sent CENTER command to robot.");
}

void MainWindow::onRelaxClicked()
{
    if (!_mqttClient->sendRelax(_mqttClient->controlTopic())) {
        _statusMessageLabel->setText(_mqttClient->lastError());
        return;
    }
    _statusMessageLabel->setText("Sent RELAX (torque off) command to robot.");
}

void MainWindow::onStopClicked()
{
    // Disengage teleoperation immediately
    if (_teleopActive) {
        _teleopToggleBtn->setChecked(false);
        onTeleopToggleClicked();
    }

    if (!_mqttClient->sendStop(_mqttClient->controlTopic())) {
        _statusMessageLabel->setText(_mqttClient->lastError());
        return;
    }
    _statusMessageLabel->setText("EMERGENCY STOP TRIGGERED!");
}

void MainWindow::onSettingsClicked()
{
    if (!_settingsDialog) {
        _settingsDialog = new SettingsDialog(this);
        connect(_settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::onSettingsApplied);
    }
    _settingsDialog->loadCurrentConfig();
    _settingsDialog->exec();
}

void MainWindow::onSettingsApplied()
{
    const auto &cfg = TeleopConfig::instance();

    // Reconfigure transmission timer rate
    int intervalMs = 1000 / (cfg.safety.txRateHz > 0 ? cfg.safety.txRateHz : 30);
    _txTimer.setInterval(intervalMs);

    // Reconfigure camera
    restartCamera();

    // Reapply theme
    applyTheme();

    // Reconnect to updated MQTT broker address
    connectMqtt();

    statusBar()->showMessage(QString("Settings applied. Connecting to MQTT broker at %1:%2...")
                                 .arg(QString::fromStdString(cfg.mqtt.host))
                                 .arg(cfg.mqtt.port), 3000);
}

void MainWindow::onTxTimerTimeout()
{
    if (!_teleopActive || !_mqttClient->isConnected() || !_hasPoseData) {
        return;
    }

    const auto &cfg = TeleopConfig::instance();
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - _lastPoseTimestamp > cfg.safety.watchdogTimeoutMs) {
        return;
    }

    if (!_mqttClient->sendPwm(_commandPwm, _mqttClient->controlTopic())) {
        _statusMessageLabel->setText(_mqttClient->lastError());
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
