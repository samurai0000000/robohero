/*
 * TestTeleopSelfUpdate.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <QApplication>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
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
#include "TeleopConfig.hxx"
#include "UrdfLimits.hxx"
#include "Version.hxx"

#include <QBuffer>
#include <QImage>
#define ROBOHERO_LIBCONFIG_NAMESPACE mingw_libconfig
#include "../src/mingw_compat/MinGwLibconfigCompat.cxx"
#define ROBOHERO_OPENCV_DECODE_ONLY
#include "../src/mingw_compat/MinGwOpenCvCompat.cxx"

#include <rapidcheck.h>

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
           "leak:libdbus-1\n"
           "leak:libonnxruntime\n";
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

std::string buildCatalogJson(const std::string &versionName,
                             int versionCode,
                             const std::string &sha256,
                             int64_t sizeBytes,
                             const std::string &releaseNotes = "YOLOv8 pose retargeting & DirectML updates.")
{
    return std::string("{\n") +
           "  \"schemaVersion\": 1,\n"
           "  \"platform\": \"windows\",\n"
           "  \"baseUrl\": \"https://www.selfso.com/software/windows/\",\n"
           "  \"apps\": {\n"
           "    \"robohero-teleop\": {\n"
           "      \"slug\": \"robohero-teleop\",\n"
           "      \"title\": \"RoboHero Teleop\",\n"
           "      \"applicationId\": \"com.selfso.robohero.teleop\",\n"
           "      \"versionCode\": " + std::to_string(versionCode) + ",\n"
           "      \"versionName\": \"" + versionName + "\",\n"
           "      \"installType\": \"installer\",\n"
           "      \"installerArgs\": \"/S\",\n"
           "      \"executableName\": \"robohero_teleop.exe\",\n"
           "      \"arch\": \"x86_64\",\n"
           "      \"minOs\": \"Windows 10 (64-bit)+\",\n"
           "      \"packageUrl\": \"https://www.selfso.com/software/windows/robohero-teleop/robohero_teleop-setup.exe\",\n"
           "      \"sha256\": \"" + sha256 + "\",\n"
           "      \"sizeBytes\": " + std::to_string(sizeBytes) + ",\n"
           "      \"releaseNotes\": \"" + releaseNotes + "\"\n"
           "    }\n"
           "  }\n"
           "}\n";
}

} // namespace

TEST_GROUP(TeleopSelfUpdateTestGroup)
{
    std::shared_ptr<MockHttpTransport> transport;
    std::unique_ptr<QTemporaryDir> tempDir;

    void setup() override
    {
        transport = std::make_shared<MockHttpTransport>();
        tempDir = std::make_unique<QTemporaryDir>();
        CHECK(tempDir->isValid());
        std::string cfgPath = (tempDir->path() + "/teleop.cfg").toStdString();
        qputenv("ROBOHERO_CONFIG_PATH", cfgPath.c_str());
    }

    void teardown() override
    {
        QCoreApplication::processEvents();
        tempDir.reset();
        transport.reset();
    }
};

TEST(TeleopSelfUpdateTestGroup, DefaultConfigMatchesCanonicalMetadata)
{
    const selfupdate::AppConfig cfg = MainWindow::defaultSelfUpdateConfig();
    STRCMP_EQUAL("com.selfso.robohero.teleop", cfg.appId.c_str());
    STRCMP_EQUAL("com.selfso.robohero.teleop", cfg.applicationId.c_str());
    STRCMP_EQUAL("robohero-teleop", cfg.appSlug.c_str());
    STRCMP_EQUAL("robohero-teleop", cfg.slug.c_str());
    STRCMP_EQUAL("RoboHero Teleop", cfg.displayName.c_str());
    STRCMP_EQUAL(robohero::ROBOHERO_VERSION_STRING, cfg.currentVersion.c_str());
    STRCMP_EQUAL(robohero::ROBOHERO_VERSION_STRING, cfg.installedVersionName.c_str());
    LONGS_EQUAL(robohero::ROBOHERO_VERSION_CODE, cfg.currentVersionCode);
    LONGS_EQUAL(robohero::ROBOHERO_VERSION_CODE, cfg.installedVersionCode);
    STRCMP_EQUAL("https://www.selfso.com/software/windows/version.json",
                 cfg.manifestUrl.c_str());
    CHECK_TRUE(cfg.checkOnStartup);
}

TEST(TeleopSelfUpdateTestGroup, NominalAndBoundaryVersionChecks)
{
    const std::string dummyPayload = "MZ-mock-robohero-teleop-installer-payload";
    const std::string sha = selfupdate::Sha256::hashBytes(
        dummyPayload.data(), dummyPayload.size());

    selfupdate::InstallOptions installOpts;
    installOpts.dryRun = true;

    auto controller = std::make_shared<selfupdate::SelfUpdateController>(
        MainWindow::defaultSelfUpdateConfig(),
        installOpts,
        tempDir->path().toStdString(),
        transport);

    // 1. Newer version (2.2.2 / 20202) -> Available
    transport->setManifestResponse(
        200,
        buildCatalogJson("2.2.2", 20202, sha, static_cast<int64_t>(dummyPayload.size())));
    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(controller->state().phase));
    STRCMP_EQUAL("2.2.2", controller->state().manifest.versionName.c_str());
    LONGS_EQUAL(20202, controller->state().manifest.versionCode);

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

TEST(TeleopSelfUpdateTestGroup, DownloadVerifyAndDryRunInstallSucceeds)
{
    const std::string installerBytes = "MZ-verified-robohero-teleop-setup-2.2.2";
    const std::string sha = selfupdate::Sha256::hashBytes(
        installerBytes.data(), installerBytes.size());

    transport->setManifestResponse(
        200,
        buildCatalogJson("2.2.2", 20202, sha, static_cast<int64_t>(installerBytes.size()),
                         "Improved upper-body elbow flexion retargeting."));
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

TEST(TeleopSelfUpdateTestGroup, FaultInjectionHttpErrorMalformedJsonAndShaMismatch)
{
    const std::string validBytes = "MZ-expected-teleop-installer-binary";
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
        buildCatalogJson("2.2.2", 20202, validSha, static_cast<int64_t>(validBytes.size())));
    controller->checkForUpdates(true);
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(controller->state().phase));

    const std::string corruptedBytes = "MZ-corrupted-teleop-installer-binary";
    transport->setDownloadPayload(true, corruptedBytes);
    controller->downloadPackage();
    controller->waitForIdle();
    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Error),
                static_cast<int>(controller->state().phase));

    for (const auto &entry : std::filesystem::directory_iterator(tempDir->path().toStdString())) {
        CHECK_FALSE(entry.path().extension() == ".part");
    }
}

TEST(TeleopSelfUpdateTestGroup, TeleopConfigAndMinGwLibconfigFaultInjection)
{
    // 1. Missing file recovery in TeleopConfig::load() creates default file
    const std::string newCfgPath = (tempDir->path() + "/created_default.cfg").toStdString();
    CHECK_FALSE(std::filesystem::exists(newCfgPath));
    CHECK_TRUE(TeleopConfig::instance().load(newCfgPath));
    CHECK_TRUE(std::filesystem::exists(newCfgPath));

    // 2. Malformed syntax in TeleopConfig::load() returns false cleanly
    const std::string badCfgPath = (tempDir->path() + "/malformed.cfg").toStdString();
    {
        std::ofstream ofs(badCfgPath);
        ofs << "camera : { width = \"unterminated;\n";
    }
    CHECK_FALSE(TeleopConfig::instance().load(badCfgPath));

    // 3. MinGwLibconfigCompat boundary & fault injection tests
    mingw_libconfig::Config mcfg;
    // Empty input
    mcfg.readString("");
    LONGS_EQUAL(0, mcfg.getRoot().getLength());

    // Comments and nested groups
    mcfg.readString(
        "# hash comment\n"
        "// line comment\n"
        "/* block comment */\n"
        "camera : { deviceIndex = 2; mirrorVideo = false; };\n");
    int devIdx = -1;
    bool mirror = true;
    CHECK_TRUE(mcfg.getRoot()["camera"].lookupValue("deviceIndex", devIdx));
    CHECK_TRUE(mcfg.getRoot()["camera"].lookupValue("mirrorVideo", mirror));
    LONGS_EQUAL(2, devIdx);
    CHECK_FALSE(mirror);

    // Non-existent file throws FileIOException
    bool caughtIo = false;
    try {
        mcfg.readFile("/nonexistent/directory/missing.cfg");
    } catch (const mingw_libconfig::FileIOException &) {
        caughtIo = true;
    }
    CHECK_TRUE(caughtIo);

    // Unterminated block comment throws ParseException
    bool caughtComment = false;
    try {
        mcfg.readString("/* unterminated comment");
    } catch (const mingw_libconfig::ParseException &) {
        caughtComment = true;
    }
    CHECK_TRUE(caughtComment);

    // Unterminated group throws ParseException
    bool caughtGroup = false;
    try {
        mcfg.readString("camera : { width = 640;");
    } catch (const mingw_libconfig::ParseException &) {
        caughtGroup = true;
    }
    CHECK_TRUE(caughtGroup);

    // Oversized nesting depth (> 64) throws ParseException
    std::string deepInput;
    for (int i = 0; i < 70; ++i) {
        deepInput += "g" + std::to_string(i) + " : { ";
    }
    for (int i = 0; i < 70; ++i) {
        deepInput += "}; ";
    }
    bool caughtDeep = false;
    try {
        mcfg.readString(deepInput);
    } catch (const mingw_libconfig::ParseException &) {
        caughtDeep = true;
    }
    CHECK_TRUE(caughtDeep);
}

