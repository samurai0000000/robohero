/*
 * SettingsDialog.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "SettingsDialog.hxx"
#include "TeleopConfig.hxx"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("RoboHero Teleop Settings");
    resize(540, 420);
    setupUi();
    loadCurrentConfig();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    _tabWidget = new QTabWidget(this);

    // 1. Camera Tab
    auto *camTab = new QWidget();
    auto *camForm = new QFormLayout(camTab);
    _cameraDeviceSpin = new QSpinBox(camTab);
    _cameraDeviceSpin->setRange(0, 10);

    _resolutionCombo = new QComboBox(camTab);
    _resolutionCombo->addItem("640 x 480 (VGA)", "640x480");
    _resolutionCombo->addItem("1280 x 720 (HD)", "1280x720");
    _resolutionCombo->addItem("1920 x 1080 (FHD)", "1920x1080");

    _cameraFpsSpin = new QSpinBox(camTab);
    _cameraFpsSpin->setRange(10, 120);
    _cameraFpsSpin->setSuffix(" fps");

    _cameraMirrorCheck = new QCheckBox("Mirror camera video horizontally (recommended)", camTab);

    camForm->addRow("Camera Device Index:", _cameraDeviceSpin);
    camForm->addRow("Resolution:", _resolutionCombo);
    camForm->addRow("Target Frame Rate:", _cameraFpsSpin);
    camForm->addRow("", _cameraMirrorCheck);
    _tabWidget->addTab(camTab, "Camera");

    // 2. AI Inference Tab
    auto *aiTab = new QWidget();
    auto *aiForm = new QFormLayout(aiTab);
    _aiProviderCombo = new QComboBox(aiTab);
    _aiProviderCombo->addItem("Auto (Prefer DirectML / CUDA, fallback CPU)", "auto");
    _aiProviderCombo->addItem("DirectML (Windows DirectX 12)", "directml");
    _aiProviderCombo->addItem("CUDA (NVIDIA GPU)", "cuda");
    _aiProviderCombo->addItem("CPU (Portable)", "cpu");

    _confThresholdSpin = new QDoubleSpinBox(aiTab);
    _confThresholdSpin->setRange(0.05, 0.99);
    _confThresholdSpin->setSingleStep(0.05);

    _kptThresholdSpin = new QDoubleSpinBox(aiTab);
    _kptThresholdSpin->setRange(0.05, 0.99);
    _kptThresholdSpin->setSingleStep(0.05);

    _targetSelectionCombo = new QComboBox(aiTab);
    _targetSelectionCombo->addItem("Largest Bounding Box", "largest");
    _targetSelectionCombo->addItem("Highest Confidence", "confidence");

    _activeProviderLabel = new QLabel("Active Backend: Initializing...", aiTab);
    _activeProviderLabel->setStyleSheet("color: #00ADB5; font-weight: bold; font-size: 11px;");

    auto *modelInfoLabel = new QLabel("Model: Embedded YOLOv8n-pose (640x640 ONNX)", aiTab);
    modelInfoLabel->setStyleSheet("color: #88a; font-style: italic;");

    aiForm->addRow("Execution Provider:", _aiProviderCombo);
    aiForm->addRow("", _activeProviderLabel);
    aiForm->addRow("Person Confidence Thresh:", _confThresholdSpin);
    aiForm->addRow("Keypoint Confidence Thresh:", _kptThresholdSpin);
    aiForm->addRow("Target Person Selection:", _targetSelectionCombo);
    aiForm->addRow("", modelInfoLabel);
    _tabWidget->addTab(aiTab, "AI Inference");

    // 3. Motion & Retargeting Tab
    auto *retargetTab = new QWidget();
    auto *retargetForm = new QFormLayout(retargetTab);
    _smoothingAlphaSpin = new QDoubleSpinBox(retargetTab);
    _smoothingAlphaSpin->setRange(0.01, 1.0);
    _smoothingAlphaSpin->setSingleStep(0.05);
    _smoothingAlphaSpin->setToolTip("0.0 = frozen, 1.0 = instant/raw (no filter)");

    _deadbandDegSpin = new QDoubleSpinBox(retargetTab);
    _deadbandDegSpin->setRange(0.0, 10.0);
    _deadbandDegSpin->setSingleStep(0.5);
    _deadbandDegSpin->setSuffix(" deg");

    _maxVelocitySpin = new QDoubleSpinBox(retargetTab);
    _maxVelocitySpin->setRange(10.0, 500.0);
    _maxVelocitySpin->setSingleStep(10.0);
    _maxVelocitySpin->setSuffix(" deg/s");

    _retargetModeCombo = new QComboBox(retargetTab);
    _retargetModeCombo->addItem("Upper Body (Head + Arms)", "upper_body");
    _retargetModeCombo->addItem("Full Body (Arms + Legs)", "full_body");

    _stancePresetCombo = new QComboBox(retargetTab);
    _stancePresetCombo->addItem("Neutral (Standing)", "neutral");
    _stancePresetCombo->addItem("Combat (Ready)", "combat");

    retargetForm->addRow("Smoothing Factor (Alpha):", _smoothingAlphaSpin);
    retargetForm->addRow("Deadband Threshold:", _deadbandDegSpin);
    retargetForm->addRow("Max Joint Speed:", _maxVelocitySpin);
    retargetForm->addRow("Retargeting Mode:", _retargetModeCombo);
    retargetForm->addRow("Stance Preset:", _stancePresetCombo);
    _tabWidget->addTab(retargetTab, "Retargeting");

    // 4. Safety & Watchdog Tab
    auto *safetyTab = new QWidget();
    auto *safetyForm = new QFormLayout(safetyTab);
    _safetyMarginSpin = new QDoubleSpinBox(safetyTab);
    _safetyMarginSpin->setRange(0.0, 15.0);
    _safetyMarginSpin->setSingleStep(0.5);
    _safetyMarginSpin->setSuffix(" deg");

    _txRateHzSpin = new QSpinBox(safetyTab);
    _txRateHzSpin->setRange(5, 60);
    _txRateHzSpin->setSuffix(" Hz");

    _watchdogTimeoutSpin = new QSpinBox(safetyTab);
    _watchdogTimeoutSpin->setRange(100, 3000);
    _watchdogTimeoutSpin->setSingleStep(50);
    _watchdogTimeoutSpin->setSuffix(" ms");

    safetyForm->addRow("URDF Safety Margin:", _safetyMarginSpin);
    safetyForm->addRow("MQTT Publish Rate:", _txRateHzSpin);
    safetyForm->addRow("Watchdog Timeout:", _watchdogTimeoutSpin);
    _tabWidget->addTab(safetyTab, "Safety");

    // 5. MQTT Network Tab
    auto *mqttTab = new QWidget();
    auto *mqttForm = new QFormLayout(mqttTab);
    _mqttHostEdit = new QLineEdit(mqttTab);
    _mqttPortSpin = new QSpinBox(mqttTab);
    _mqttPortSpin->setRange(1, 65535);

    _mqttUsernameEdit = new QLineEdit(mqttTab);
    _mqttPasswordEdit = new QLineEdit(mqttTab);
    _mqttPasswordEdit->setEchoMode(QLineEdit::Password);

    _mqttRobotIdEdit = new QLineEdit(mqttTab);
    _mqttRobotIdEdit->setPlaceholderText("e.g. TTR-ee40, robohero");
    _mqttKeepaliveSpin = new QSpinBox(mqttTab);
    _mqttKeepaliveSpin->setRange(5, 300);
    _mqttKeepaliveSpin->setSuffix(" s");

    _mqttAutoConnectCheck = new QCheckBox("Auto-connect to broker on startup", mqttTab);

    mqttForm->addRow("Broker Address:", _mqttHostEdit);
    mqttForm->addRow("Broker Port:", _mqttPortSpin);
    mqttForm->addRow("Username (optional):", _mqttUsernameEdit);
    mqttForm->addRow("Password (optional):", _mqttPasswordEdit);
    mqttForm->addRow("Robot ID:", _mqttRobotIdEdit);
    mqttForm->addRow("Keepalive Interval:", _mqttKeepaliveSpin);
    mqttForm->addRow("", _mqttAutoConnectCheck);
    _tabWidget->addTab(mqttTab, "MQTT Network");

    // 6. View Tab
    auto *viewTab = new QWidget();
    auto *viewForm = new QFormLayout(viewTab);
    _viewMeshesCheck = new QCheckBox("Render 3D Robot Links", viewTab);
    _viewSkeletonCheck = new QCheckBox("Draw Pose Skeleton on Camera View", viewTab);
    _viewGridCheck = new QCheckBox("Display 3D Coordinate Floor Grid", viewTab);
    _viewDarkThemeCheck = new QCheckBox("Dark Application Palette", viewTab);

    viewForm->addRow("", _viewMeshesCheck);
    viewForm->addRow("", _viewSkeletonCheck);
    viewForm->addRow("", _viewGridCheck);
    viewForm->addRow("", _viewDarkThemeCheck);
    _tabWidget->addTab(viewTab, "View");

    mainLayout->addWidget(_tabWidget);

    // Bottom action buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    _okBtn = new QPushButton("OK", this);
    _cancelBtn = new QPushButton("Cancel", this);
    _applyBtn = new QPushButton("Apply", this);

    _okBtn->setDefault(true);
    btnLayout->addWidget(_okBtn);
    btnLayout->addWidget(_cancelBtn);
    btnLayout->addWidget(_applyBtn);
    mainLayout->addLayout(btnLayout);

    connect(_okBtn, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    connect(_cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    connect(_applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
}

void SettingsDialog::loadCurrentConfig()
{
    const auto &cfg = TeleopConfig::instance();

    _cameraDeviceSpin->setValue(cfg.camera.deviceIndex);
    QString resStr = QString("%1x%2").arg(cfg.camera.width).arg(cfg.camera.height);
    int resIdx = _resolutionCombo->findData(resStr);
    if (resIdx >= 0) {
        _resolutionCombo->setCurrentIndex(resIdx);
    }
    _cameraFpsSpin->setValue(cfg.camera.targetFps);
    _cameraMirrorCheck->setChecked(cfg.camera.mirrorVideo);

    int provIdx = _aiProviderCombo->findData(QString::fromStdString(cfg.ai.provider));
    if (provIdx >= 0) {
        _aiProviderCombo->setCurrentIndex(provIdx);
    }
    _confThresholdSpin->setValue(cfg.ai.confThreshold);
    _kptThresholdSpin->setValue(cfg.ai.kptThreshold);
    int targetIdx = _targetSelectionCombo->findData(QString::fromStdString(cfg.ai.targetSelection));
    if (targetIdx >= 0) {
        _targetSelectionCombo->setCurrentIndex(targetIdx);
    }

    _smoothingAlphaSpin->setValue(cfg.retarget.smoothingAlpha);
    _deadbandDegSpin->setValue(cfg.retarget.deadbandDeg);
    _maxVelocitySpin->setValue(cfg.retarget.maxVelocityDegS);
    int modeIdx = _retargetModeCombo->findData(QString::fromStdString(cfg.retarget.mode));
    if (modeIdx >= 0) {
        _retargetModeCombo->setCurrentIndex(modeIdx);
    }
    int stanceIdx = _stancePresetCombo->findData(QString::fromStdString(cfg.retarget.stancePreset));
    if (stanceIdx >= 0) {
        _stancePresetCombo->setCurrentIndex(stanceIdx);
    }

    _safetyMarginSpin->setValue(cfg.safety.safetyMarginDeg);
    _txRateHzSpin->setValue(cfg.safety.txRateHz);
    _watchdogTimeoutSpin->setValue(cfg.safety.watchdogTimeoutMs);

    _mqttHostEdit->setText(QString::fromStdString(cfg.mqtt.host));
    _mqttPortSpin->setValue(cfg.mqtt.port);
    _mqttUsernameEdit->setText(QString::fromStdString(cfg.mqtt.username));
    _mqttPasswordEdit->setText(QString::fromStdString(cfg.mqtt.password));
    _mqttRobotIdEdit->setText(QString::fromStdString(cfg.mqtt.robotId));
    _mqttKeepaliveSpin->setValue(cfg.mqtt.keepalive);
    _mqttAutoConnectCheck->setChecked(cfg.mqtt.autoConnect);

    _viewMeshesCheck->setChecked(cfg.view.showRobotMeshes);
    _viewSkeletonCheck->setChecked(cfg.view.showSkeleton);
    _viewGridCheck->setChecked(cfg.view.showGrid);
    _viewDarkThemeCheck->setChecked(cfg.view.darkTheme);
}

void SettingsDialog::applyToConfig()
{
    auto &cfg = TeleopConfig::instance();

    cfg.camera.deviceIndex = _cameraDeviceSpin->value();
    QString resData = _resolutionCombo->currentData().toString();
    QStringList parts = resData.split('x');
    if (parts.size() == 2) {
        cfg.camera.width = parts[0].toInt();
        cfg.camera.height = parts[1].toInt();
    }
    cfg.camera.targetFps = _cameraFpsSpin->value();
    cfg.camera.mirrorVideo = _cameraMirrorCheck->isChecked();

    cfg.ai.provider = _aiProviderCombo->currentData().toString().toStdString();
    cfg.ai.confThreshold = static_cast<float>(_confThresholdSpin->value());
    cfg.ai.kptThreshold = static_cast<float>(_kptThresholdSpin->value());
    cfg.ai.targetSelection = _targetSelectionCombo->currentData().toString().toStdString();

    cfg.retarget.smoothingAlpha = static_cast<float>(_smoothingAlphaSpin->value());
    cfg.retarget.deadbandDeg = static_cast<float>(_deadbandDegSpin->value());
    cfg.retarget.maxVelocityDegS = static_cast<float>(_maxVelocitySpin->value());
    cfg.retarget.mode = _retargetModeCombo->currentData().toString().toStdString();
    cfg.retarget.stancePreset = _stancePresetCombo->currentData().toString().toStdString();

    cfg.safety.safetyMarginDeg = static_cast<float>(_safetyMarginSpin->value());
    cfg.safety.txRateHz = _txRateHzSpin->value();
    cfg.safety.watchdogTimeoutMs = _watchdogTimeoutSpin->value();

    std::string hostStr = _mqttHostEdit->text().trimmed().toStdString();
    cfg.mqtt.host = hostStr.empty() ? "localhost" : hostStr;
    cfg.mqtt.port = _mqttPortSpin->value();
    cfg.mqtt.username = _mqttUsernameEdit->text().trimmed().toStdString();
    cfg.mqtt.password = _mqttPasswordEdit->text().toStdString();
    QString robotIdQStr = _mqttRobotIdEdit->text().trimmed();
    if (robotIdQStr.startsWith("robot/robohero/")) {
        robotIdQStr.remove(0, QString("robot/robohero/").length());
    }
    while (robotIdQStr.endsWith('/')) {
        robotIdQStr.chop(1);
    }
    if (robotIdQStr.endsWith("/control")) {
        robotIdQStr.chop(QString("/control").length());
    } else if (robotIdQStr.endsWith("/status")) {
        robotIdQStr.chop(QString("/status").length());
    }
    std::string robotIdStr = robotIdQStr.toStdString();
    cfg.mqtt.robotId = robotIdStr.empty() ? "robohero" : robotIdStr;
    cfg.mqtt.keepalive = _mqttKeepaliveSpin->value();
    cfg.mqtt.autoConnect = _mqttAutoConnectCheck->isChecked();

    cfg.view.showRobotMeshes = _viewMeshesCheck->isChecked();
    cfg.view.showSkeleton = _viewSkeletonCheck->isChecked();
    cfg.view.showGrid = _viewGridCheck->isChecked();
    cfg.view.darkTheme = _viewDarkThemeCheck->isChecked();

    cfg.save();
    emit settingsApplied();
}

void SettingsDialog::onOkClicked()
{
    applyToConfig();
    accept();
}

void SettingsDialog::onApplyClicked()
{
    applyToConfig();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}

void SettingsDialog::setEstimatorInfo(const std::string &activeProvider,
                                     const std::vector<std::string> &availableProviders)
{
    if (!_activeProviderLabel) {
        return;
    }

    QString activeStr = QString::fromStdString(activeProvider);
    QString availStr;
    for (size_t i = 0; i < availableProviders.size(); ++i) {
        if (i > 0) {
            availStr += ", ";
        }
        availStr += QString::fromStdString(availableProviders[i]);
    }

    bool isGpu = (activeProvider == "DirectML" || activeProvider == "CUDA");
    QString color = isGpu ? "#00ff88" : "#ffcc00";

    _activeProviderLabel->setText(
        QString("Active: <span style='color: %1;'>%2</span> (Detected: %3)")
            .arg(color)
            .arg(activeStr.isEmpty() ? "None" : activeStr)
            .arg(availStr.isEmpty() ? "CPU" : availStr));
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
