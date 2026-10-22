/*
 * TestMotionSelfUpdate.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <QApplication>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <selfupdate/HttpTransport.hxx>
#include <selfupdate/SelfUpdateController.hxx>
#include <selfupdate/Sha256.hxx>
#include "MainWindow.hxx"
#include "SelfUpdateDialog.hxx"
#include "UrdfLimits.hxx"
#include "Version.hxx"

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/MemoryLeakWarningPlugin.h>
#include <CppUTest/TestHarness.h>

__attribute__((constructor(101))) static void disableCppUTestNewDeleteOverloads()
{
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
}

extern "C" const char *__lsan_default_suppressions()
{
    return "leak:libfontconfig\n"
           "leak:libglib-2.0\n"
           "leak:libQt6Gui\n"
           "leak:libQt6Core\n"
           "leak:libqoffscreen\n"
           "leak:libdbus-1\n";
}

namespace {

class MockHttpTransport : public selfupdate::HttpTransport {
public:
    void setManifestResponse(int statusCode,
                             const std::string &body,
                             const std::string &errorMsg = "")
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _manifestStatus = statusCode;
        _manifestBody = body;
        _manifestError = errorMsg;
    }

    void setDownloadPayload(bool succeed,
                            const std::string &payload,
                            const std::string &errorMsg = "")
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _downloadSucceed = succeed;
        _downloadPayload = payload;
        _downloadError = errorMsg;
    }

    selfupdate::HttpResponse get(const std::string &url,
                                 int connectTimeoutMs,
                                 int readTimeoutMs,
                                 const std::atomic<bool> *cancelFlag) override
    {
        (void)url;
        (void)connectTimeoutMs;
        (void)readTimeoutMs;
        (void)cancelFlag;
        std::lock_guard<std::mutex> lock(_mutex);
        selfupdate::HttpResponse res;
        res.statusCode = _manifestStatus;
        res.body = _manifestBody;
        res.errorMessage = _manifestError;
        return res;
    }

    bool downloadStream(const std::string &url,
                        selfupdate::StreamChunkCallback onChunk,
                        int connectTimeoutMs,
                        int readTimeoutMs,
                        const std::atomic<bool> *cancelFlag,
                        std::string *outError) override
    {
        (void)url;
        (void)connectTimeoutMs;
        (void)readTimeoutMs;
        std::string payload;
        bool succeed = false;
        std::string err;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            payload = _downloadPayload;
            succeed = _downloadSucceed;
            err = _downloadError;
        }
        if (!succeed) {
            if (outError) {
                *outError = err.empty() ? "Simulated stream failure" : err;
            }
            return false;
        }

        const int64_t total = static_cast<int64_t>(payload.size());
        const size_t half = payload.size() / 2;
        if (half > 0) {
            if (cancelFlag && cancelFlag->load()) {
                return false;
            }
            if (!onChunk(payload.data(), half, total)) {
                return false;
            }
        }
        const size_t rem = payload.size() - half;
        if (rem > 0) {
            if (cancelFlag && cancelFlag->load()) {
                return false;
            }
            if (!onChunk(payload.data() + half, rem, total)) {
                return false;
            }
        }
        return true;
    }

private:
    std::mutex _mutex;
    int _manifestStatus = 200;
    std::string _manifestBody;
    std::string _manifestError;
    bool _downloadSucceed = true;
    std::string _downloadPayload;
    std::string _downloadError;
};

std::string newerVersionName()
{
    return std::to_string(robohero::ROBOHERO_VERSION_MAJOR) + "." +
           std::to_string(robohero::ROBOHERO_VERSION_MINOR) + "." +
           std::to_string(robohero::ROBOHERO_VERSION_PATCH + 1);
}

constexpr int newerVersionCode()
{
    return robohero::ROBOHERO_VERSION_CODE + 1;
}

std::string buildCatalogJson(const std::string &versionName,
                             int versionCode,
                             const std::string &sha256,
                             int64_t sizeBytes,
                             const std::string &releaseNotes = "Keyframe & gait engine improvements.")
{
    return std::string("{\n") +
           "  \"schemaVersion\": 1,\n"
           "  \"platform\": \"windows\",\n"
           "  \"baseUrl\": \"https://www.selfso.com/software/windows/\",\n"
           "  \"apps\": {\n"
           "    \"robohero-motion\": {\n"
           "      \"slug\": \"robohero-motion\",\n"
           "      \"title\": \"RoboHero Motion\",\n"
           "      \"applicationId\": \"com.selfso.robohero.motion\",\n"
           "      \"versionCode\": " + std::to_string(versionCode) + ",\n"
           "      \"versionName\": \"" + versionName + "\",\n"
           "      \"installType\": \"installer\",\n"
           "      \"installerArgs\": \"/S\",\n"
           "      \"executableName\": \"robohero_motion.exe\",\n"
           "      \"arch\": \"x86_64\",\n"
           "      \"minOs\": \"Windows 10 (64-bit)+\",\n"
           "      \"packageUrl\": \"https://www.selfso.com/software/windows/robohero-motion/robohero_motion-setup.exe\",\n"
           "      \"sha256\": \"" + sha256 + "\",\n"
           "      \"sizeBytes\": " + std::to_string(sizeBytes) + ",\n"
           "      \"releaseNotes\": \"" + releaseNotes + "\"\n"
           "    }\n"
           "  }\n"
           "}\n";
}

} // namespace

TEST_GROUP(MotionSelfUpdateTestGroup)
{
    std::shared_ptr<MockHttpTransport> transport;
    std::unique_ptr<QTemporaryDir> tempDir;

    void setup() override
    {
        transport = std::make_shared<MockHttpTransport>();
        tempDir = std::make_unique<QTemporaryDir>();
        CHECK(tempDir->isValid());
    }

    void teardown() override
    {
        QCoreApplication::processEvents();
        tempDir.reset();
        transport.reset();
    }
};

TEST(MotionSelfUpdateTestGroup, DefaultConfigMatchesCanonicalMetadata)
{
    const selfupdate::AppConfig cfg = MainWindow::defaultSelfUpdateConfig();
    STRCMP_EQUAL("com.selfso.robohero.motion", cfg.appId.c_str());
    STRCMP_EQUAL("com.selfso.robohero.motion", cfg.applicationId.c_str());
    STRCMP_EQUAL("robohero-motion", cfg.appSlug.c_str());
    STRCMP_EQUAL("robohero-motion", cfg.slug.c_str());
    STRCMP_EQUAL("RoboHero Motion", cfg.displayName.c_str());
    STRCMP_EQUAL(robohero::ROBOHERO_VERSION_STRING, cfg.currentVersion.c_str());
    STRCMP_EQUAL(robohero::ROBOHERO_VERSION_STRING, cfg.installedVersionName.c_str());
    LONGS_EQUAL(robohero::ROBOHERO_VERSION_CODE, cfg.currentVersionCode);
    LONGS_EQUAL(robohero::ROBOHERO_VERSION_CODE, cfg.installedVersionCode);
    STRCMP_EQUAL("https://www.selfso.com/software/windows/version.json",
                 cfg.manifestUrl.c_str());
    CHECK_TRUE(cfg.checkOnStartup);
}

TEST(MotionSelfUpdateTestGroup, NominalAndBoundaryVersionChecks)
{
    const std::string dummyPayload = "MZ-mock-robohero-motion-installer-payload";
    const std::string sha = selfupdate::Sha256::hashBytes(
        dummyPayload.data(), dummyPayload.size());

    selfupdate::InstallOptions installOpts;
    installOpts.dryRun = true;

    auto controller = std::make_shared<selfupdate::SelfUpdateController>(
        MainWindow::defaultSelfUpdateConfig(),
        installOpts,
        tempDir->path().toStdString(),
        transport);

    // 1. Newer version -> Available
    const std::string newerVer = newerVersionName();
    const int newerCode = newerVersionCode();
    transport->setManifestResponse(
        200,
        buildCatalogJson(newerVer, newerCode, sha, static_cast<int64_t>(dummyPayload.size())));
    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(controller->state().phase));
    STRCMP_EQUAL(newerVer.c_str(), controller->state().manifest.versionName.c_str());
    LONGS_EQUAL(newerCode, controller->state().manifest.versionCode);

    // 2. Equal version -> UpToDate
    transport->setManifestResponse(
        200,
        buildCatalogJson(robohero::ROBOHERO_VERSION_STRING,
                         robohero::ROBOHERO_VERSION_CODE,
                         sha,
                         static_cast<int64_t>(dummyPayload.size())));
    controller->checkForUpdates(false);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::UpToDate),
                static_cast<int>(controller->state().phase));

    // 3. Older version (2.1.23 / 20123) -> UpToDate
    transport->setManifestResponse(
        200,
        buildCatalogJson("2.1.23", 20123, sha, static_cast<int64_t>(dummyPayload.size())));
    controller->checkForUpdates(false);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::UpToDate),
                static_cast<int>(controller->state().phase));
}

TEST(MotionSelfUpdateTestGroup, DownloadVerifyAndDryRunInstallSucceeds)
{
    const std::string installerBytes = "MZ-verified-robohero-motion-setup-2.2.2";
    const std::string sha = selfupdate::Sha256::hashBytes(
        installerBytes.data(), installerBytes.size());

    transport->setManifestResponse(
        200,
        buildCatalogJson(newerVersionName(), newerVersionCode(), sha,
                         static_cast<int64_t>(installerBytes.size()),
                         "Added parametric sidestep gait."));
    transport->setDownloadPayload(true, installerBytes);

    selfupdate::InstallOptions installOpts;
    installOpts.dryRun = true;

    auto controller = std::make_shared<selfupdate::SelfUpdateController>(
        MainWindow::defaultSelfUpdateConfig(),
        installOpts,
        tempDir->path().toStdString(),
        transport);

    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(controller->state().phase));

    controller->downloadPackage();
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::ReadyToInstall),
                static_cast<int>(controller->state().phase));
    CHECK_FALSE(controller->state().stagedPackagePath.empty());
    CHECK_TRUE(std::filesystem::exists(controller->state().stagedPackagePath));

    controller->installStagedPackage();
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Relaunching),
                static_cast<int>(controller->state().phase));
    CHECK_TRUE(controller->lastInstallResult().success);
}

TEST(MotionSelfUpdateTestGroup, FaultInjectionHttpErrorMalformedJsonAndShaMismatch)
{
    const std::string validBytes = "MZ-expected-installer-binary";
    const std::string validSha = selfupdate::Sha256::hashBytes(
        validBytes.data(), validBytes.size());

    selfupdate::InstallOptions installOpts;
    installOpts.dryRun = true;

    auto controller = std::make_shared<selfupdate::SelfUpdateController>(
        MainWindow::defaultSelfUpdateConfig(),
        installOpts,
        tempDir->path().toStdString(),
        transport);

    // 1. HTTP 500 error
    transport->setManifestResponse(500, "", "Internal Server Error");
    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Error),
                static_cast<int>(controller->state().phase));
    CHECK_TRUE(controller->state().errorMessage.find("500") != std::string::npos);

    // 2. Malformed JSON manifest
    transport->setManifestResponse(200, "{ malformed json ");
    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Error),
                static_cast<int>(controller->state().phase));

    // 3. Corrupted download payload (SHA-256 mismatch) cleans up .part file
    transport->setManifestResponse(
        200,
        buildCatalogJson(newerVersionName(), newerVersionCode(), validSha,
                         static_cast<int64_t>(validBytes.size())));
    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(controller->state().phase));

    const std::string corruptedBytes = "MZ-corrupted-installer-binary";
    transport->setDownloadPayload(true, corruptedBytes);
    controller->downloadPackage();
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Error),
                static_cast<int>(controller->state().phase));

    for (const auto &entry : std::filesystem::directory_iterator(tempDir->path().toStdString())) {
        CHECK_FALSE(entry.path().extension() == ".part");
    }
}

TEST(MotionSelfUpdateTestGroup, MainWindowAndDialogIntegrationOffscreen)
{
    const std::string payload = "MZ-robohero-motion-setup-2.2.2";
    const std::string sha = selfupdate::Sha256::hashBytes(payload.data(), payload.size());

    const std::string newerVer = newerVersionName();
    transport->setManifestResponse(
        200,
        buildCatalogJson(newerVer, newerVersionCode(), sha,
                         static_cast<int64_t>(payload.size()),
                         "New inverse kinematics solver."));
    transport->setDownloadPayload(true, payload);

    selfupdate::InstallOptions installOpts;
    installOpts.dryRun = true;

    auto controller = std::make_shared<selfupdate::SelfUpdateController>(
        MainWindow::defaultSelfUpdateConfig(),
        installOpts,
        tempDir->path().toStdString(),
        transport);

    MainWindow mainWindow(controller);
    robohero::SelfUpdateDialog dialog(controller, "RoboHero Motion", &mainWindow);

    STRCMP_EQUAL("Idle", dialog.phaseLabelText().toUtf8().constData());
    CHECK_FALSE(mainWindow.isUpdateStatusBadgeVisible());

    // Process startup check scheduled via QTimer::singleShot(0, ...)
    QCoreApplication::processEvents();
    controller->waitForIdle();
    QCoreApplication::processEvents();

    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(mainWindow.currentSelfUpdateState().phase));
    CHECK_TRUE(mainWindow.isUpdateStatusBadgeVisible());
    CHECK_TRUE(mainWindow.updateStatusBadgeText().contains(QString::fromStdString(newerVer)));

    STRCMP_EQUAL("Available", dialog.phaseLabelText().toUtf8().constData());
    CHECK_TRUE(dialog.versionLabelText().contains(QString::fromStdString(newerVer)));
    CHECK_TRUE(dialog.releaseNotesText().contains("inverse kinematics"));

    // Trigger primary action in Available state -> downloads package
    dialog.triggerPrimaryAction();
    controller->waitForIdle();
    QCoreApplication::processEvents();

    STRCMP_EQUAL("ReadyToInstall", dialog.phaseLabelText().toUtf8().constData());
    LONGS_EQUAL(100, dialog.progressValue());
    CHECK_TRUE(mainWindow.updateStatusBadgeText().contains("Ready to Install"));
}

int main(int argc, char **argv)
{
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("ROBOHERO_DISABLE_AUTO_MQTT", "1");
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(motion_resources);
    UrdfLimits::instance().init(":/model/robohero.urdf", ":/model/calibration.json");
    return CommandLineTestRunner::RunAllTests(argc, argv);
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