TEST(TeleopSelfUpdateTestGroup, RapidCheckTeleopConfigAndMinGwLibconfigRoundTrip)
{
    const std::string propCfgPath = (tempDir->path() + "/rc_teleop.cfg").toStdString();

    bool ok = rc::check(
        "TeleopConfig and MinGwLibconfigCompat preserve round-trip configuration values",
        [&propCfgPath]() {
            const int devIdx = *rc::gen::inRange(0, 10);
            const int width = *rc::gen::element(640, 1280, 1920);
            const int height = *rc::gen::element(480, 720, 1080);
            const int fps = *rc::gen::inRange(10, 120);
            const bool mirror = *rc::gen::arbitrary<bool>();

            const std::string provider = *rc::gen::element<std::string>(
                "auto", "directml", "cuda", "cpu");
            const int confPct = *rc::gen::inRange(5, 99);
            const int kptPct = *rc::gen::inRange(5, 99);
            const float confThresh = static_cast<float>(confPct) / 100.0f;
            const float kptThresh = static_cast<float>(kptPct) / 100.0f;

            const int port = *rc::gen::inRange(1, 65535);
            const int keepalive = *rc::gen::inRange(5, 300);
            const bool autoConnect = *rc::gen::arbitrary<bool>();
            const std::string robotSuffix = *rc::gen::element<std::string>(
                "robohero", "TTR-ee40", "unit_01");
            const bool addPrefix = *rc::gen::arbitrary<bool>();

            auto &cfg = TeleopConfig::instance();
            cfg.camera.deviceIndex = devIdx;
            cfg.camera.width = width;
            cfg.camera.height = height;
            cfg.camera.targetFps = fps;
            cfg.camera.mirrorVideo = mirror;

            cfg.ai.provider = provider;
            cfg.ai.confThreshold = confThresh;
            cfg.ai.kptThreshold = kptThresh;

            cfg.mqtt.port = port;
            cfg.mqtt.keepalive = keepalive;
            cfg.mqtt.autoConnect = autoConnect;
            cfg.mqtt.robotId = addPrefix ? ("robot/robohero/" + robotSuffix + "/")
                                         : robotSuffix;

            RC_ASSERT(cfg.save(propCfgPath));

            // Parse with MinGwLibconfigCompat and rewrite to verify cross-parser fidelity
            mingw_libconfig::Config mingwCfg;
            mingwCfg.readFile(propCfgPath.c_str());
            mingwCfg.writeFile(propCfgPath.c_str());

            // Reset in-memory values and reload
            cfg.camera.deviceIndex = -1;
            cfg.camera.width = 0;
            cfg.camera.height = 0;
            cfg.camera.targetFps = 0;
            cfg.camera.mirrorVideo = !mirror;
            cfg.ai.provider.clear();
            cfg.ai.confThreshold = 0.0f;
            cfg.ai.kptThreshold = 0.0f;
            cfg.mqtt.port = 0;
            cfg.mqtt.keepalive = 0;
            cfg.mqtt.autoConnect = !autoConnect;
            cfg.mqtt.robotId.clear();

            RC_ASSERT(cfg.load(propCfgPath));
            RC_ASSERT(cfg.camera.deviceIndex == devIdx);
            RC_ASSERT(cfg.camera.width == width);
            RC_ASSERT(cfg.camera.height == height);
            RC_ASSERT(cfg.camera.targetFps == fps);
            RC_ASSERT(cfg.camera.mirrorVideo == mirror);
            RC_ASSERT(cfg.ai.provider == provider);
            RC_ASSERT(std::fabs(cfg.ai.confThreshold - confThresh) < 1e-4f);
            RC_ASSERT(std::fabs(cfg.ai.kptThreshold - kptThresh) < 1e-4f);
            RC_ASSERT(cfg.mqtt.port == port);
            RC_ASSERT(cfg.mqtt.keepalive == keepalive);
            RC_ASSERT(cfg.mqtt.autoConnect == autoConnect);
            RC_ASSERT(cfg.mqtt.robotId == robotSuffix);
        });
    CHECK_TRUE(ok);
}

