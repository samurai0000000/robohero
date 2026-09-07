/*
 * MotionLibrary.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MotionLibrary.hxx"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QCoreApplication>
#include <iostream>

MotionLibrary MotionLibrary::_self;

MotionLibrary &MotionLibrary::instance()
{
    return _self;
}

MotionLibrary::MotionLibrary()
{
}

MotionLibrary::~MotionLibrary()
{
}

void MotionLibrary::init()
{
    QSettings settings("SelfSo", "RoboHeroMotion");
    _customWorkspaceDir = settings.value("workspaceDir", "").toString();
    _lastLoadedFile = settings.value("lastLoadedFile", "").toString();

    QDir userDir(defaultUserDir());
    if (!userDir.exists()) {
        userDir.mkpath(".");
    }

    scan();
}

QString MotionLibrary::defaultUserDir() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.robohero";
    }
    return base + "/motions";
}

QString MotionLibrary::customWorkspaceDir() const
{
    return _customWorkspaceDir;
}

void MotionLibrary::setCustomWorkspaceDir(const QString &dir)
{
    _customWorkspaceDir = dir;
    QSettings settings("SelfSo", "RoboHeroMotion");
    settings.setValue("workspaceDir", dir);
    scan();
}

QString MotionLibrary::lastLoadedFile() const
{
    return _lastLoadedFile;
}

void MotionLibrary::setLastLoadedFile(const QString &filePath)
{
    _lastLoadedFile = filePath;
    QSettings settings("SelfSo", "RoboHeroMotion");
    settings.setValue("lastLoadedFile", filePath);
}

void MotionLibrary::scanDirectory(const QString &dirPath, bool isPreset)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        return;
    }

    QStringList filters;
    filters << "*.rhm.json" << "*.json";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo &info : fileList) {
        QString absPath = info.absoluteFilePath();
        bool alreadyExists = false;
        for (const auto &existing : _items) {
            if (existing.filePath == absPath) {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) {
            continue;
        }

        MotionSequence seq;
        if (seq.loadFromFile(absPath.toStdString())) {
            MotionItem item;
            item.filePath = absPath;
            item.name = QString::fromStdString(seq.name().empty() ? info.baseName().toStdString() : seq.name());
            item.description = QString::fromStdString(seq.description());
            item.durationMs = seq.durationMs();
            item.stepCount = seq.keyframeCount();
            item.loop = seq.isLoop();
            item.isPreset = isPreset;
            _items.push_back(item);
        }
    }
}

void MotionLibrary::scan()
{
    _items.clear();

    // 1. Scan embedded Qt resource motions (always available in standalone binaries)
    scanDirectory(":/motions", true);

    // 2. Scan app bundle motions directory (relative to executable and source root)
    QString appDir = QCoreApplication::applicationDirPath();
    scanDirectory(appDir + "/motions", true);
    scanDirectory(appDir + "/../motions", true);
    scanDirectory(appDir + "/../../motions", true);

    // 3. Scan standard user motions directory
    scanDirectory(defaultUserDir(), false);

    // 4. Scan custom workspace directory if specified
    if (!_customWorkspaceDir.isEmpty()) {
        scanDirectory(_customWorkspaceDir, false);
    }
}

const MotionLibrary::MotionItem *MotionLibrary::getItem(int index) const
{
    if (index >= 0 && index < static_cast<int>(_items.size())) {
        return &_items[index];
    }
    return nullptr;
}

const MotionLibrary::MotionItem *MotionLibrary::findItemByPath(const QString &filePath) const
{
    for (const auto &item : _items) {
        if (item.filePath == filePath) {
            return &item;
        }
    }
    return nullptr;
}

bool MotionLibrary::saveMotion(const MotionSequence &seq, const QString &filePath)
{
    if (seq.saveToFile(filePath.toStdString())) {
        scan();
        setLastLoadedFile(filePath);
        return true;
    }
    return false;
}

bool MotionLibrary::deleteMotion(const QString &filePath)
{
    QFile file(filePath);
    if (file.exists() && file.remove()) {
        scan();
        return true;
    }
    return false;
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
