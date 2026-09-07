/*
 * MotionLibraryWidget.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_LIBRARY_WIDGET_HXX
#define ROBOHERO_MOTION_LIBRARY_WIDGET_HXX

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "MotionLibrary.hxx"

class MotionLibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotionLibraryWidget(QWidget *parent = nullptr);
    ~MotionLibraryWidget() override;

    void refresh();
    void selectFile(const QString &filePath);

signals:
    void motionLoadRequested(const QString &filePath);
    void motionPlayOnRobotRequested(const QString &filePath);
    void newMotionRequested();
    void saveMotionRequested();

private slots:
    void onSearchTextChanged(const QString &text);
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onLoadBtnClicked();
    void onPlayBtnClicked();
    void onNewBtnClicked();
    void onSaveBtnClicked();
    void onDeleteBtnClicked();
    void onRescanBtnClicked();

private:
    void setupUi();

    QLineEdit *_searchEdit;
    QListWidget *_listWidget;
    QLabel *_metaNameLabel;
    QLabel *_metaDescLabel;
    QLabel *_metaDurationLabel;
    QLabel *_metaLoopLabel;
    QPushButton *_loadBtn;
    QPushButton *_playRobotBtn;
    QPushButton *_newBtn;
    QPushButton *_saveBtn;
    QPushButton *_deleteBtn;
    QPushButton *_rescanBtn;

    QString _selectedFilePath;
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