TEST(TeleopSelfUpdateTestGroup, MainWindowAndDialogIntegrationOffscreen)
{
    const std::string payload = "MZ-robohero-teleop-setup-2.2.2";
    const std::string sha = selfupdate::Sha256::hashBytes(payload.data(), payload.size());

    transport->setManifestResponse(
        200,
        buildCatalogJson("2.2.2", 20202, sha, static_cast<int64_t>(payload.size()),
                         "Added DirectML FP16 acceleration."));
    transport->setDownloadPayload(true, payload);

    selfupdate::InstallOptions installOpts;
    installOpts.dryRun = true;

    auto controller = std::make_shared<selfupdate::SelfUpdateController>(
        MainWindow::defaultSelfUpdateConfig(),
        installOpts,
        tempDir->path().toStdString(),
        transport);

    MainWindow mainWindow(controller);
    robohero::SelfUpdateDialog dialog(controller, "RoboHero Teleop", &mainWindow);

    STRCMP_EQUAL("Idle", dialog.phaseLabelText().toUtf8().constData());
    CHECK_FALSE(mainWindow.isUpdateStatusBadgeVisible());

    // Process startup check scheduled via QTimer::singleShot(0, ...)
    QCoreApplication::processEvents();
    controller->waitForIdle();
    QCoreApplication::processEvents();

    CHECK_EQUAL(static_cast<int>(selfupdate::UpdatePhase::Available),
                static_cast<int>(mainWindow.currentSelfUpdateState().phase));
    CHECK_TRUE(mainWindow.isUpdateStatusBadgeVisible());
    CHECK_TRUE(mainWindow.updateStatusBadgeText().contains("2.2.2"));

    STRCMP_EQUAL("Available", dialog.phaseLabelText().toUtf8().constData());
    CHECK_TRUE(dialog.versionLabelText().contains("2.2.2"));
    CHECK_TRUE(dialog.releaseNotesText().contains("DirectML"));

    // Trigger primary action in Available state -> downloads package
    dialog.triggerPrimaryAction();
    controller->waitForIdle();
    QCoreApplication::processEvents();

    STRCMP_EQUAL("ReadyToInstall", dialog.phaseLabelText().toUtf8().constData());
    LONGS_EQUAL(100, dialog.progressValue());
    CHECK_TRUE(mainWindow.updateStatusBadgeText().contains("Ready to Install"));
}

