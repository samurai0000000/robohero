/*
 * MainWindow.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MainWindow.hxx"
#include "UrdfLimits.hxx"
#include "ParametricGait.hxx"
#include "SettingsDialog.hxx"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QScrollArea>
#include <QMessageBox>
#include <QFileDialog>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , _viewerMain(nullptr)
    , _viewerTelem(nullptr)
    , _timelineWidget(nullptr)
    , _libraryWidget(nullptr)
    , _player(nullptr)
    , _mqttClient(nullptr)
    , _settingsDialog(nullptr)
    , _splitViewMode(false)
    , _telemContainer(nullptr)
    , _splitViewBtn(nullptr)
    , _updatingSlidersFromEngine(false)
    , _gaitModeCombo(nullptr)
    , _strideSpin(nullptr)
    , _stepHeightSpin(nullptr)
    , _swaySpin(nullptr)
    , _turnSpin(nullptr)
    , _durationSpin(nullptr)
    , _torsoPitchSpin(nullptr)
    , _armSwingSpin(nullptr)
    , _armModeCombo(nullptr)
    , _cycleSpin(nullptr)
    , _genGaitBtn(nullptr)
    , _statusMessageLabel(nullptr)
    , _mqttStatusBadge(nullptr)
    , _comStabilityBadge(nullptr)
{
    for (int ch = 0; ch < 17; ++ch) {
        int center = UrdfLimits::instance().getCalibration(ch).center;
        _telemPwm[ch] = center;
        _telemAngles[ch] = UrdfLimits::instance().pwmToAngle(ch, center);
    }

    setWindowTitle("RoboHero Motion & Gait Studio");
    setWindowIcon(QIcon(":/icons/robohero_icon.png"));
    resize(1400, 900);

    // Initialize core components
    _mqttClient = new MqttClient(this);
    connect(_mqttClient, &MqttClient::connected, this, &MainWindow::onMqttConnected);
    connect(_mqttClient, &MqttClient::disconnected, this, &MainWindow::onMqttDisconnected);
    connect(_mqttClient, &MqttClient::connectionError, this, &MainWindow::onMqttError);
    connect(_mqttClient, &MqttClient::telemetryReceived, this, &MainWindow::onTelemetryReceived,
            Qt::QueuedConnection);

    _player = new MotionPlayer(this);
    _player->setMqttClient(_mqttClient);
    connect(_player, &MotionPlayer::playheadChanged, this, &MainWindow::onPlayheadChanged);
    connect(_player, &MotionPlayer::poseUpdated, this, &MainWindow::onPoseUpdated);

    setupUi();
    setupToolbar();
    setupStatusBar();
    applyTheme();

    // Default to standby pose
    _currentSequence = MotionSequence::createDefaultStandby();
    _player->setMotion(_currentSequence);

    _viewerMain->setStandbyAngles(_player->standbyAngles());
    _viewerTelem->setStandbyAngles(_player->standbyAngles());

    // Auto-connect MQTT if enabled in settings
    _settingsDialog = new SettingsDialog(this);
    connect(_settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::onSettingsApplied);
    if (_settingsDialog->mqttAutoConnect()) {
        connectMqtt();
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Main Vertical Splitter: Viewports + Panels on top, Timeline on bottom
    auto *vSplitter = new QSplitter(Qt::Vertical, centralWidget);

    // Top Horizontal Splitter: Library/Generator (left), 3D Viewport (center), Sliders (right)
    auto *hSplitter = new QSplitter(Qt::Horizontal, vSplitter);

    // Left Panel: TabWidget with Motion Library & Parametric Gait Generator
    auto *leftTabs = new QTabWidget(hSplitter);
    leftTabs->setMinimumWidth(280);
    leftTabs->setMaximumWidth(400);

    _libraryWidget = new MotionLibraryWidget(leftTabs);
    connect(_libraryWidget, &MotionLibraryWidget::motionLoadRequested, this, &MainWindow::onMotionLoadRequested);
    connect(_libraryWidget, &MotionLibraryWidget::motionPlayOnRobotRequested, this, &MainWindow::onMotionPlayOnRobotRequested);
    connect(_libraryWidget, &MotionLibraryWidget::newMotionRequested, this, &MainWindow::onNewMotionRequested);
    connect(_libraryWidget, &MotionLibraryWidget::saveMotionRequested, this, &MainWindow::onSaveMotionRequested);
    leftTabs->addTab(_libraryWidget, "Motion Library");

    auto *generatorContainer = new QWidget(leftTabs);
    setupGeneratorTab(generatorContainer);
    leftTabs->addTab(generatorContainer, "Gait Engine");

    hSplitter->addWidget(leftTabs);

    // Center Panel: 3D URDF Viewports Container
    auto *viewportContainer = new QWidget(hSplitter);
    auto *vpLayout = new QHBoxLayout(viewportContainer);
    vpLayout->setContentsMargins(0, 0, 0, 0);
    vpLayout->setSpacing(4);

    // Main / Planned Viewport
    auto *mainContainer = new QWidget(viewportContainer);
    auto *mainVLayout = new QVBoxLayout(mainContainer);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(2);
    auto *mainLabel = new QLabel("<b>Planned Trajectory (Virtual Twin)</b>", mainContainer);
    mainLabel->setStyleSheet("color: #00ADB5; padding: 2px 6px;");
    mainLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _viewerMain = new UrdfViewerWidget(mainContainer);
    _viewerMain->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainVLayout->addWidget(mainLabel);
    mainVLayout->addWidget(_viewerMain, 1);
    vpLayout->addWidget(mainContainer, 1);

    // Telemetry Echo Viewport (for Side-by-Side Split View)
    _telemContainer = new QWidget(viewportContainer);
    auto *telemVLayout = new QVBoxLayout(_telemContainer);
    telemVLayout->setContentsMargins(0, 0, 0, 0);
    telemVLayout->setSpacing(2);
    auto *telemLabel = new QLabel("<b>Physical Robot Telemetry</b>", _telemContainer);
    telemLabel->setStyleSheet("color: #00ff88; padding: 2px 6px;");
    telemLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _viewerTelem = new UrdfViewerWidget(_telemContainer);
    _viewerTelem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    telemVLayout->addWidget(telemLabel);
    telemVLayout->addWidget(_viewerTelem, 1);
    _telemContainer->setVisible(false); // Initially hidden in single viewport mode
    vpLayout->addWidget(_telemContainer, 1);

    // Synchronize orbital camera rotation between both viewports
    connect(_viewerMain, &UrdfViewerWidget::cameraChanged, _viewerTelem, &UrdfViewerWidget::syncCamera);
    connect(_viewerTelem, &UrdfViewerWidget::cameraChanged, _viewerMain, &UrdfViewerWidget::syncCamera);

    hSplitter->addWidget(viewportContainer);

    // Right Panel: 17 Joint Angle Sliders Inspector
    auto *sliderContainer = new QWidget(hSplitter);
    sliderContainer->setObjectName("jointSliderPanel");
    sliderContainer->setMinimumWidth(380);
    sliderContainer->setMaximumWidth(560);
    setupJointSliders(sliderContainer);
    hSplitter->addWidget(sliderContainer);

    // Set horizontal splitter initial stretch factors & sizes: [Library ~260, 3D Viewport ~680, Inspector ~460]
    hSplitter->setStretchFactor(0, 0);
    hSplitter->setStretchFactor(1, 1);
    hSplitter->setStretchFactor(2, 0);
    hSplitter->setSizes({260, 680, 460});

    vSplitter->addWidget(hSplitter);

    // Bottom Panel: Timeline & Dope Sheet
    _timelineWidget = new TimelineWidget(vSplitter);
    connect(_timelineWidget, &TimelineWidget::timeChanged, this, &MainWindow::onTimelineTimeChanged);
    connect(_timelineWidget, &TimelineWidget::playToggled, this, &MainWindow::onPlayToggled);
    connect(_timelineWidget, &TimelineWidget::stopClicked, this, &MainWindow::onStopClicked);
    connect(_timelineWidget, &TimelineWidget::loopToggled, this, &MainWindow::onLoopToggled);
    connect(_timelineWidget, &TimelineWidget::syncToRobotToggled, this, &MainWindow::onSyncToRobotToggled);
    connect(_timelineWidget, &TimelineWidget::addKeyframeRequested, this, &MainWindow::onAddKeyframeRequested);
    connect(_timelineWidget, &TimelineWidget::deleteKeyframeRequested, this, &MainWindow::onDeleteKeyframeRequested);
    connect(_timelineWidget, &TimelineWidget::keyframeSelected, this, &MainWindow::onKeyframeSelected);

    vSplitter->addWidget(_timelineWidget);
    vSplitter->setStretchFactor(0, 1);
    vSplitter->setStretchFactor(1, 0);

    mainLayout->addWidget(vSplitter);
}

void MainWindow::setupToolbar()
{
    auto *toolBar = addToolBar("Main Controls");
    toolBar->setMovable(false);

    // Viewpoint presets
    auto *btnFront = new QPushButton("Front", this);
    connect(btnFront, &QPushButton::clicked, this, [this]() { onViewPresetClicked("front"); });
    toolBar->addWidget(btnFront);

    auto *btnSide = new QPushButton("Side", this);
    connect(btnSide, &QPushButton::clicked, this, [this]() { onViewPresetClicked("side"); });
    toolBar->addWidget(btnSide);

    auto *btnBack = new QPushButton("Back", this);
    connect(btnBack, &QPushButton::clicked, this, [this]() { onViewPresetClicked("back"); });
    toolBar->addWidget(btnBack);

    auto *btnTop = new QPushButton("Top", this);
    connect(btnTop, &QPushButton::clicked, this, [this]() { onViewPresetClicked("top"); });
    toolBar->addWidget(btnTop);

    auto *btnPersp = new QPushButton("3D", this);
    connect(btnPersp, &QPushButton::clicked, this, [this]() { onViewPresetClicked("persp"); });
    toolBar->addWidget(btnPersp);

    toolBar->addSeparator();

    // Dual View Mode: Ghost Overlay vs Side-by-Side Split View
    _splitViewBtn = new QPushButton("◫ Split View", this);
    _splitViewBtn->setCheckable(true);
    _splitViewBtn->setToolTip("Toggle between Single Viewport (with Ghost Overlay) and Side-by-Side Split View");
    connect(_splitViewBtn, &QPushButton::clicked, this, &MainWindow::onSplitViewToggled);
    toolBar->addWidget(_splitViewBtn);

    _ghostCheckBox = new QCheckBox("👻 Ghost View", this);
    _ghostCheckBox->setChecked(true);
    _ghostCheckBox->setToolTip("Toggle semi-transparent ghost overlay (neutral standby stance when offline, live telemetry when online)");
    connect(_ghostCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (!_splitViewMode) {
            _viewerMain->setShowGhost(checked);
        }
        _statusMessageLabel->setText(checked ? "Ghost View enabled." : "Ghost View disabled.");
    });
    toolBar->addWidget(_ghostCheckBox);

    toolBar->addSeparator();

    // Gestures Menu
    auto *gestureBtn = new QPushButton("✨ Gestures ▼", this);
    auto *gestureMenu = new QMenu(gestureBtn);
    gestureMenu->addAction("Friendly Wave", this, [this]() { onGenerateGestureClicked("wave"); });
    gestureMenu->addAction("Respectful Bow", this, [this]() { onGenerateGestureClicked("bow"); });
    gestureMenu->addAction("Applause Clap", this, [this]() { onGenerateGestureClicked("clap"); });
    gestureMenu->addAction("Martial Arts Guard", this, [this]() { onGenerateGestureClicked("guard"); });
    gestureMenu->addAction("Athletic Squat", this, [this]() { onGenerateGestureClicked("squat"); });
    gestureBtn->setMenu(gestureMenu);
    toolBar->addWidget(gestureBtn);

    toolBar->addSeparator();

    // Hardware quick commands
    auto *centerBtn = new QPushButton("🎯 Center", this);
    centerBtn->setToolTip("Send Center PWM (0 rad) command to all joints");
    connect(centerBtn, &QPushButton::clicked, this, &MainWindow::onCenterClicked);
    toolBar->addWidget(centerBtn);

    auto *relaxBtn = new QPushButton("⏸ Relax", this);
    relaxBtn->setToolTip("Send Relax (torque off) command to robot");
    connect(relaxBtn, &QPushButton::clicked, this, &MainWindow::onRelaxClicked);
    toolBar->addWidget(relaxBtn);

    auto *stopHwBtn = new QPushButton("🛑 Emergency Stop", this);
    stopHwBtn->setStyleSheet("background-color: #882222; color: #ffffff; font-weight: bold;");
    connect(stopHwBtn, &QPushButton::clicked, this, &MainWindow::onStopHardwareClicked);
    toolBar->addWidget(stopHwBtn);

    toolBar->addSeparator();

    // Settings
    auto *settingsBtn = new QPushButton("⚙ Settings", this);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    toolBar->addWidget(settingsBtn);

    // Spacer
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    // MQTT Status Badge
    _mqttStatusBadge = new QLabel("MQTT: Disconnected", this);
    _mqttStatusBadge->setStyleSheet("color: #ff5555; padding-right: 15px; font-weight: bold;");
    toolBar->addWidget(_mqttStatusBadge);
}

void MainWindow::setupStatusBar()
{
    _statusMessageLabel = new QLabel("RoboHero Motion & Gait Studio ready.", this);
    _statusMessageLabel->setObjectName("statusMessage");
    statusBar()->addWidget(_statusMessageLabel, 1);

    _comStabilityBadge = new QLabel("🟢 CoM Stable", this);
    _comStabilityBadge->setStyleSheet("color: #00ff88; font-weight: bold; padding: 0 10px;");
    statusBar()->addPermanentWidget(_comStabilityBadge);
}

void MainWindow::setupJointSliders(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    scrollArea->viewport()->setStyleSheet("background: transparent;");

    auto *scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");

    const auto &limits = UrdfLimits::instance();

    auto createGroup = [&](const QString &title, const std::vector<int> &channels) -> QGroupBox * {
        auto *grp = new QGroupBox(title, scrollContent);
        auto *grpLayout = new QVBoxLayout(grp);
        grpLayout->setContentsMargins(6, 12, 6, 6);
        grpLayout->setSpacing(4);

        for (int ch : channels) {
            auto lim = limits.getLimit(ch);
            auto cal = limits.getCalibration(ch);

            // Clean friendly label from calibration name
            QString rawName = QString::fromStdString(cal.name);
            rawName.remove("left_").remove("right_").remove("_joint");
            QStringList parts = rawName.split('_', Qt::SkipEmptyParts);
            for (auto &part : parts) {
                if (!part.isEmpty()) {
                    part[0] = part[0].toUpper();
                }
            }
            QString friendlyName = parts.join(' ');

            auto *rowLayout = new QHBoxLayout();
            rowLayout->setSpacing(4);

            auto *nameLbl = new QLabel(grp);
            nameLbl->setText(QString("<span style='color: #00ADB5; font-size: 10px; font-weight: bold;'>CH%1</span> "
                                     "<span style='color: #f0f0f5; font-size: 11px; font-weight: bold;'>%2</span>")
                             .arg(ch)
                             .arg(friendlyName));
            nameLbl->setToolTip(QString("Channel %1: %2\nRange: %3° to %4°")
                                .arg(ch)
                                .arg(QString::fromStdString(cal.name))
                                .arg(lim.lower * 180.0 / M_PI, 0, 'f', 1)
                                .arg(lim.upper * 180.0 / M_PI, 0, 'f', 1));

            auto *valLbl = new QLabel("0.0°", grp);
            valLbl->setStyleSheet(
                "font-size: 11px; font-weight: bold; color: #00e5ff; "
                "background-color: #16161e; border: 1px solid #363648; "
                "border-radius: 3px; padding: 1px 4px;"
            );
            valLbl->setFixedWidth(52);
            valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            rowLayout->addWidget(nameLbl);
            rowLayout->addStretch();
            rowLayout->addWidget(valLbl);
            grpLayout->addLayout(rowLayout);

            auto *slider = new QSlider(Qt::Horizontal, grp);
            // Map degrees (-180 to +180) to slider values (-1800 to 1800)
            int minVal = static_cast<int>(lim.lower * 180.0 / M_PI * 10.0);
            int maxVal = static_cast<int>(lim.upper * 180.0 / M_PI * 10.0);
            slider->setRange(minVal, maxVal);
            slider->setValue(0);
            slider->setStyleSheet(
                "QSlider::groove:horizontal { height: 4px; background: #32323e; border-radius: 2px; }"
                "QSlider::sub-page:horizontal { background: #00ADB5; border-radius: 2px; }"
                "QSlider::handle:horizontal { background: #ffffff; border: 2px solid #00ADB5; width: 14px; margin: -5px 0; border-radius: 7px; }"
                "QSlider::handle:horizontal:hover { background: #00e5ff; border-color: #ffffff; }"
            );

            connect(slider, &QSlider::valueChanged, this, [this, ch](int val) {
                onSliderValueChanged(ch, val);
            });

            grpLayout->addWidget(slider);

            _jointControls[ch].nameLabel = nameLbl;
            _jointControls[ch].valLabel = valLbl;
            _jointControls[ch].slider = slider;
        }

        return grp;
    };

    auto *headGroup = createGroup("Head", {16});
    auto *rightArmGroup = createGroup("Right Arm", {10, 9, 8});
    auto *leftArmGroup = createGroup("Left Arm", {5, 6, 7});
    auto *rightLegGroup = createGroup("Right Leg", {11, 12, 13, 14, 15});
    auto *leftLegGroup = createGroup("Left Leg", {4, 3, 2, 1, 0});

    auto *gridLayout = new QGridLayout(scrollContent);
    gridLayout->setContentsMargins(4, 4, 4, 4);
    gridLayout->setSpacing(8);

    // Row 0: Head (Centered across columns 0 and 1)
    gridLayout->addWidget(headGroup, 0, 0, 1, 2);

    // Row 1: Arms (Col 0: Right Arm, Col 1: Left Arm)
    gridLayout->addWidget(rightArmGroup, 1, 0);
    gridLayout->addWidget(leftArmGroup, 1, 1);

    // Row 2: Legs (Col 0: Right Leg, Col 1: Left Leg)
    gridLayout->addWidget(rightLegGroup, 2, 0);
    gridLayout->addWidget(leftLegGroup, 2, 1);

    gridLayout->setRowStretch(3, 1);

    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);
}

void MainWindow::setupGeneratorTab(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(6);

    _gaitModeCombo = new QComboBox(parent);
    _gaitModeCombo->addItem("Forward Walk", "forward");
    _gaitModeCombo->addItem("Backward Walk", "backward");
    _gaitModeCombo->addItem("Turn Left", "turn_left");
    _gaitModeCombo->addItem("Turn Right", "turn_right");
    _gaitModeCombo->addItem("Sidestep Left", "sidestep_left");
    _gaitModeCombo->addItem("Sidestep Right", "sidestep_right");
    formLayout->addRow("Gait Mode:", _gaitModeCombo);

    _strideSpin = new QDoubleSpinBox(parent);
    _strideSpin->setRange(10.0, 70.0);
    _strideSpin->setValue(40.0);
    _strideSpin->setSuffix(" mm");
    formLayout->addRow("Stride Length:", _strideSpin);

    _stepHeightSpin = new QDoubleSpinBox(parent);
    _stepHeightSpin->setRange(5.0, 45.0);
    _stepHeightSpin->setValue(25.0);
    _stepHeightSpin->setSuffix(" mm");
    formLayout->addRow("Step Height:", _stepHeightSpin);

    _swaySpin = new QDoubleSpinBox(parent);
    _swaySpin->setRange(5.0, 35.0);
    _swaySpin->setValue(18.0);
    _swaySpin->setSuffix(" mm");
    formLayout->addRow("Pelvis Sway:", _swaySpin);

    _turnSpin = new QDoubleSpinBox(parent);
    _turnSpin->setRange(2.0, 30.0);
    _turnSpin->setValue(10.0);
    _turnSpin->setSuffix(" °");
    formLayout->addRow("Turn Angle:", _turnSpin);

    _durationSpin = new QSpinBox(parent);
    _durationSpin->setRange(300, 1500);
    _durationSpin->setValue(600);
    _durationSpin->setSuffix(" ms");
    formLayout->addRow("Step Cadence:", _durationSpin);

    _torsoPitchSpin = new QDoubleSpinBox(parent);
    _torsoPitchSpin->setRange(0.0, 15.0);
    _torsoPitchSpin->setValue(3.0);
    _torsoPitchSpin->setSuffix(" °");
    formLayout->addRow("Torso Pitch:", _torsoPitchSpin);

    _armSwingSpin = new QDoubleSpinBox(parent);
    _armSwingSpin->setRange(0.0, 45.0);
    _armSwingSpin->setValue(20.0);
    _armSwingSpin->setSuffix(" °");
    formLayout->addRow("Arm Swing:", _armSwingSpin);

    _armModeCombo = new QComboBox(parent);
    _armModeCombo->addItem("Natural Swing", "natural_swing");
    _armModeCombo->addItem("Martial Arts Guard", "guard");
    _armModeCombo->addItem("Held at Sides", "held");
    formLayout->addRow("Arm Stance:", _armModeCombo);

    _cycleSpin = new QSpinBox(parent);
    _cycleSpin->setRange(1, 10);
    _cycleSpin->setValue(2);
    _cycleSpin->setSuffix(" cycles");
    formLayout->addRow("Cycle Count:", _cycleSpin);

    layout->addLayout(formLayout);

    _genGaitBtn = new QPushButton("🚀 Synthesize Gait", parent);
    _genGaitBtn->setStyleSheet("background-color: #00ADB5; color: #ffffff; font-weight: bold; padding: 8px; border-radius: 4px;");
    connect(_genGaitBtn, &QPushButton::clicked, this, &MainWindow::onGenerateGaitClicked);
    layout->addWidget(_genGaitBtn);

    layout->addStretch();
}

void MainWindow::applyTheme()
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#1e1e24"));
    pal.setColor(QPalette::WindowText, QColor("#f0f0f5"));
    pal.setColor(QPalette::Base, QColor("#16161a"));
    pal.setColor(QPalette::AlternateBase, QColor("#22222a"));
    pal.setColor(QPalette::ToolTipBase, QColor("#26262e"));
    pal.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    pal.setColor(QPalette::Text, QColor("#f0f0f5"));
    pal.setColor(QPalette::Button, QColor("#2a2a34"));
    pal.setColor(QPalette::ButtonText, QColor("#ffffff"));
    setPalette(pal);

    setStyleSheet(
        "QMainWindow { background-color: #1e1e24; color: #f0f0f5; }"
        "QWidget { color: #f0f0f5; }"
        "QLabel { color: #f0f0f5; }"
        "QWidget#jointSliderPanel { background-color: #1a1a22; border-left: 1px solid #2e2e3a; }"
        "QScrollArea { background-color: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background-color: transparent; }"
        "QToolBar { background-color: #26262e; color: #f0f0f5; border-bottom: 1px solid #333; spacing: 8px; padding: 4px; }"
        "QMenuBar { background-color: #26262e; color: #f0f0f5; border-bottom: 1px solid #333; }"
        "QMenu { background-color: #26262e; color: #f0f0f5; border: 1px solid #444; }"
        "QMenu::item:selected { background-color: #00ADB5; color: #ffffff; }"
        "QStatusBar { background-color: #1a1a1e; color: #f0f0f5; }"
        "QPushButton { background-color: #33333d; color: #fff; border: 1px solid #4a4a55; border-radius: 4px; padding: 5px 12px; }"
        "QPushButton:hover { background-color: #3d3d4a; border-color: #00ADB5; }"
        "QPushButton:pressed { background-color: #222228; }"
        "QCheckBox { color: #f0f0f5; }"
        "QGroupBox { background-color: #23232c; color: #f0f0f5; border: 1px solid #363645; border-radius: 6px; margin-top: 14px; padding-top: 12px; padding-bottom: 6px; padding-left: 6px; padding-right: 6px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 1px 6px; color: #00ADB5; background-color: #2a2a36; border: 1px solid #363645; border-radius: 3px; font-size: 11px; }"
        "QTabWidget::pane { border: 1px solid #444; background-color: #1e1e24; }"
        "QTabBar::tab { color: #f0f0f5; background-color: #26262e; padding: 6px 12px; border: 1px solid #444; }"
        "QTabBar::tab:selected { color: #ffffff; background-color: #383842; }"
        "QLineEdit { color: #f0f0f5; background-color: #16161a; border: 1px solid #444; padding: 3px; }"
        "QSpinBox, QDoubleSpinBox { color: #f0f0f5; background-color: #16161a; border: 1px solid #444; padding: 2px; }"
        "QComboBox { color: #f0f0f5; background-color: #16161a; border: 1px solid #444; padding: 2px; }"
        "QComboBox QAbstractItemView { color: #f0f0f5; background-color: #26262e; }"
        "QSplitter::handle { background-color: #333; }"
    );
}

void MainWindow::connectMqtt()
{
    _mqttClient->disconnectFromBroker();
    _mqttClient->setRobotId(_settingsDialog->mqttRobotId().toStdString());
    _mqttStatusBadge->setText(QString("MQTT: Connecting (%1:%2)...").arg(_settingsDialog->mqttHost()).arg(_settingsDialog->mqttPort()));
    _mqttStatusBadge->setStyleSheet("color: #ffaa00; padding-right: 15px; font-weight: bold;");

    _mqttClient->connectToBroker(_settingsDialog->mqttHost().toStdString(),
                                 _settingsDialog->mqttPort(),
                                 _settingsDialog->mqttUsername().toStdString(),
                                 _settingsDialog->mqttPassword().toStdString(),
                                 _settingsDialog->mqttKeepalive());
}

void MainWindow::onPlayheadChanged(int timeMs)
{
    _timelineWidget->setTime(timeMs);
}

void MainWindow::onPoseUpdated(const std::array<double, 17> &angles, const std::array<int, 17> &pwm)
{
    if (_viewerMain) {
        _viewerMain->setJointAngles(angles);
        _viewerMain->setJointPwm(pwm);
    }
    updateJointSliders(angles, pwm);
}

void MainWindow::onTimelineTimeChanged(int timeMs)
{
    _player->seek(timeMs, _timelineWidget->isSyncToRobot());
}

void MainWindow::onPlayToggled(bool playing)
{
    if (playing) {
        _player->play();
        _statusMessageLabel->setText("Playback started.");
    } else {
        _player->pause();
        _statusMessageLabel->setText("Playback paused.");
    }
}

void MainWindow::onStopClicked()
{
    _player->stop(true);
    _statusMessageLabel->setText("Playback stopped. Returned to Standby pose.");
}

void MainWindow::onLoopToggled(bool loop)
{
    _player->setLoop(loop);
}

void MainWindow::onSyncToRobotToggled(bool sync)
{
    _player->setSyncToRobot(sync);
    if (sync) {
        _statusMessageLabel->setText("⚡ Robot synchronization ACTIVE. Motion commands streaming to robot.");
    } else {
        _statusMessageLabel->setText("Robot synchronization PAUSED.");
    }
}

void MainWindow::onAddKeyframeRequested(int timeMs)
{
    int idx = _currentSequence.addKeyframe(timeMs, _player->currentAngles(), "cubic_in_out");
    _timelineWidget->setKeyframes(_currentSequence.keyframes());
    _timelineWidget->setDuration(_currentSequence.durationMs());
    _player->setMotion(_currentSequence, false);
    _player->seek(timeMs, _timelineWidget->isSyncToRobot());
    _timelineWidget->setTime(timeMs);
    _statusMessageLabel->setText(QString("Added keyframe at %1 ms (index %2)").arg(timeMs).arg(idx));
}

void MainWindow::onDeleteKeyframeRequested(int index)
{
    int currentTime = _timelineWidget->currentTime();
    _currentSequence.removeKeyframe(index);
    _timelineWidget->setKeyframes(_currentSequence.keyframes());
    _player->setMotion(_currentSequence, false);
    _player->seek(currentTime, _timelineWidget->isSyncToRobot());
    _timelineWidget->setTime(currentTime);
    _statusMessageLabel->setText(QString("Deleted keyframe at index %1").arg(index));
}

void MainWindow::onKeyframeSelected(int index)
{
    const auto &kfs = _currentSequence.keyframes();
    if (index >= 0 && index < static_cast<int>(kfs.size())) {
        _statusMessageLabel->setText(QString("Selected keyframe %1 at %2 ms").arg(index).arg(kfs[index].timeMs));
    }
}

void MainWindow::onMotionLoadRequested(const QString &filePath)
{
    if (_currentSequence.loadFromFile(filePath.toStdString())) {
        _currentFilePath = filePath;
        _player->setMotion(_currentSequence);
        _timelineWidget->setDuration(_currentSequence.durationMs());
        _timelineWidget->setKeyframes(_currentSequence.keyframes());
        _timelineWidget->setLoop(_currentSequence.isLoop());
        _timelineWidget->setTime(0);
        MotionLibrary::instance().setLastLoadedFile(filePath);
        _statusMessageLabel->setText(QString("Loaded motion: '%1'").arg(QString::fromStdString(_currentSequence.name())));
    } else {
        QMessageBox::warning(this, "Load Error", QString("Failed to load motion file:\n%1").arg(filePath));
    }
}

void MainWindow::onMotionPlayOnRobotRequested(const QString &filePath)
{
    onMotionLoadRequested(filePath);
    _timelineWidget->setSyncToRobot(true);
    _player->setSyncToRobot(true);
    _timelineWidget->setPlaying(true);
    _player->play();
    _statusMessageLabel->setText(QString("Streaming motion '%1' to robot...").arg(QString::fromStdString(_currentSequence.name())));
}

void MainWindow::onNewMotionRequested()
{
    _currentSequence = MotionSequence::createDefaultStandby();
    _currentFilePath.clear();
    _player->setMotion(_currentSequence);
    _timelineWidget->setDuration(_currentSequence.durationMs());
    _timelineWidget->setKeyframes(_currentSequence.keyframes());
    _timelineWidget->setTime(0);
    _statusMessageLabel->setText("Created new blank motion sequence.");
}

void MainWindow::onSaveMotionRequested()
{
    QString savePath = _currentFilePath;
    if (savePath.isEmpty() || savePath.contains(":/")) {
        QString defaultDir = MotionLibrary::instance().defaultUserDir();
        savePath = QFileDialog::getSaveFileName(this, "Save Motion Sequence",
                                              defaultDir + "/motion.rhm.json",
                                              "RoboHero Motion (*.rhm.json *.json)");
        if (savePath.isEmpty()) return;
    }

    if (MotionLibrary::instance().saveMotion(_currentSequence, savePath)) {
        _currentFilePath = savePath;
        _libraryWidget->refresh();
        _libraryWidget->selectFile(savePath);
        _statusMessageLabel->setText(QString("Saved motion to '%1'").arg(savePath));
    } else {
        QMessageBox::warning(this, "Save Error", QString("Failed to save motion file:\n%1").arg(savePath));
    }
}

void MainWindow::onGenerateGaitClicked()
{
    ParametricGait::GaitParameters p;
    p.mode = _gaitModeCombo->currentData().toString().toStdString();
    p.strideLengthMm = _strideSpin->value();
    p.stepHeightMm = _stepHeightSpin->value();
    p.swayAmplitudeMm = _swaySpin->value();
    p.turnAngleDeg = _turnSpin->value();
    p.stepDurationMs = _durationSpin->value();
    p.torsoPitchDeg = _torsoPitchSpin->value();
    p.armSwingDeg = _armSwingSpin->value();
    p.armMode = _armModeCombo->currentData().toString().toStdString();
    p.cycleCount = _cycleSpin->value();

    _currentSequence = ParametricGait::generateGait(p);
    _currentFilePath.clear();
    _player->setMotion(_currentSequence);
    _timelineWidget->setDuration(_currentSequence.durationMs());
    _timelineWidget->setKeyframes(_currentSequence.keyframes());
    _timelineWidget->setLoop(true);
    _timelineWidget->setTime(0);

    _statusMessageLabel->setText(QString("Synthesized %1 gait (%2 ms, %3 keyframes)")
                                 .arg(QString::fromStdString(p.mode))
                                 .arg(_currentSequence.durationMs())
                                 .arg(_currentSequence.keyframeCount()));
}

void MainWindow::onGenerateGestureClicked(const QString &gestureType)
{
    _currentSequence = ParametricGait::generateGesture(gestureType.toStdString());
    _currentFilePath.clear();
    _player->setMotion(_currentSequence);
    _timelineWidget->setDuration(_currentSequence.durationMs());
    _timelineWidget->setKeyframes(_currentSequence.keyframes());
    _timelineWidget->setLoop(false);
    _timelineWidget->setTime(0);

    _statusMessageLabel->setText(QString("Synthesized '%1' gesture (%2 ms)")
                                 .arg(gestureType)
                                 .arg(_currentSequence.durationMs()));
}

void MainWindow::onSliderValueChanged(int channel, int value)
{
    if (_updatingSlidersFromEngine) return;

    double deg = static_cast<double>(value) / 10.0;
    double rad = deg * (M_PI / 180.0);

    _jointControls[channel].valLabel->setText(QString::number(deg, 'f', 1) + "°");

    int pwm = UrdfLimits::instance().angleToPwm(channel, rad);
    _player->setCurrentJointAngle(channel, rad, pwm);

    if (_viewerMain) {
        _viewerMain->setJointAngles(_player->currentAngles());
        _viewerMain->setJointPwm(_player->currentPwm());
    }

    if (_timelineWidget->isSyncToRobot() && _mqttClient && _mqttClient->isConnected()) {
        _mqttClient->sendPwm(_player->currentPwm(), _mqttClient->controlTopic());
    }
}

void MainWindow::updateJointSliders(const std::array<double, 17> &angles, const std::array<int, 17> &pwm)
{
    (void) pwm;
    _updatingSlidersFromEngine = true;
    for (int ch = 0; ch < 17; ++ch) {
        double deg = angles[ch] * (180.0 / M_PI);
        int sliderVal = static_cast<int>(std::round(deg * 10.0));
        _jointControls[ch].slider->blockSignals(true);
        _jointControls[ch].slider->setValue(sliderVal);
        _jointControls[ch].slider->blockSignals(false);
        _jointControls[ch].valLabel->setText(QString::number(deg, 'f', 1) + "°");
    }
    _updatingSlidersFromEngine = false;
}

void MainWindow::onSplitViewToggled()
{
    _splitViewMode = _splitViewBtn->isChecked();
    _telemContainer->setVisible(_splitViewMode);

    if (_splitViewMode) {
        _splitViewBtn->setText("▣ Single View");
        // In split mode, main viewer only shows solid planned model
        _viewerMain->setShowGhost(false);
        _ghostCheckBox->setEnabled(false);
        _statusMessageLabel->setText("Split View active: Planned Trajectory (Left) | Robot Telemetry (Right)");
    } else {
        _splitViewBtn->setText("◫ Split View");
        _ghostCheckBox->setEnabled(true);
        _viewerMain->setShowGhost(_ghostCheckBox->isChecked());
        _statusMessageLabel->setText("Single View active: Solid (Planned) with Ghost Overlay");
    }
}

void MainWindow::onViewPresetClicked(const QString &preset)
{
    if (_viewerMain) _viewerMain->setViewPreset(preset);
    if (_viewerTelem) _viewerTelem->setViewPreset(preset);
}

void MainWindow::onMqttConnected()
{
    _mqttStatusBadge->setText(QString("MQTT: Connected (%1:%2)")
                              .arg(_settingsDialog->mqttHost())
                              .arg(_settingsDialog->mqttPort()));
    _mqttStatusBadge->setStyleSheet("color: #00ff88; padding-right: 15px; font-weight: bold;");
    _statusMessageLabel->setText("MQTT connected. Ready to stream to RoboHero.");
}

void MainWindow::onMqttDisconnected()
{
    _mqttStatusBadge->setText("MQTT: Disconnected");
    _mqttStatusBadge->setStyleSheet("color: #ff5555; padding-right: 15px; font-weight: bold;");
    _statusMessageLabel->setText("MQTT disconnected.");
}

void MainWindow::onMqttError(const QString &msg)
{
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::onTelemetryReceived(const std::array<int, 17> &pwmValues,
                                     const std::array<bool, 17> &validMask)
{
    bool updated = false;
    const auto &limits = UrdfLimits::instance();
    for (int ch = 0; ch < 17; ++ch) {
        if (validMask[ch]) {
            _telemPwm[ch] = pwmValues[ch];
            _telemAngles[ch] = limits.pwmToAngle(ch, pwmValues[ch]);
            updated = true;
        }
    }

    if (updated) {
        if (_viewerMain) {
            _viewerMain->setTelemetryJointAngles(_telemAngles, validMask);
            _viewerMain->setTelemetryJointPwm(_telemPwm, validMask);
        }
        if (_viewerTelem) {
            _viewerTelem->setJointAngles(_telemAngles);
            _viewerTelem->setJointPwm(_telemPwm);
        }
    }
}

void MainWindow::onCenterClicked()
{
    if (_mqttClient && _mqttClient->isConnected()) {
        _mqttClient->sendCenter(_mqttClient->controlTopic());
        _statusMessageLabel->setText("Sent CENTER command to robot.");
    }
}

void MainWindow::onRelaxClicked()
{
    if (_mqttClient && _mqttClient->isConnected()) {
        _mqttClient->sendRelax(_mqttClient->controlTopic());
        _statusMessageLabel->setText("Sent RELAX (torque off) command to robot.");
    }
}

void MainWindow::onStopHardwareClicked()
{
    if (_player->isPlaying()) {
        _player->stop(false);
        _timelineWidget->setPlaying(false);
    }
    if (_mqttClient && _mqttClient->isConnected()) {
        _mqttClient->sendStop(_mqttClient->controlTopic());
    }
    _statusMessageLabel->setText("EMERGENCY STOP TRIGGERED!");
}

void MainWindow::onSettingsClicked()
{
    _settingsDialog->loadSettings();
    _settingsDialog->exec();
}

void MainWindow::onSettingsApplied()
{
    connectMqtt();
    if (!_settingsDialog->workspaceDirectory().isEmpty()) {
        MotionLibrary::instance().setCustomWorkspaceDir(_settingsDialog->workspaceDirectory());
        _libraryWidget->refresh();
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
