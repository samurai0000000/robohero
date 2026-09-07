/*
 * SettingsDialog.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_SETTINGS_DIALOG_HXX
#define ROBOHERO_MOTION_SETTINGS_DIALOG_HXX

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void loadSettings();
    void saveSettings();

    QString mqttHost() const;
    int mqttPort() const;
    QString mqttUsername() const;
    QString mqttPassword() const;
    QString mqttRobotId() const;
    int mqttKeepalive() const;
    bool mqttAutoConnect() const;

    QString workspaceDirectory() const;
    bool darkTheme() const;

signals:
    void settingsApplied();

private slots:
    void onBrowseWorkspaceClicked();
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUi();

    // MQTT
    QLineEdit *_mqttHostEdit;
    QSpinBox *_mqttPortSpin;
    QLineEdit *_mqttUserEdit;
    QLineEdit *_mqttPassEdit;
    QLineEdit *_mqttRobotIdEdit;
    QSpinBox *_mqttKeepaliveSpin;
    QCheckBox *_mqttAutoConnectCheck;

    // Workspace & View
    QLineEdit *_workspaceDirEdit;
    QPushButton *_browseDirBtn;
    QCheckBox *_darkThemeCheck;

    QPushButton *_applyBtn;
    QPushButton *_okBtn;
    QPushButton *_cancelBtn;
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