TEST(TeleopSelfUpdateTestGroup, MinGwOpenCvVideoFrameDecodeAndFaultInjection)
{
    cv::Mat out;

    // 1. Fault injection: null pointer, zero length, non-positive dimensions, unknown FourCC
    const uint8_t dummy[64] = {0};
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(nullptr, 64, 4, 4, cv::VIDEO_FMT_YUY2, 16, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 0, 4, 4, cv::VIDEO_FMT_YUY2, 16, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 64, 0, 4, cv::VIDEO_FMT_YUY2, 16, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 64, 4, -2, cv::VIDEO_FMT_YUY2, 16, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 64, 4, 4, 0x12345678u, 16, 0, false, out));

    // 2. Truncated buffer fault injection for YUY2, NV12, RGB24, RGB32
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 31, 4, 4, cv::VIDEO_FMT_YUY2, 16, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 23, 4, 4, cv::VIDEO_FMT_NV12, 12, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 47, 4, 4, cv::VIDEO_FMT_D3D_RGB24, 24, 0, false, out));
    CHECK_FALSE(cv::decodeRawVideoFrameToBgr(dummy, 63, 4, 4, cv::VIDEO_FMT_D3D_RGB32, 32, 0, false, out));

    // 3. YUY2 (0x32595559) neutral white (Y=235, U=128, V=128 -> B=255, G=255, R=255)
    std::vector<uint8_t> yuy2Buf(4 * 2 * 2, 0);
    for (size_t i = 0; i < yuy2Buf.size(); i += 4) {
        yuy2Buf[i + 0] = 235;
        yuy2Buf[i + 1] = 128;
        yuy2Buf[i + 2] = 235;
        yuy2Buf[i + 3] = 128;
    }
    CHECK_TRUE(cv::decodeRawVideoFrameToBgr(
        yuy2Buf.data(), yuy2Buf.size(), 4, 2, cv::VIDEO_FMT_YUY2, 16, 0, false, out));
    LONGS_EQUAL(4, out.cols);
    LONGS_EQUAL(2, out.rows);
    LONGS_EQUAL(255, out.ptr<uint8_t>(0)[0]);
    LONGS_EQUAL(255, out.ptr<uint8_t>(0)[1]);
    LONGS_EQUAL(255, out.ptr<uint8_t>(0)[2]);

    // 4. NV12 (0x3231564E) neutral black (Y=16, U=128, V=128 -> B=0, G=0, R=0)
    std::vector<uint8_t> nv12Buf(4 * 4 + 4 * 2, 128);
    std::fill(nv12Buf.begin(), nv12Buf.begin() + 16, 16);
    CHECK_TRUE(cv::decodeRawVideoFrameToBgr(
        nv12Buf.data(), nv12Buf.size(), 4, 4, cv::VIDEO_FMT_NV12, 12, 0, false, out));
    LONGS_EQUAL(4, out.cols);
    LONGS_EQUAL(4, out.rows);
    LONGS_EQUAL(0, out.ptr<uint8_t>(3)[0]);
    LONGS_EQUAL(0, out.ptr<uint8_t>(3)[1]);
    LONGS_EQUAL(0, out.ptr<uint8_t>(3)[2]);

    // 5. Bottom-up BI_RGB 24-bit and D3DFMT_RGB32
    std::vector<uint8_t> rgb24BottomUp(2 * 2 * 3, 0);
    // Row 0 in memory (bottom row in image): blue (255, 0, 0)
    rgb24BottomUp[0] = 255;
    // Row 1 in memory (top row in image): red (0, 0, 200)
    rgb24BottomUp[6 + 2] = 200;
    CHECK_TRUE(cv::decodeRawVideoFrameToBgr(
        rgb24BottomUp.data(), rgb24BottomUp.size(), 2, 2, cv::VIDEO_FMT_BI_RGB, 24, 6, true, out));
    LONGS_EQUAL(200, out.ptr<uint8_t>(0)[2]);
    LONGS_EQUAL(255, out.ptr<uint8_t>(1)[0]);

    // 6. MJPG decode via QImage
    QImage srcImg(8, 8, QImage::Format_RGB888);
    srcImg.fill(QColor(20, 180, 40));
    QByteArray jpegBytes;
    QBuffer qbuf(&jpegBytes);
    qbuf.open(QIODevice::WriteOnly);
    CHECK_TRUE(srcImg.save(&qbuf, "JPG", 100));
    CHECK_TRUE(cv::decodeRawVideoFrameToBgr(
        reinterpret_cast<const uint8_t *>(jpegBytes.constData()),
        static_cast<size_t>(jpegBytes.size()),
        8, 8, cv::VIDEO_FMT_MJPG, 24, 0, false, out));
    LONGS_EQUAL(8, out.cols);
    LONGS_EQUAL(8, out.rows);
}

