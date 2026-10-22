/*
 * SelfUpdateDialog.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_SELF_UPDATE_DIALOG_HXX
#define ROBOHERO_SELF_UPDATE_DIALOG_HXX

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <memory>
#include <string>
#include <selfupdate/SelfUpdateController.hxx>

namespace selfupdate {

struct AppConfig : public CheckerConfig {
    std::string appId;
    std::string appSlug;
    std::string slug;
    std::string displayName;
    std::string currentVersion;
    int64_t currentVersionCode = 0;
    bool checkOnStartup = true;

    CheckerConfig toCheckerConfig() const
    {
        CheckerConfig cfg = *this;
        if (cfg.applicationId.empty()) {
            if (!appId.empty()) {
                cfg.applicationId = appId;
            } else if (!appSlug.empty()) {
                cfg.applicationId = appSlug;
            } else {
                cfg.applicationId = slug;
            }
        }
        if (cfg.installedVersionName.empty()) {
            cfg.installedVersionName = currentVersion;
        }
        if (cfg.installedVersionCode <= 0) {
            cfg.installedVersionCode = currentVersionCode;
        }
        return cfg;
    }
};

} // namespace selfupdate

namespace robohero {

class SelfUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SelfUpdateDialog(
        std::shared_ptr<selfupdate::SelfUpdateController> controller,
        const QString &appTitle = "RoboHero",
        QWidget *parent = nullptr);
    explicit SelfUpdateDialog(
        selfupdate::SelfUpdateController *controller,
        const QString &appTitle = "RoboHero",
        QWidget *parent = nullptr);
    ~SelfUpdateDialog() override;

    void applyUpdateState(const selfupdate::UpdateState &state);
    void onUpdateStateChanged(const selfupdate::UpdateState &state);

    static void broadcastState(
        const selfupdate::SelfUpdateController *controller,
        const selfupdate::UpdateState &state);

    const selfupdate::UpdateState &currentUpdateState() const;

    QString phaseLabelText() const;
    QString phaseBadgeText() const;
    QString versionLabelText() const;
    QString versionSummaryText() const;
    QString releaseNotesText() const;
    QString metadataSummaryText() const;
    QString statusText() const;
    int progressValue() const;
    int progressPercent() const;

    bool isDownloadButtonEnabled() const;
    bool isInstallButtonEnabled() const;
    bool isCancelButtonEnabled() const;

    void triggerCheckForUpdates();
    void triggerDownloadUpdate();
    void triggerInstallUpdate();
    void triggerDownloadAndInstall();
    void triggerCancel();
    void triggerPrimaryAction();

private:
    void initUi();

    std::shared_ptr<selfupdate::SelfUpdateController> _controllerShared;
    selfupdate::SelfUpdateController *_controller;
    QString _appTitle;
    bool _boundStandalone;
    selfupdate::UpdateState _currentState;

    QLabel *_titleLabel;
    QLabel *_phaseBadgeLabel;
    QLabel *_versionSummaryLabel;
    QLabel *_metadataLabel;
    QTextEdit *_releaseNotesView;
    QProgressBar *_progressBar;
    QLabel *_statusLabel;

    QPushButton *_checkBtn;
    QPushButton *_downloadBtn;
    QPushButton *_installBtn;
    QPushButton *_cancelBtn;
    QPushButton *_closeBtn;
};

} // namespace robohero

using robohero::SelfUpdateDialog;

#endif // ROBOHERO_SELF_UPDATE_DIALOG_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
