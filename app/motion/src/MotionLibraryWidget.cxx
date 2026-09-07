/*
 * MotionLibraryWidget.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "MotionLibraryWidget.hxx"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileInfo>

MotionLibraryWidget::MotionLibraryWidget(QWidget *parent)
    : QWidget(parent)
    , _searchEdit(nullptr)
    , _listWidget(nullptr)
    , _metaNameLabel(nullptr)
    , _metaDescLabel(nullptr)
    , _metaDurationLabel(nullptr)
    , _metaLoopLabel(nullptr)
    , _loadBtn(nullptr)
    , _playRobotBtn(nullptr)
    , _newBtn(nullptr)
    , _saveBtn(nullptr)
    , _deleteBtn(nullptr)
    , _rescanBtn(nullptr)
{
    setupUi();
    refresh();
}

MotionLibraryWidget::~MotionLibraryWidget()
{
}

void MotionLibraryWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Search / Filter Bar
    auto *searchLayout = new QHBoxLayout();
    _searchEdit = new QLineEdit(this);
    _searchEdit->setPlaceholderText("🔍 Filter motions...");
    _searchEdit->setClearButtonEnabled(true);
    connect(_searchEdit, &QLineEdit::textChanged, this, &MotionLibraryWidget::onSearchTextChanged);
    searchLayout->addWidget(_searchEdit);

    _rescanBtn = new QPushButton("🔄", this);
    _rescanBtn->setToolTip("Rescan motion folders");
    _rescanBtn->setFixedWidth(32);
    connect(_rescanBtn, &QPushButton::clicked, this, &MotionLibraryWidget::onRescanBtnClicked);
    searchLayout->addWidget(_rescanBtn);
    mainLayout->addLayout(searchLayout);

    // Motion List
    _listWidget = new QListWidget(this);
    _listWidget->setStyleSheet(
        "QListWidget { background-color: #16161a; border: 1px solid #333; border-radius: 4px; padding: 4px; }"
        "QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #222; border-radius: 3px; }"
        "QListWidget::item:hover { background-color: #24242e; }"
        "QListWidget::item:selected { background-color: #00ADB5; color: #ffffff; }"
    );
    connect(_listWidget, &QListWidget::itemSelectionChanged, this, &MotionLibraryWidget::onItemSelectionChanged);
    connect(_listWidget, &QListWidget::itemDoubleClicked, this, &MotionLibraryWidget::onItemDoubleClicked);
    mainLayout->addWidget(_listWidget, 1);

    // Selected Motion Metadata Box
    auto *metaBox = new QGroupBox("Motion Details", this);
    auto *metaLayout = new QVBoxLayout(metaBox);
    metaLayout->setContentsMargins(8, 8, 8, 8);
    metaLayout->setSpacing(4);

    _metaNameLabel = new QLabel("<b>(No motion selected)</b>", metaBox);
    _metaNameLabel->setWordWrap(true);
    metaLayout->addWidget(_metaNameLabel);

    _metaDescLabel = new QLabel("", metaBox);
    _metaDescLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    _metaDescLabel->setWordWrap(true);
    metaLayout->addWidget(_metaDescLabel);

    auto *statsLayout = new QHBoxLayout();
    _metaDurationLabel = new QLabel("Duration: --", metaBox);
    _metaDurationLabel->setStyleSheet("color: #00ADB5; font-size: 11px; font-weight: bold;");
    _metaLoopLabel = new QLabel("Loop: --", metaBox);
    _metaLoopLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    statsLayout->addWidget(_metaDurationLabel);
    statsLayout->addWidget(_metaLoopLabel);
    statsLayout->addStretch();
    metaLayout->addLayout(statsLayout);

    mainLayout->addWidget(metaBox);

    // Action Buttons
    auto *actionLayout1 = new QHBoxLayout();
    _loadBtn = new QPushButton("📂 Load", this);
    _loadBtn->setToolTip("Load motion into the choreographer timeline");
    _loadBtn->setEnabled(false);
    connect(_loadBtn, &QPushButton::clicked, this, &MotionLibraryWidget::onLoadBtnClicked);
    actionLayout1->addWidget(_loadBtn);

    _playRobotBtn = new QPushButton("▶ Play on Robot", this);
    _playRobotBtn->setToolTip("Immediately stream and execute motion on physical robot via MQTT");
    _playRobotBtn->setStyleSheet("background-color: #008855; color: #fff; font-weight: bold; border-radius: 4px; padding: 5px;");
    _playRobotBtn->setEnabled(false);
    connect(_playRobotBtn, &QPushButton::clicked, this, &MotionLibraryWidget::onPlayBtnClicked);
    actionLayout1->addWidget(_playRobotBtn);
    mainLayout->addLayout(actionLayout1);

    auto *actionLayout2 = new QHBoxLayout();
    _newBtn = new QPushButton("+ New", this);
    _newBtn->setToolTip("Create a new blank motion sequence");
    connect(_newBtn, &QPushButton::clicked, this, &MotionLibraryWidget::onNewBtnClicked);
    actionLayout2->addWidget(_newBtn);

    _saveBtn = new QPushButton("💾 Save", this);
    _saveBtn->setToolTip("Save current motion to library");
    connect(_saveBtn, &QPushButton::clicked, this, &MotionLibraryWidget::onSaveBtnClicked);
    actionLayout2->addWidget(_saveBtn);

    _deleteBtn = new QPushButton("🗑 Delete", this);
    _deleteBtn->setToolTip("Delete selected user motion");
    _deleteBtn->setEnabled(false);
    connect(_deleteBtn, &QPushButton::clicked, this, &MotionLibraryWidget::onDeleteBtnClicked);
    actionLayout2->addWidget(_deleteBtn);
    mainLayout->addLayout(actionLayout2);
}

void MotionLibraryWidget::refresh()
{
    _listWidget->clear();
    MotionLibrary::instance().scan();

    QString filter = _searchEdit->text().trimmed().toLower();
    const auto &items = MotionLibrary::instance().items();

    for (const auto &item : items) {
        if (!filter.isEmpty()) {
            if (!item.name.toLower().contains(filter) &&
                !item.description.toLower().contains(filter)) {
                continue;
            }
        }

        QString badge = item.isPreset ? "📦 [Preset]" : "👤 [User]";
        double sec = static_cast<double>(item.durationMs) / 1000.0;
        QString text = QString("%1 %2\n   ⏱ %3s  |  %4 kf  %5")
                       .arg(badge)
                       .arg(item.name)
                       .arg(sec, 0, 'f', 2)
                       .arg(item.stepCount)
                       .arg(item.loop ? "|  🔁 Loop" : "");

        auto *listItem = new QListWidgetItem(text, _listWidget);
        listItem->setData(Qt::UserRole, item.filePath);
    }

    if (!items.empty() && _selectedFilePath.isEmpty()) {
        selectFile(items[0].filePath);
    }
}

void MotionLibraryWidget::selectFile(const QString &filePath)
{
    _selectedFilePath = filePath;
    for (int i = 0; i < _listWidget->count(); ++i) {
        auto *it = _listWidget->item(i);
        if (it->data(Qt::UserRole).toString() == filePath) {
            _listWidget->setCurrentItem(it);
            break;
        }
    }
    onItemSelectionChanged();
}

void MotionLibraryWidget::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    refresh();
}

void MotionLibraryWidget::onItemSelectionChanged()
{
    auto *cur = _listWidget->currentItem();
    if (!cur) {
        _selectedFilePath.clear();
        _metaNameLabel->setText("<b>(No motion selected)</b>");
        _metaDescLabel->setText("");
        _metaDurationLabel->setText("Duration: --");
        _metaLoopLabel->setText("Loop: --");
        _loadBtn->setEnabled(false);
        _playRobotBtn->setEnabled(false);
        _deleteBtn->setEnabled(false);
        return;
    }

    _selectedFilePath = cur->data(Qt::UserRole).toString();
    const auto *item = MotionLibrary::instance().findItemByPath(_selectedFilePath);
    if (item) {
        _metaNameLabel->setText(QString("<b>%1</b>").arg(item->name));
        _metaDescLabel->setText(item->description.isEmpty() ? "(No description)" : item->description);
        double sec = static_cast<double>(item->durationMs) / 1000.0;
        _metaDurationLabel->setText(QString("Duration: %1s (%2 kf)").arg(sec, 0, 'f', 2).arg(item->stepCount));
        _metaLoopLabel->setText(item->loop ? "Loop: 🔁 Yes" : "Loop: ➡ Once");
        _loadBtn->setEnabled(true);
        _playRobotBtn->setEnabled(true);
        _deleteBtn->setEnabled(!item->isPreset);
    }
}

void MotionLibraryWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        _selectedFilePath = item->data(Qt::UserRole).toString();
        emit motionLoadRequested(_selectedFilePath);
    }
}

void MotionLibraryWidget::onLoadBtnClicked()
{
    if (!_selectedFilePath.isEmpty()) {
        emit motionLoadRequested(_selectedFilePath);
    }
}

void MotionLibraryWidget::onPlayBtnClicked()
{
    if (!_selectedFilePath.isEmpty()) {
        emit motionPlayOnRobotRequested(_selectedFilePath);
    }
}

void MotionLibraryWidget::onNewBtnClicked()
{
    emit newMotionRequested();
}

void MotionLibraryWidget::onSaveBtnClicked()
{
    emit saveMotionRequested();
}

void MotionLibraryWidget::onDeleteBtnClicked()
{
    if (_selectedFilePath.isEmpty()) return;

    const auto *item = MotionLibrary::instance().findItemByPath(_selectedFilePath);
    if (!item || item->isPreset) {
        QMessageBox::information(this, "Cannot Delete", "Bundled presets cannot be deleted.");
        return;
    }

    auto res = QMessageBox::question(this, "Confirm Delete",
                                     QString("Are you sure you want to delete motion:\n'%1'?").arg(item->name),
                                     QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        MotionLibrary::instance().deleteMotion(_selectedFilePath);
        refresh();
    }
}

void MotionLibraryWidget::onRescanBtnClicked()
{
    refresh();
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
