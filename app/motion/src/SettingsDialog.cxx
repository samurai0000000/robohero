/*
 * SettingsDialog.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "SettingsDialog.hxx"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , _mqttHostEdit(nullptr)
    , _mqttPortSpin(nullptr)
    , _mqttUserEdit(nullptr)
    , _mqttPassEdit(nullptr)
    , _mqttRobotIdEdit(nullptr)
    , _mqttKeepaliveSpin(nullptr)
    , _mqttAutoConnectCheck(nullptr)
    , _workspaceDirEdit(nullptr)
    , _browseDirBtn(nullptr)
    , _darkThemeCheck(nullptr)
    , _applyBtn(nullptr)
    , _okBtn(nullptr)
    , _cancelBtn(nullptr)
{
    setWindowTitle("RoboHero Motion Studio - Settings");
    setMinimumWidth(480);
    setupUi();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Group 1: MQTT Settings
    auto *mqttGroup = new QGroupBox("MQTT Robot Communication", this);
    auto *mqttLayout = new QFormLayout(mqttGroup);
    mqttLayout->setSpacing(8);

    _mqttHostEdit = new QLineEdit(mqttGroup);
    _mqttHostEdit->setPlaceholderText("localhost");
    mqttLayout->addRow("Broker Host:", _mqttHostEdit);

    _mqttPortSpin = new QSpinBox(mqttGroup);
    _mqttPortSpin->setRange(1, 65535);
    _mqttPortSpin->setValue(1883);
    mqttLayout->addRow("Broker Port:", _mqttPortSpin);

    _mqttRobotIdEdit = new QLineEdit(mqttGroup);
    _mqttRobotIdEdit->setPlaceholderText("01");
    mqttLayout->addRow("Robot ID:", _mqttRobotIdEdit);

    _mqttUserEdit = new QLineEdit(mqttGroup);
    _mqttUserEdit->setPlaceholderText("(Optional)");
    mqttLayout->addRow("Username:", _mqttUserEdit);

    _mqttPassEdit = new QLineEdit(mqttGroup);
    _mqttPassEdit->setEchoMode(QLineEdit::Password);
    _mqttPassEdit->setPlaceholderText("(Optional)");
    mqttLayout->addRow("Password:", _mqttPassEdit);

    _mqttKeepaliveSpin = new QSpinBox(mqttGroup);
    _mqttKeepaliveSpin->setRange(5, 300);
    _mqttKeepaliveSpin->setValue(60);
    _mqttKeepaliveSpin->setSuffix(" s");
    mqttLayout->addRow("Keepalive:", _mqttKeepaliveSpin);

    _mqttAutoConnectCheck = new QCheckBox("Auto-connect to MQTT on startup", mqttGroup);
    _mqttAutoConnectCheck->setChecked(true);
    mqttLayout->addRow("", _mqttAutoConnectCheck);

    mainLayout->addWidget(mqttGroup);

    // Group 2: Workspace & Library
    auto *wsGroup = new QGroupBox("Motion Library & Storage", this);
    auto *wsLayout = new QVBoxLayout(wsGroup);

    auto *dirRow = new QHBoxLayout();
    _workspaceDirEdit = new QLineEdit(wsGroup);
    _workspaceDirEdit->setPlaceholderText("Custom workspace directory...");
    dirRow->addWidget(_workspaceDirEdit);

    _browseDirBtn = new QPushButton("Browse...", wsGroup);
    connect(_browseDirBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseWorkspaceClicked);
    dirRow->addWidget(_browseDirBtn);
    wsLayout->addLayout(dirRow);

    mainLayout->addWidget(wsGroup);

    // Group 3: Display & Appearance
    auto *viewGroup = new QGroupBox("Appearance", this);
    auto *viewLayout = new QVBoxLayout(viewGroup);

    _darkThemeCheck = new QCheckBox("Enable Dark Theme", viewGroup);
    _darkThemeCheck->setChecked(true);
    viewLayout->addWidget(_darkThemeCheck);

    mainLayout->addWidget(viewGroup);

    // Dialog Button Row
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    _applyBtn = new QPushButton("Apply", this);
    connect(_applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    btnRow->addWidget(_applyBtn);

    _okBtn = new QPushButton("OK", this);
    _okBtn->setDefault(true);
    connect(_okBtn, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    btnRow->addWidget(_okBtn);

    _cancelBtn = new QPushButton("Cancel", this);
    connect(_cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    btnRow->addWidget(_cancelBtn);

    mainLayout->addLayout(btnRow);
}

void SettingsDialog::loadSettings()
{
    QSettings settings("RoboHero", "RoboHeroMotion");

    _mqttHostEdit->setText(settings.value("mqtt/host", "localhost").toString());
    _mqttPortSpin->setValue(settings.value("mqtt/port", 1883).toInt());
    _mqttRobotIdEdit->setText(settings.value("mqtt/robot_id", "01").toString());
    _mqttUserEdit->setText(settings.value("mqtt/username", "").toString());
    _mqttPassEdit->setText(settings.value("mqtt/password", "").toString());
    _mqttKeepaliveSpin->setValue(settings.value("mqtt/keepalive", 60).toInt());
    _mqttAutoConnectCheck->setChecked(settings.value("mqtt/autoconnect", true).toBool());

    _workspaceDirEdit->setText(settings.value("workspace/directory", "").toString());
    _darkThemeCheck->setChecked(settings.value("view/dark_theme", true).toBool());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("RoboHero", "RoboHeroMotion");

    settings.setValue("mqtt/host", _mqttHostEdit->text().trimmed());
    settings.setValue("mqtt/port", _mqttPortSpin->value());
    settings.setValue("mqtt/robot_id", _mqttRobotIdEdit->text().trimmed());
    settings.setValue("mqtt/username", _mqttUserEdit->text().trimmed());
    settings.setValue("mqtt/password", _mqttPassEdit->text());
    settings.setValue("mqtt/keepalive", _mqttKeepaliveSpin->value());
    settings.setValue("mqtt/autoconnect", _mqttAutoConnectCheck->isChecked());

    settings.setValue("workspace/directory", _workspaceDirEdit->text().trimmed());
    settings.setValue("view/dark_theme", _darkThemeCheck->isChecked());
}

QString SettingsDialog::mqttHost() const
{
    return _mqttHostEdit->text().trimmed();
}

int SettingsDialog::mqttPort() const
{
    return _mqttPortSpin->value();
}

QString SettingsDialog::mqttUsername() const
{
    return _mqttUserEdit->text().trimmed();
}

QString SettingsDialog::mqttPassword() const
{
    return _mqttPassEdit->text();
}

QString SettingsDialog::mqttRobotId() const
{
    return _mqttRobotIdEdit->text().trimmed();
}

int SettingsDialog::mqttKeepalive() const
{
    return _mqttKeepaliveSpin->value();
}

bool SettingsDialog::mqttAutoConnect() const
{
    return _mqttAutoConnectCheck->isChecked();
}

QString SettingsDialog::workspaceDirectory() const
{
    return _workspaceDirEdit->text().trimmed();
}

bool SettingsDialog::darkTheme() const
{
    return _darkThemeCheck->isChecked();
}

void SettingsDialog::onBrowseWorkspaceClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Custom Workspace Directory",
                                                    _workspaceDirEdit->text());
    if (!dir.isEmpty()) {
        _workspaceDirEdit->setText(dir);
    }
}

void SettingsDialog::onApplyClicked()
{
    saveSettings();
    emit settingsApplied();
}

void SettingsDialog::onOkClicked()
{
    saveSettings();
    emit settingsApplied();
    accept();
}

void SettingsDialog::onCancelClicked()
{
    loadSettings();
    reject();
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
