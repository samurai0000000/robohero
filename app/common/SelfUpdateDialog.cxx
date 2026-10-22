/*
 * SelfUpdateDialog.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "SelfUpdateDialog.hxx"
#include "Version.hxx"
#include <selfupdate/QtSelfUpdateBridge.hxx>
#include <QCoreApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMetaObject>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

namespace robohero {

namespace {

std::vector<SelfUpdateDialog *> &openDialogs()
{
    static std::vector<SelfUpdateDialog *> s_dialogs;
    return s_dialogs;
}

} // namespace

SelfUpdateDialog::SelfUpdateDialog(
    std::shared_ptr<selfupdate::SelfUpdateController> controller,
    const QString &appTitle,
    QWidget *parent)
    : QDialog(parent)
    , _controllerShared(std::move(controller))
    , _controller(_controllerShared.get())
    , _appTitle(appTitle.isEmpty() ? QString("RoboHero") : appTitle)
    , _boundStandalone(false)
    , _currentState()
    , _titleLabel(nullptr)
    , _phaseBadgeLabel(nullptr)
    , _versionSummaryLabel(nullptr)
    , _metadataLabel(nullptr)
    , _releaseNotesView(nullptr)
    , _progressBar(nullptr)
    , _statusLabel(nullptr)
    , _checkBtn(nullptr)
    , _downloadBtn(nullptr)
    , _installBtn(nullptr)
    , _cancelBtn(nullptr)
    , _closeBtn(nullptr)
{
    initUi();
}

SelfUpdateDialog::SelfUpdateDialog(
    selfupdate::SelfUpdateController *controller,
    const QString &appTitle,
    QWidget *parent)
    : QDialog(parent)
    , _controllerShared(nullptr)
    , _controller(controller)
    , _appTitle(appTitle.isEmpty() ? QString("RoboHero") : appTitle)
    , _boundStandalone(false)
    , _currentState()
    , _titleLabel(nullptr)
    , _phaseBadgeLabel(nullptr)
    , _versionSummaryLabel(nullptr)
    , _metadataLabel(nullptr)
    , _releaseNotesView(nullptr)
    , _progressBar(nullptr)
    , _statusLabel(nullptr)
    , _checkBtn(nullptr)
    , _downloadBtn(nullptr)
    , _installBtn(nullptr)
    , _cancelBtn(nullptr)
    , _closeBtn(nullptr)
{
    initUi();
}

SelfUpdateDialog::~SelfUpdateDialog()
{
    auto &list = openDialogs();
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
    if (_boundStandalone && _controller != nullptr) {
        _controller->setStateCallback(nullptr);
    }
}

void SelfUpdateDialog::initUi()
{
    openDialogs().push_back(this);

    setWindowTitle(QString("Software Update — %1").arg(_appTitle));
    setMinimumSize(540, 460);
    resize(580, 500);

    setStyleSheet(
        "QDialog { background-color: #1e1e24; color: #e0e0e0; }"
        "QLabel { color: #e0e0e0; }"
        "QGroupBox { color: #e0e0e0; border: 1px solid #444; margin-top: 8px; font-weight: bold; }"
        "QGroupBox::title { color: #00ADB5; }"
        "QTextEdit { background-color: #16161a; color: #e0e0e0; border: 1px solid #444; }"
        "QProgressBar { border: 1px solid #444; border-radius: 4px; text-align: center; background-color: #16161a; color: #ffffff; }"
        "QProgressBar::chunk { background-color: #00ADB5; }"
        "QPushButton { background-color: #33333d; color: #fff; border: 1px solid #4a4a55; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #3d3d4a; border-color: #00ADB5; }"
        "QPushButton:disabled { background-color: #25252c; color: #666; border-color: #333; }"
    );

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(10);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(2);
    _titleLabel = new QLabel(_appTitle, this);
    _titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00ADB5;");
    _versionSummaryLabel = new QLabel(this);
    _versionSummaryLabel->setStyleSheet("font-size: 13px;");
    headerTextLayout->addWidget(_titleLabel);
    headerTextLayout->addWidget(_versionSummaryLabel);
    headerRow->addLayout(headerTextLayout, 1);

    _phaseBadgeLabel = new QLabel("Idle", this);
    _phaseBadgeLabel->setStyleSheet(
        "font-size: 11px; font-weight: bold; padding: 4px 8px; "
        "border-radius: 4px; border: 1px solid #00ADB5; color: #00ADB5;");
    headerRow->addWidget(_phaseBadgeLabel, 0, Qt::AlignTop);
    rootLayout->addLayout(headerRow);

    _metadataLabel = new QLabel(this);
    _metadataLabel->setWordWrap(true);
    _metadataLabel->setStyleSheet("font-size: 11px; color: #aaaaaa;");
    rootLayout->addWidget(_metadataLabel);

    auto *notesBox = new QGroupBox("Release Notes", this);
    auto *notesLayout = new QVBoxLayout(notesBox);
    _releaseNotesView = new QTextEdit(notesBox);
    _releaseNotesView->setReadOnly(true);
    notesLayout->addWidget(_releaseNotesView);
    rootLayout->addWidget(notesBox, 1);

    _progressBar = new QProgressBar(this);
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);
    _progressBar->setFixedHeight(20);
    rootLayout->addWidget(_progressBar);

    _statusLabel = new QLabel(this);
    _statusLabel->setWordWrap(true);
    _statusLabel->setStyleSheet("font-size: 12px;");
    rootLayout->addWidget(_statusLabel);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    _checkBtn = new QPushButton("Check Now", this);
    _downloadBtn = new QPushButton("Download Update", this);
    _installBtn = new QPushButton("Install & Restart", this);
    _cancelBtn = new QPushButton("Cancel", this);
    _closeBtn = new QPushButton("Close", this);

    btnRow->addWidget(_checkBtn);
    btnRow->addWidget(_downloadBtn);
    btnRow->addWidget(_installBtn);
    btnRow->addWidget(_cancelBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(_closeBtn);
    rootLayout->addLayout(btnRow);

    QObject::connect(_checkBtn, &QPushButton::clicked, this, [this]() {
        triggerCheckForUpdates();
    });
    QObject::connect(_downloadBtn, &QPushButton::clicked, this, [this]() {
        triggerDownloadUpdate();
    });
    QObject::connect(_installBtn, &QPushButton::clicked, this, [this]() {
        triggerInstallUpdate();
    });
    QObject::connect(_cancelBtn, &QPushButton::clicked, this, [this]() {
        triggerCancel();
    });
    QObject::connect(_closeBtn, &QPushButton::clicked, this, [this]() {
        reject();
    });

    if (_controller != nullptr) {
        if (parentWidget() == nullptr) {
            _boundStandalone = true;
            selfupdate::bindQtController(
                *_controller,
                this,
                [this](const selfupdate::UpdateState &st) {
                    applyUpdateState(st);
                });
        }
        applyUpdateState(_controller->state());
    } else {
        applyUpdateState(_currentState);
    }
}

void SelfUpdateDialog::broadcastState(
    const selfupdate::SelfUpdateController *controller,
    const selfupdate::UpdateState &state)
{
    auto &list = openDialogs();
    for (SelfUpdateDialog *dlg : list) {
        if (dlg != nullptr && dlg->_controller == controller) {
            dlg->applyUpdateState(state);
        }
    }
}

void SelfUpdateDialog::onUpdateStateChanged(const selfupdate::UpdateState &state)
{
    if (QCoreApplication::instance() != nullptr &&
        QThread::currentThread() != QCoreApplication::instance()->thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, state]() {
                applyUpdateState(state);
            },
            Qt::QueuedConnection);
        return;
    }
    applyUpdateState(state);
}

void SelfUpdateDialog::applyUpdateState(const selfupdate::UpdateState &state)
{
    _currentState = state;

    QString installedVer = QString::fromStdString(
        state.installedVersionName.empty() ? std::string(ROBOHERO_VERSION_STRING)
                                           : state.installedVersionName);

    switch (state.phase) {
    case selfupdate::UpdatePhase::Idle:
        _phaseBadgeLabel->setText("Idle");
        _versionSummaryLabel->setText(
            QString("Installed: v%1 (Build %2)")
                .arg(installedVer)
                .arg(state.installedVersionCode));
        _metadataLabel->setText("");
        _releaseNotesView->setPlainText("No release notes.");
        _statusLabel->setText("Ready to check for updates.");
        _checkBtn->setEnabled(true);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(false);
        _progressBar->setValue(0);
        break;

    case selfupdate::UpdatePhase::Checking:
        _phaseBadgeLabel->setText("Checking");
        _versionSummaryLabel->setText(
            QString("Installed: v%1 (Build %2)")
                .arg(installedVer)
                .arg(state.installedVersionCode));
        _statusLabel->setText("Querying Selfso Windows release catalog...");
        _checkBtn->setEnabled(false);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(true);
        break;

    case selfupdate::UpdatePhase::UpToDate:
        _phaseBadgeLabel->setText("UpToDate");
        _versionSummaryLabel->setText(
            QString("Installed: v%1 (Latest)").arg(installedVer));
        _statusLabel->setText(
            QString("%1 v%2 is up to date.").arg(_appTitle).arg(installedVer));
        _checkBtn->setEnabled(true);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(false);
        _progressBar->setValue(100);
        break;

    case selfupdate::UpdatePhase::Available: {
        _phaseBadgeLabel->setText("Available");
        QString availVer = QString::fromStdString(state.manifest.versionName);
        _versionSummaryLabel->setText(
            QString("Installed: v%1 -> Available: v%2 (Build %3)")
                .arg(installedVer)
                .arg(availVer)
                .arg(state.manifest.versionCode));

        QString modeStr = (state.manifest.installMode == selfupdate::InstallMode::Installer)
                              ? "Setup Installer"
                              : "Standalone Binary";
        QString sizeStr = (state.manifest.sizeBytes > 0)
                              ? QString("%1 KB (%2 bytes)")
                                    .arg(state.manifest.sizeBytes / 1024)
                                    .arg(state.manifest.sizeBytes)
                              : "Unknown size";
        _metadataLabel->setText(
            QString("Mode: %1 | Architecture: %2 | Minimum OS: %3\nSize: %4 | SHA-256: %5")
                .arg(modeStr)
                .arg(QString::fromStdString(state.manifest.arch))
                .arg(QString::fromStdString(state.manifest.minOs))
                .arg(sizeStr)
                .arg(QString::fromStdString(state.manifest.sha256)));

        if (!state.manifest.releaseNotes.empty()) {
            _releaseNotesView->setPlainText(
                QString::fromStdString(state.manifest.releaseNotes));
        } else {
            _releaseNotesView->setPlainText("No release notes provided.");
        }

        _statusLabel->setText(
            QString("Update v%1 is available for download.").arg(availVer));
        _checkBtn->setEnabled(true);
        _downloadBtn->setEnabled(true);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(false);
        _progressBar->setValue(0);
        break;
    }

    case selfupdate::UpdatePhase::Downloading: {
        _phaseBadgeLabel->setText("Downloading");
        int pct = 0;
        double frac = state.progressFraction();
        if (frac >= 0.0) {
            pct = std::clamp(static_cast<int>(frac * 100.0), 0, 100);
        }
        _progressBar->setValue(pct);
        _statusLabel->setText(
            QString("Downloading update package... %1% (%2 / %3 bytes)")
                .arg(pct)
                .arg(state.bytesRead)
                .arg(state.totalBytes > 0 ? state.totalBytes : 0));
        _checkBtn->setEnabled(false);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(true);
        break;
    }

    case selfupdate::UpdatePhase::ReadyToInstall: {
        _phaseBadgeLabel->setText("ReadyToInstall");
        QString availVer = QString::fromStdString(state.manifest.versionName);
        if (!availVer.isEmpty()) {
            _versionSummaryLabel->setText(
                QString("Installed: v%1 -> Ready: v%2 (Build %3)")
                    .arg(installedVer)
                    .arg(availVer)
                    .arg(state.manifest.versionCode));
        }
        if (!state.manifest.releaseNotes.empty()) {
            _releaseNotesView->setPlainText(
                QString::fromStdString(state.manifest.releaseNotes));
        }
        _progressBar->setValue(100);
        _statusLabel->setText("Package verified (SHA-256 OK). Ready to install.");
        _checkBtn->setEnabled(true);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(true);
        _cancelBtn->setEnabled(false);
        break;
    }

    case selfupdate::UpdatePhase::Installing:
        _phaseBadgeLabel->setText("LaunchingInstaller");
        _statusLabel->setText("Launching update installer...");
        _checkBtn->setEnabled(false);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(false);
        break;

    case selfupdate::UpdatePhase::Relaunching:
        _phaseBadgeLabel->setText("LaunchingInstaller");
        _progressBar->setValue(100);
        _statusLabel->setText(
            QString("Update staged. Closing %1 to complete installation and relaunch...")
                .arg(_appTitle));
        _checkBtn->setEnabled(false);
        _downloadBtn->setEnabled(false);
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(false);
        _closeBtn->setEnabled(false);
        if (isVisible()) {
            accept();
        }
        break;

    case selfupdate::UpdatePhase::Error:
        _phaseBadgeLabel->setText("Error");
        _statusLabel->setText(
            QString("Error: %1").arg(QString::fromStdString(state.errorMessage)));
        _checkBtn->setEnabled(true);
        _downloadBtn->setEnabled(!state.manifest.packageUrl.empty());
        _installBtn->setEnabled(false);
        _cancelBtn->setEnabled(false);
        break;
    }
}

const selfupdate::UpdateState &SelfUpdateDialog::currentUpdateState() const
{
    return _currentState;
}

QString SelfUpdateDialog::phaseLabelText() const
{
    return _phaseBadgeLabel ? _phaseBadgeLabel->text() : QString();
}

QString SelfUpdateDialog::phaseBadgeText() const
{
    return phaseLabelText();
}

QString SelfUpdateDialog::versionLabelText() const
{
    return _versionSummaryLabel ? _versionSummaryLabel->text() : QString();
}

QString SelfUpdateDialog::versionSummaryText() const
{
    return versionLabelText();
}

QString SelfUpdateDialog::releaseNotesText() const
{
    return _releaseNotesView ? _releaseNotesView->toPlainText() : QString();
}

QString SelfUpdateDialog::metadataSummaryText() const
{
    return _metadataLabel ? _metadataLabel->text() : QString();
}

QString SelfUpdateDialog::statusText() const
{
    return _statusLabel ? _statusLabel->text() : QString();
}

int SelfUpdateDialog::progressValue() const
{
    return _progressBar ? _progressBar->value() : 0;
}

int SelfUpdateDialog::progressPercent() const
{
    return progressValue();
}

bool SelfUpdateDialog::isDownloadButtonEnabled() const
{
    return _downloadBtn ? _downloadBtn->isEnabled() : false;
}

bool SelfUpdateDialog::isInstallButtonEnabled() const
{
    return _installBtn ? _installBtn->isEnabled() : false;
}

bool SelfUpdateDialog::isCancelButtonEnabled() const
{
    return _cancelBtn ? _cancelBtn->isEnabled() : false;
}

void SelfUpdateDialog::triggerCheckForUpdates()
{
    if (_controller != nullptr) {
        _controller->checkForUpdates(true);
    }
}

void SelfUpdateDialog::triggerDownloadUpdate()
{
    if (_controller != nullptr) {
        _controller->downloadPackage();
    }
}

void SelfUpdateDialog::triggerInstallUpdate()
{
    if (_controller != nullptr) {
        _controller->installStagedPackage();
    }
}

void SelfUpdateDialog::triggerDownloadAndInstall()
{
    if (_controller != nullptr) {
        if (_currentState.phase == selfupdate::UpdatePhase::ReadyToInstall) {
            _controller->installStagedPackage();
        } else {
            _controller->downloadAndInstall();
        }
    }
}

void SelfUpdateDialog::triggerCancel()
{
    if (_controller != nullptr) {
        _controller->cancel();
    }
}

void SelfUpdateDialog::triggerPrimaryAction()
{
    if (_controller == nullptr) {
        return;
    }
    switch (_currentState.phase) {
    case selfupdate::UpdatePhase::Available:
        triggerDownloadUpdate();
        break;
    case selfupdate::UpdatePhase::ReadyToInstall:
        triggerInstallUpdate();
        break;
    case selfupdate::UpdatePhase::Idle:
    case selfupdate::UpdatePhase::UpToDate:
    case selfupdate::UpdatePhase::Error:
    default:
        triggerCheckForUpdates();
        break;
    }
}

} // namespace robohero

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
