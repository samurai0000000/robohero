/*
 * MotionLibrary.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_LIBRARY_HXX
#define ROBOHERO_MOTION_LIBRARY_HXX

#include <QString>
#include <QStringList>
#include <vector>
#include "MotionSequence.hxx"

class MotionLibrary
{
public:
    struct MotionItem
    {
        QString filePath;
        QString name;
        QString description;
        int durationMs;
        int stepCount;
        bool loop;
        bool isPreset;
    };

    static MotionLibrary &instance();

    void init();
    void scan();

    const std::vector<MotionItem> &items() const { return _items; }
    int count() const { return static_cast<int>(_items.size()); }
    const MotionItem *getItem(int index) const;
    const MotionItem *findItemByPath(const QString &filePath) const;

    QString defaultUserDir() const;
    QString customWorkspaceDir() const;
    void setCustomWorkspaceDir(const QString &dir);

    QString lastLoadedFile() const;
    void setLastLoadedFile(const QString &filePath);

    bool saveMotion(const MotionSequence &seq, const QString &filePath);
    bool deleteMotion(const QString &filePath);

private:
    MotionLibrary();
    ~MotionLibrary();

    static MotionLibrary _self;
    std::vector<MotionItem> _items;
    QString _customWorkspaceDir;
    QString _lastLoadedFile;

    void scanDirectory(const QString &dirPath, bool isPreset);
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
