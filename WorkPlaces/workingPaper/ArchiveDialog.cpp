#include "ArchiveDialog.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include "ProjectListDelegate.h"
#include "../../storage/ProjectStore.h"

ArchiveDialog::ArchiveDialog(const QString &archivePath, QWidget *parent)
    : QDialog(parent) {
    setObjectName("archiveListDialog");
    setWindowTitle("查看归档");
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumSize(500, 360);
    resize(560, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(12);

    auto *title = new QLabel("归档作品", this);
    title->setObjectName("archiveDialogTitle");
    layout->addWidget(title);

    auto *description = new QLabel(
        "这些作品已从作品库移出，原始文件仍保留在归档目录。", this);
    description->setObjectName("archiveDialogDescription");
    description->setWordWrap(true);
    layout->addWidget(description);


    QList<Project> projects;
    QStringList warnings;
    const QFileInfo archiveInfo(archivePath);
    if (archiveInfo.exists()) {
        if (archiveInfo.isDir() && QDir(archivePath).isReadable()) {
            projects = ProjectStore::loadProjects(archivePath, warnings);
        } else {
            warnings.append("归档目录无法读取：" + archivePath);
        }
    }

    auto *count = new QLabel(QString("共 %1 部归档作品").arg(projects.size()), this);
    count->setObjectName("archiveDialogCount");
    layout->addWidget(count);

    if (projects.isEmpty()) {
        auto *empty = new QLabel(
            warnings.isEmpty() ? "归档中暂无作品" : "未找到可读取的归档作品", this);
        empty->setObjectName("archiveDialogEmpty");
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty, 1);
    } else {
        list_ = new QListWidget(this);
        list_->setObjectName("archiveProjectList");
        list_->setItemDelegate(new ProjectListDelegate(list_));
        list_->setIconSize(QSize(24, 24));
        list_->setSpacing(6);
        list_->setUniformItemSizes(true);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->verticalScrollBar()->setSingleStep(24);
        list_->setSelectionMode(QAbstractItemView::SingleSelection);

        for (const auto &project : projects) {
            const QString summary =
                QString("%1 章 · 最近编辑 %2")
                    .arg(project.chapters.size())
                    .arg(project.updatedAt.toLocalTime().toString("yyyy-MM-dd HH:mm"));
            auto *item = new QListWidgetItem(
                QIcon(":/icons/library.png"), project.name, list_);
            item->setData(Qt::UserRole, project.id);
            item->setData(ProjectListDelegate::SummaryRole, summary);
            item->setToolTip(QString("%1\n%2").arg(project.name, summary));
        }
        layout->addWidget(list_, 1);
    }

    if (!warnings.isEmpty()) {
        auto *warning = new QLabel(
            QString("有 %1 个归档项目未能读取").arg(warnings.size()), this);
        warning->setObjectName("archiveDialogWarning");
        warning->setToolTip(warnings.join('\n'));
        layout->addWidget(warning);
    }

    auto *footer = new QHBoxLayout;
    footer->addStretch();

    restoreButton_ = new QPushButton("恢复到作品库", this);
    restoreButton_->setObjectName("archiveRestoreButton");
    restoreButton_->setEnabled(false);
    footer->addWidget(restoreButton_);

    auto *closeButton = new QPushButton("关闭", this);
    closeButton->setObjectName("archiveDialogCloseButton");
    closeButton->setDefault(true);
    footer->addWidget(closeButton);
    layout->addLayout(footer);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    if (list_) {
        connect(list_, &QListWidget::currentItemChanged, this,
                [this](QListWidgetItem *current, QListWidgetItem *) {
                    restoreButton_->setEnabled(
                        current && !current->data(Qt::UserRole).toString().isEmpty());
                });
    }
    connect(restoreButton_, &QPushButton::clicked, this, [this] {
        if (!selectedProjectId().isEmpty()) accept();
    });

   uiStyle();
}

QString ArchiveDialog::selectedProjectId() const {
    if (!list_ || !list_->currentItem()) return {};
    return list_->currentItem()->data(Qt::UserRole).toString();
}

void ArchiveDialog::uiStyle() {
     setStyleSheet(R"(
        QDialog#archiveListDialog {
            background-color: #1c1e22;
        }
        QDialog#archiveListDialog QLabel {
            background-color: transparent;
            border: none;
            font-family: "Microsoft YaHei UI";
        }
        QLabel#archiveDialogTitle {
            color: #edf0f4;
            font-size: 20px;
            font-weight: bold;
        }
        QLabel#archiveDialogDescription,
        QLabel#archiveDialogCount {
            color: #a1a9b4;
            font-size: 12px;
        }
        QLabel#archiveDialogEmpty {
            color: #929dab;
            font-size: 14px;
        }
        QLabel#archiveDialogWarning {
            color: #e2b05e;
            font-size: 12px;
        }
        QListWidget#archiveProjectList {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 8px;
            padding: 8px;
            outline: 0;
        }
        QListWidget#archiveProjectList::item {
            background-color: #25282d;
            border: 1px solid #373c43;
            border-radius: 7px;
        }
        QListWidget#archiveProjectList::item:hover {
            background-color: #2c3139;
            border-color: #4b5260;
        }
        QListWidget#archiveProjectList::item:selected {
            background-color: #303958;
            border-color: #4b5887;
        }
        QListWidget#archiveProjectList QScrollBar:vertical {
            background-color: transparent;
            width: 8px;
            margin: 0;
            border: none;
        }
        QListWidget#archiveProjectList QScrollBar::handle:vertical {
            background-color: #4b5260;
            min-height: 28px;
            border-radius: 4px;
        }
        QListWidget#archiveProjectList QScrollBar::handle:vertical:hover {
            background-color: #68717d;
        }
        QListWidget#archiveProjectList QScrollBar::handle:vertical:pressed {
            background-color: #7d89b7;
        }
        QListWidget#archiveProjectList QScrollBar::add-line:vertical,
        QListWidget#archiveProjectList QScrollBar::sub-line:vertical {
            height: 0;
            border: none;
            background-color: transparent;
        }
        QListWidget#archiveProjectList QScrollBar::up-arrow:vertical,
        QListWidget#archiveProjectList QScrollBar::down-arrow:vertical {
            width: 0;
            height: 0;
        }
        QListWidget#archiveProjectList QScrollBar::add-page:vertical,
        QListWidget#archiveProjectList QScrollBar::sub-page:vertical {
            background-color: transparent;
        }
        QDialog#archiveListDialog QPushButton#archiveRestoreButton {
            background-color: #303958;
            color: #bac5ff;
            border: 1px solid #4b5887;
            border-radius: 6px;
            padding: 9px 20px;
            min-width: 100px;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
            font-weight: bold;
        }
        QDialog#archiveListDialog QPushButton#archiveRestoreButton:hover {
            background-color: #394567;
            border-color: #6271a4;
        }
        QDialog#archiveListDialog QPushButton#archiveRestoreButton:disabled {
            background-color: #25282d;
            color: #68717d;
            border-color: #373c43;
        }
        QPushButton#archiveDialogCloseButton {
            background-color: #25282d;
            color: #cbd1da;
            border: 1px solid #373c43;
            border-radius: 6px;
            padding: 9px 20px;
            min-width: 80px;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }
        QPushButton#archiveDialogCloseButton:hover {
            background-color: #30343a;
            border-color: #4b5260;
        }
        QPushButton#archiveDialogCloseButton:focus {
            border-color: #9caaff;
        }
    )");
}
