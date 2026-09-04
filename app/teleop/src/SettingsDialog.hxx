/*
 * SettingsDialog.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SETTINGS_DIALOG_HXX
#define ROBOHERO_SETTINGS_DIALOG_HXX

#include <QDialog>
#include <QTabWidget>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void loadCurrentConfig();
    void applyToConfig();
    void setEstimatorInfo(const std::string &activeProvider,
                          const std::vector<std::string> &availableProviders);

signals:
    void settingsApplied();

private slots:
    void onOkClicked();
    void onApplyClicked();
    void onCancelClicked();

private:
    void setupUi();

    QTabWidget *_tabWidget;

    // Camera tab
    QSpinBox *_cameraDeviceSpin;
    QComboBox *_resolutionCombo;
    QSpinBox *_cameraFpsSpin;
    QCheckBox *_cameraMirrorCheck;

    // AI tab
    QComboBox *_aiProviderCombo;
    QLabel *_activeProviderLabel;
    QDoubleSpinBox *_confThresholdSpin;
    QDoubleSpinBox *_kptThresholdSpin;
    QComboBox *_targetSelectionCombo;

    // Retargeting tab
    QDoubleSpinBox *_smoothingAlphaSpin;
    QDoubleSpinBox *_deadbandDegSpin;
    QDoubleSpinBox *_maxVelocitySpin;
    QComboBox *_retargetModeCombo;
    QComboBox *_stancePresetCombo;

    // Safety tab
    QDoubleSpinBox *_safetyMarginSpin;
    QSpinBox *_txRateHzSpin;
    QSpinBox *_watchdogTimeoutSpin;

    // MQTT tab
    QLineEdit *_mqttHostEdit;
    QSpinBox *_mqttPortSpin;
    QLineEdit *_mqttUsernameEdit;
    QLineEdit *_mqttPasswordEdit;
    QLineEdit *_mqttRobotIdEdit;
    QSpinBox *_mqttKeepaliveSpin;
    QCheckBox *_mqttAutoConnectCheck;

    // View tab
    QCheckBox *_viewMeshesCheck;
    QCheckBox *_viewSkeletonCheck;
    QCheckBox *_viewGridCheck;
    QCheckBox *_viewDarkThemeCheck;

    // Dialog buttons
    QPushButton *_okBtn;
    QPushButton *_cancelBtn;
    QPushButton *_applyBtn;
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