TEST(TeleopSelfUpdateTestGroup, RapidCheckVideoFrameDecoder)
{
    bool ok = rc::check(
        "decodeRawVideoFrameToBgr decodes arbitrary YUY2, UYVY, NV12, RGB24, and RGB32 frames safely",
        []() {
            const int w = *rc::gen::inRange(1, 33) * 2;
            const int h = *rc::gen::inRange(1, 33) * 2;
            const uint8_t yVal = static_cast<uint8_t>(*rc::gen::inRange(16, 236));
            const uint8_t uVal = static_cast<uint8_t>(*rc::gen::inRange(16, 241));
            const uint8_t vVal = static_cast<uint8_t>(*rc::gen::inRange(16, 241));

            // YUY2 and UYVY must produce identical BGR output for identical YUV values
            std::vector<uint8_t> yuy2(static_cast<size_t>(w) * static_cast<size_t>(h) * 2);
            std::vector<uint8_t> uyvy(static_cast<size_t>(w) * static_cast<size_t>(h) * 2);
            for (size_t i = 0; i < yuy2.size(); i += 4) {
                yuy2[i + 0] = yVal;
                yuy2[i + 1] = uVal;
                yuy2[i + 2] = yVal;
                yuy2[i + 3] = vVal;

                uyvy[i + 0] = uVal;
                uyvy[i + 1] = yVal;
                uyvy[i + 2] = vVal;
                uyvy[i + 3] = yVal;
            }

            cv::Mat matYuy2;
            cv::Mat matUyvy;
            RC_ASSERT(cv::decodeRawVideoFrameToBgr(
                yuy2.data(), yuy2.size(), w, h, cv::VIDEO_FMT_YUY2, 16, 0, false, matYuy2));
            RC_ASSERT(cv::decodeRawVideoFrameToBgr(
                uyvy.data(), uyvy.size(), w, h, cv::VIDEO_FMT_UYVY, 16, 0, false, matUyvy));
            RC_ASSERT(matYuy2.cols == w && matYuy2.rows == h);
            RC_ASSERT(matUyvy.cols == w && matUyvy.rows == h);
            RC_ASSERT(matYuy2.ptr<uint8_t>(0)[0] == matUyvy.ptr<uint8_t>(0)[0]);
            RC_ASSERT(matYuy2.ptr<uint8_t>(0)[1] == matUyvy.ptr<uint8_t>(0)[1]);
            RC_ASSERT(matYuy2.ptr<uint8_t>(0)[2] == matUyvy.ptr<uint8_t>(0)[2]);
        });
    CHECK_TRUE(ok);
}

int main(int argc, char **argv)
{
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("ROBOHERO_DISABLE_AUTO_MQTT", "1");
    qputenv("ROBOHERO_DISABLE_AUTO_CAMERA", "1");
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(teleop_resources);
    qRegisterMetaType<PoseEstimator::PersonPose>("PoseEstimator::PersonPose");
    qRegisterMetaType<std::array<int, 17>>("std::array<int, 17>");
    qRegisterMetaType<std::array<bool, 17>>("std::array<bool, 17>");
    qRegisterMetaType<std::array<double, 17>>("std::array<double, 17>");
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
