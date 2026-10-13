//
// Created by shijie on 2026/10/3.
//

#include "Paper.h"
#include <QDir>
#include <QFileInfo>
#include "ProjectListDelegate.h"
#include <QSplitter>
#include <QLabel>
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QIcon>
#include <QFrame>
#include <QMenu>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QUuid>
#include <QTimer>
#include "../../models/Project.h"
#include "../../storage/ProjectStore.h"
#include "../../storage/StoragePaths.h"
#include <QFileDialog>
#include <QSaveFile>
#include "../../storage/Chapter/ChapterStore.h"
#include "../../storage/DocumentAbout/DocumentImporter.h"
#include "ArchiveDialog.h"
#include "ImportPreviewDialog.h"
Paper::Paper( const QString &projectsPath,QWidget *parent) :QWidget(parent),projectsPath_(projectsPath){
   auto *layout = new QVBoxLayout(this);
   layout->setContentsMargins(28, 28, 28, 20);
   layout->setSpacing(24);


   auto *headLayout = new QHBoxLayout();
   layout->addLayout(headLayout);
   // 标题
   auto *titleQwidget = new QWidget(this);
   auto *titleLayout = new QVBoxLayout(titleQwidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);
   auto *firstLabel = new QLabel("作品库",titleQwidget);
   auto *secondLabel = new QLabel("管理你的作品,从这里开始或写作",titleQwidget);
   titleLayout->addWidget(firstLabel);
   titleLayout->addWidget(secondLabel);
   // 归档、导入与新建
   auto *archiveButton = new QPushButton("查看归档", this);
   btnLoad = new QPushButton("↓ 导入",this);
   btnCreate = new QPushButton("₊ 新建",this);
  setObjectName("libraryPage");
  setAttribute(Qt::WA_StyledBackground, true);

  firstLabel->setObjectName("pageTitle");
  secondLabel->setObjectName("pageDescription");

    archiveButton->setObjectName("archiveButton");
    btnLoad->setObjectName("importButton");
    btnCreate->setObjectName("createButton");
    btnCreate->setText("＋ 新建作品");
    headLayout->addWidget(titleQwidget);
    headLayout->addStretch();
    headLayout->addWidget(archiveButton);
    headLayout->addWidget(btnLoad);
    headLayout->addWidget(btnCreate);

    connect(archiveButton, &QPushButton::clicked, this, [this] {
        const QString archivePath = StoragePaths::archiveDirectory(projectsPath_);
        ArchiveDialog dialog(archivePath, this);
        if (dialog.exec() == QDialog::Accepted) {
            const QString id = dialog.selectedProjectId();
            if (!id.isEmpty()) emit restoreProjectRequested(id);
        }
    });

    // 中间的body

    bodyStack = new QStackedWidget(this);
    layout->addWidget(bodyStack, 1);

    // 空状态页面
    emptyPage = new QFrame(bodyStack);
    emptyPage->setObjectName("emptyPage");
    auto *emptyIcon = new QLabel(emptyPage);
    emptyIcon->setObjectName("emptyIcon");
    emptyIcon->setFixedSize(78, 78);
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyIcon->setPixmap(
        QIcon(":/icons/library.png").pixmap(38, 38)
    );
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(24, 24, 24, 24);
    emptyLayout->setSpacing(12);
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);
    emptyLayout->addSpacing(10);

    // 大小标题
    auto * label1 = new QLabel("开始你的第一部作品",emptyPage);
    auto *label2 = new QLabel(emptyPage);
    label1->setObjectName("emptyTitle");

    label2->setObjectName("emptyDescription");
    label2->setText(
        "为故事取一个名字，开始写下第一章。\n"
        "也可以导入已有的 TXT、Markdown 文稿或作品文件。"
    );
    label2->setWordWrap(true);
    label1->setAlignment(Qt::AlignCenter);
    label2->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(label1);
    emptyLayout->addWidget(label2);
    auto *actionLayout = new QHBoxLayout;
    actionLayout->setSpacing(10);
    // 空页按钮  功能复用的顶部按钮
    auto *emptyCreateButton =
        new QPushButton("＋ 新建作品", emptyPage);
    auto *emptyImportButton =
        new QPushButton("导入已有文稿", emptyPage);

    emptyImportButton->setObjectName("emptyImportButton");
    emptyCreateButton->setObjectName("emptyCreateButton");


    actionLayout->addStretch();
    actionLayout->addWidget(emptyCreateButton);
    actionLayout->addWidget(emptyImportButton);
    actionLayout->addStretch();

    emptyLayout->addSpacing(12);
    emptyLayout->addLayout(actionLayout);

    auto *emptyNote = new QLabel(
        "作品名和设定都可以在后续修改", emptyPage
    );
    emptyNote->setObjectName("emptyNote");
    emptyNote->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyNote);
    //  功能复用的顶部按钮
    connect(emptyCreateButton, &QPushButton::clicked,
        btnCreate, &QPushButton::click);

    connect(emptyImportButton, &QPushButton::clicked,
            btnLoad, &QPushButton::click);
    connect(btnLoad,&QPushButton::clicked,this,&Paper::importProjectTxt);
    emptyLayout->addStretch();



    // 有内容页

    filledPage = new QSplitter(Qt::Horizontal,bodyStack);
    filledPage->setSizes({340, 600});

    listPanel = new QFrame(filledPage);
    detailPanel = new QFrame(filledPage);
    listPanel->setObjectName("listPanel");
    listPanel->setMinimumWidth(280);
    listPanel->setMaximumWidth(340);
    detailPanel->setObjectName("detailPanel");

    filledPage->addWidget(listPanel);
    filledPage->addWidget(detailPanel);

    filledPage->setSizes({292, 600});
    filledPage->setStretchFactor(0, 0);
    filledPage->setStretchFactor(1, 1);
    filledPage->setChildrenCollapsible(false);

    // 左侧
    //     标题
    auto * leftLayout = new QVBoxLayout(listPanel);
    auto * labelAll = new QLabel("全部作品",listPanel);
    labelAll->setObjectName("listTitle");
    leftLayout->addWidget(labelAll);
    leftLayout->setContentsMargins(18, 18, 18, 18);

    //     列表
    projectList = new QListWidget(listPanel);
    projectList->setObjectName("projectList");
    projectList->setSpacing(6);
    projectList->setMouseTracking(true);
    projectList->setIconSize(QSize(24, 24));
    projectList->setItemDelegate(new ProjectListDelegate(projectList));
    projectList->setUniformItemSizes(true);
    projectList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    leftLayout->addWidget(projectList, 1);




    projectCountLabel = new QLabel("共 0 部作品", listPanel);
    leftLayout->addWidget(projectCountLabel);

    // 右侧
    //     标题
    auto * rightLayout = new QVBoxLayout(detailPanel);
    auto * labelDet = new QLabel("作品详情",detailPanel);

    rightLayout->setContentsMargins(24, 24, 24, 24);
    auto * bookHeaderLayout = new QHBoxLayout();
    bookHeaderLayout->setSpacing(20);
    //     封面
    auto *bookCover = new QLabel(detailPanel);
    bookCover->setObjectName("bookCover");
    bookCover->setFixedSize(90, 119);
    bookCover->setAlignment(Qt::AlignCenter);
    bookCover->setPixmap(
        QIcon(":/icons/library.png").pixmap(38, 38)
    );
    bookHeaderLayout->addWidget(bookCover);

    auto * bookInfoLayout = new QVBoxLayout();
    bookHeaderLayout->addLayout(bookInfoLayout,1);
    bookNameLabel = new QLabel("雾港来信", detailPanel);
    bookNameLabel->setObjectName("bookName");
    bookNameLabel->setWordWrap(true);
    genreLabel = new QLabel(detailPanel);
    genreLabel->setObjectName("bookGenre");
    genreLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    genreLabel->setTextFormat(Qt::PlainText);
    tagsLabel = new QLabel(detailPanel);
    tagsLabel->setObjectName("bookTags");
    tagsLabel->setTextFormat(Qt::PlainText);
    tagsLabel->setWordWrap(true);
    updatedLabel = new QLabel(detailPanel);
    updatedLabel->setObjectName("bookUpdated");

    bookInfoLayout->addWidget(bookNameLabel);
    bookInfoLayout->addWidget(genreLabel);
    bookInfoLayout->addWidget(tagsLabel);
    bookInfoLayout->addWidget(updatedLabel);

    //    统计行
    auto *statsLayout = new QHBoxLayout;
    statsLayout->setSpacing(12);
    //         章节数
    auto *chapterFrame = new QFrame(detailPanel);
    chapterFrame->setObjectName("chapterStat");

    auto *chapterLayout = new QVBoxLayout(chapterFrame);
    chapterLayout->setContentsMargins(15, 12, 15, 12);
    chapterLayout->setSpacing(4);

    chapterCountLabel = new QLabel("12", chapterFrame);
    chapterCountLabel->setObjectName("chapterCount");

    auto *chapterCaption = new QLabel("章节", chapterFrame);
    chapterCaption->setObjectName("chapterCaption");

    chapterLayout->addWidget(chapterCountLabel);
    chapterLayout->addWidget(chapterCaption);

    statsLayout->addWidget(chapterFrame, 1);

    //         字数
    auto * wordFrame = new QFrame(detailPanel);
    wordFrame->setObjectName("wordStat");
    auto *wordLayout = new QVBoxLayout(wordFrame);
    wordLayout->setContentsMargins(15, 12, 15, 12);
    wordLayout->setSpacing(4);
    wordCountLabel = new QLabel("48,120", wordFrame);
    wordCountLabel->setObjectName("wordCount");
    auto *wordCaption = new QLabel("累计字数", wordFrame);
    wordCaption->setObjectName("wordCaption");
    wordLayout->addWidget(wordCountLabel);
    wordLayout->addWidget(wordCaption);
    statsLayout->addWidget(wordFrame, 1);

    //    简介
    auto *synopsisTitle = new QLabel("作品简介", detailPanel);
    synopsisTitle->setObjectName("synopsisTitle");

    synopsisLabel = new QLabel(detailPanel);
    synopsisLabel->setObjectName("synopsisText");
    synopsisLabel->setText(
        "一封来自旧住址的信，让林澄重新追查哥哥三年前的失踪。"
        "旧车站、失物招领簿和几名证人的讲述，"
        "逐渐指向同一个雨夜。"
    );
    synopsisLabel->setWordWrap(true);
    synopsisLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    synopsisLabel->setMinimumHeight(75);

    auto *resumeFrame = new QFrame(detailPanel);
    auto *resumeLayout = new QVBoxLayout(resumeFrame);
    resumeLayout->setContentsMargins(14, 12, 14, 12);

    auto *resumeCaption = new QLabel("上次写到",resumeFrame);
    lastChapterLabel = new QLabel("第 12 章 · 雨夜来信",resumeFrame);
    resumeLayout->addWidget(resumeCaption);
    resumeLayout->addWidget(lastChapterLabel);

    resumeFrame->setObjectName("resumeFrame");
    resumeCaption->setObjectName("resumeCaption");
    lastChapterLabel->setObjectName("lastChapter");

    //    下方功能按钮
    auto *detailActions = new QHBoxLayout;
    detailActions->setSpacing(10);

    auto *continueButton = new QPushButton("继续写作 →", detailPanel);
    auto *planningButton = new QPushButton("大纲与设定", detailPanel);
    auto *moreButton = new QPushButton("更多 ···", detailPanel);

    continueButton->setObjectName("continueButton");
    planningButton->setObjectName("planningButton");
    moreButton->setObjectName("moreButton");

    detailActions->addWidget(continueButton);
    detailActions->addWidget(planningButton);
    detailActions->addStretch();
    detailActions->addWidget(moreButton);

    auto *moreMenu = new QMenu(moreButton);
    auto *editAction = moreMenu->addAction("编辑作品信息");
    connect(editAction, &QAction::triggered, this, &Paper::editSelectedProject);
    auto * exportAction = moreMenu->addAction("导出作品");
    connect(exportAction,&QAction::triggered,this,&Paper::exportSelectedProject);
    moreMenu->addSeparator();
    auto * removeAction = moreMenu->addAction("从作品库移除");
    connect(removeAction,&QAction::triggered,this,&Paper::removeSelectedProject);
    moreButton->setMenu(moreMenu);


    rightLayout->addWidget(labelDet);
    rightLayout->addLayout(bookHeaderLayout);
    rightLayout->addLayout(statsLayout);

    rightLayout->addWidget(synopsisTitle);
    rightLayout->addWidget(synopsisLabel);

    rightLayout->addStretch();
    rightLayout->addWidget(resumeFrame);
    rightLayout->addLayout(detailActions);
    bodyStack->addWidget(emptyPage);
    bodyStack->addWidget(filledPage);

    headerStyle();
    emptyStyle();
    fileStyle();

    connect(projectList, &QListWidget::currentItemChanged,
        this, [this](QListWidgetItem *current,
                     QListWidgetItem *) {
            if (!current) {
                return;
            }

            const QString id =
                current->data(Qt::UserRole).toString();

            showProjectDetails(id);
        });
    connect(btnCreate, &QPushButton::clicked,this, &Paper::createProject);

    connect(continueButton, &QPushButton::clicked,this, [this]() {
        auto *current = projectList->currentItem();
        if (!current) {
            return;
        }

        const QString id = current->data(Qt::UserRole).toString();

        emit openProjectRequested(id);
    });

    connect(planningButton, &QPushButton::clicked, this, [this]() {
        auto *current = projectList->currentItem();
        if (!current) return;
        const QString id = current->data(Qt::UserRole).toString();
        if (!id.isEmpty()) emit openPlanningRequested(id);
    });

    connect(projectList, &QListWidget::itemDoubleClicked,this, [this](QListWidgetItem *item) {
        if (!item)return;

        const QString id =item->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;

        emit openProjectRequested(id);
    });

    loadBooksList();
    renderProjectList();

    if (projectList->count() > 0) {
        projectList->setCurrentRow(0);
    }

}

void Paper::setDefaultAuthorName(const QString &name) {
    authorName = name;
}

void Paper::exportSelectedProject() {
    auto *current = projectList->currentItem();
    if (!current) return;
    const QString id = current->data(Qt::UserRole).toString();

    const QString path = StoragePaths::projectFilePath(
        StoragePaths::projectDirectory(projectsPath_, id));

    QString error;
    Project project;
    if (!ProjectStore::loadProject(path,project,error)) {
        QMessageBox::warning(this,"加载失败",error);
        return;
    }
    if (project.chapters.isEmpty()) {
        QMessageBox::critical(this,"无章节","请先创建章节");
        return;
    }

    QString exportPath = QFileDialog::getSaveFileName(this,"导出作品",QDir::home().filePath(QString("%1.txt").arg(project.name)),"文本文件 (*.txt)");
    if (exportPath.isEmpty()) return;
    QString text = "WriterStudio-TXT/1\n";
    text += QString("书名：%1\n").arg(project.name);
    text += QString("作者：%1\n").arg(project.authorName.trimmed());
    text += QString("章节数：%1\n").arg(project.chapters.size());
    text += QString("简介：%1\n\n").arg(project.synopsis.trimmed());
    for (int row = 0; row < project.chapters.size(); ++row) {
        const auto &chapter = project.chapters.at(row);
        QString content, error;

        if (!ChapterStore::loadChapter(QDir(projectsPath_).filePath(id), chapter.id, content, error)) {
            QMessageBox::warning(this, "导出失败", error);
            return;
        }

        text += QString("第%1章 · %2\n\n").arg(row + 1).arg(chapter.title);
        text += content + "\n\n";
    }

    auto copy = text.toUtf8();
    QSaveFile file(exportPath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this,"打开文件失败",file.errorString());
        return;
    }

    if (file.write(copy)!=copy.size()) {
        QMessageBox::critical(this,"写入失败",file.errorString());
        return;
    }
    if (!file.commit()) {
        QMessageBox::critical(this,"导出失败",file.errorString());
        return;
    }
    QMessageBox::information(this,"导出成功","导出成功完成！");
}

void Paper::refreshProjects() {
    QString selectedId;
    if (auto *current = projectList->currentItem()) {
        selectedId = current->data(Qt::UserRole).toString();
    }

    loadBooksList();
    renderProjectList();

    for (int row = 0; row < projectList->count(); row++) {
        auto *item = projectList->item(row);
        if (item->data(Qt::UserRole).toString() == selectedId) {
            projectList->setCurrentItem(item);
            return;
        }
    }
    if (projectList->count() > 0) {
        projectList->setCurrentRow(0);
    }


}

void Paper::selectProject(const QString &id) {
    for (int row = 0; row < projectList->count(); ++row) {
        auto *item = projectList->item(row);
        if (item->data(Qt::UserRole).toString() == id) {
            projectList->setCurrentItem(item);
            return;
        }
    }
}

void Paper::removeSelectedProject() {
    auto *current = projectList->currentItem();
    if (!current) return;

    const QString id = current->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    QDialog dialog(this);
    dialog.setObjectName("archiveConfirmDialog");
    dialog.setWindowTitle("移至归档");
    dialog.setMinimumWidth(440);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(
        QString("将《%1》移至归档？").arg(current->text()), &dialog);
    title->setObjectName("archiveConfirmTitle");
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    layout->addWidget(title);

    auto *description = new QLabel(
        "作品会从作品库移除，文件将保留在归档目录，之后可以恢复。\n"
        "如果作品仍在写作页打开，会先询问如何处理未保存的内容。",
        &dialog);
    description->setObjectName("archiveConfirmDescription");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch();

    auto *cancelButton = new QPushButton("取消", &dialog);
    cancelButton->setObjectName("archiveCancelButton");
    cancelButton->setDefault(true);
    buttons->addWidget(cancelButton);

    auto *archiveButton = new QPushButton("移至归档", &dialog);
    archiveButton->setObjectName("archiveConfirmButton");
    archiveButton->setAutoDefault(false);
    buttons->addWidget(archiveButton);
    layout->addLayout(buttons);

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(archiveButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.setStyleSheet(R"(
        QDialog#archiveConfirmDialog {
            background-color: #1c1e22;
        }
        QDialog#archiveConfirmDialog QLabel {
            background-color: transparent;
            border: none;
            font-family: "Microsoft YaHei UI";
        }
        QLabel#archiveConfirmTitle {
            color: #edf0f4;
            font-size: 18px;
            font-weight: bold;
        }
        QLabel#archiveConfirmDescription {
            color: #a1a9b4;
            font-size: 12px;
            padding-bottom: 8px;
        }
        QDialog#archiveConfirmDialog QPushButton {
            background-color: #25282d;
            color: #cbd1da;
            border: 1px solid #373c43;
            border-radius: 6px;
            padding: 9px 18px;
            min-width: 84px;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }
        QDialog#archiveConfirmDialog QPushButton:hover {
            background-color: #30343a;
            border-color: #4b5260;
        }
        QDialog#archiveConfirmDialog QPushButton#archiveConfirmButton {
            background-color: #554233;
            color: #f1c48d;
            border-color: #866344;
            font-weight: bold;
        }
        QDialog#archiveConfirmDialog QPushButton#archiveConfirmButton:hover {
            background-color: #684e39;
            border-color: #a77a50;
        }
        QDialog#archiveConfirmDialog QPushButton:focus {
            border-color: #9caaff;
        }
    )");

    if (dialog.exec() == QDialog::Accepted)
        emit archiveProjectRequested(id);
}

void Paper::importProjectTxt() {
    const QString targetPath = QFileDialog::getOpenFileName(
        this, "选取 TXT 小说", QDir::homePath(), "文本文件 (*.txt)");
    if (targetPath.isEmpty()) return;

    QString error;
    QString content;
    if (!DocumentImporter::readTxt(targetPath, content, error)) {
        QMessageBox::warning(this, "导入失败", error);
        return;
    }

    const SplitNovelText split = DocumentImporter::splitNovelTxt(content);
    ImportedBookHeader header;
    if (!DocumentImporter::parseBookHeader(split.headerText, header, error)) {
        QMessageBox::warning(this, "无法识别 TXT 文件头", error);
        return;
    }

    ImportPreviewDialog preview(header, split, this);
    if (preview.exec() != QDialog::Accepted) return;

    QList<ImportedChapterText> drafts;
    if (preview.mode() == ImportPreviewDialog::Mode::Chapters) {
        drafts = split.chapters;
    } else if (preview.mode() == ImportPreviewDialog::Mode::SingleChapter) {
        drafts.append({QStringLiteral("未命名"), content});
    } else {
        return;
    }
    if (drafts.isEmpty()) return;

    Project project;
    project.genre = QStringLiteral("待设定");
    project.name = header.name;
    project.authorName = header.authorName;
    project.synopsis = header.synopsis;
    project.createdAt = QDateTime::currentDateTimeUtc();
    project.updatedAt = project.createdAt;

    QString projectDir;
    do {
        project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        projectDir = StoragePaths::projectDirectory(projectsPath_, project.id);
    } while (QFileInfo::exists(projectDir));

    // projectDir 在本次导入前不存在，只清理这次新建的目录。
    const auto rollback = [&]() {
        if (QFileInfo::exists(projectDir) && !QDir(projectDir).removeRecursively()) {
            error += QString("\n无法清理未完成的导入目录：%1").arg(projectDir);
        }
    };

    QProgressDialog progress("正在导入章节…", "取消", 0, drafts.size() + 1, this);
    progress.setWindowTitle("导入作品");
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    progress.setStyleSheet(R"(
        QProgressDialog { background: #1c1e22; color: #edf0f4; }
        QLabel { color: #edf0f4; }
        QProgressBar { background: #25282d; color: #edf0f4; border: 1px solid #414751; border-radius: 5px; text-align: center; }
        QProgressBar::chunk { background: #6476aa; border-radius: 4px; }
        QPushButton { background: #303958; color: #d3dbff; border: 1px solid #56669b; border-radius: 6px; padding: 6px 15px; }
    )");
    progress.setValue(0);

    for (int i = 0; i < drafts.size(); ++i) {
        if (progress.wasCanceled()) {
            rollback();
            if (!error.isEmpty()) QMessageBox::warning(this, "清理失败", error);
            return;
        }

        const auto &draft = drafts.at(i);
        Chapter chapter;
        chapter.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        chapter.title = draft.title;
        chapter.filePath = StoragePaths::chapterRelativePath(chapter.id);
        for (const auto codePoint : draft.content.toUcs4()) {
            if (!QChar::isSpace(codePoint)) ++chapter.wordCount;
        }

        if (!ChapterStore::saveChapter(projectDir, chapter.id, draft.content, error)) {
            rollback();
            QMessageBox::warning(this, "导入正文失败", error);
            return;
        }
        project.chapters.append(chapter);
        project.lastChapterId = chapter.id;
        progress.setValue(i + 1);
    }

    if (progress.wasCanceled()) {
        rollback();
        if (!error.isEmpty()) QMessageBox::warning(this, "清理失败", error);
        return;
    }
    if (!ProjectStore::saveProject(project, projectsPath_, error)) {
        rollback();
        QMessageBox::warning(this, "保存作品失败", error);
        return;
    }
    progress.setValue(drafts.size() + 1);
    refreshProjects();
    selectProject(project.id);
}

void Paper::editSelectedProject() {
    auto *current = projectList->currentItem();
    if (!current) return;
    const QString id = current->data(Qt::UserRole).toString();
    const QString proPath = StoragePaths::projectFilePath(
        StoragePaths::projectDirectory(projectsPath_, id));

    Project project;
    QString error;
    if (!ProjectStore::loadProject(proPath, project, error) || project.id != id) {
        QMessageBox::warning(this, "打开作品失败",
                             error.isEmpty() ? "作品 ID 与目录不一致" : error);
        return;
    }

    CreateProjectDialog dialog(projectsPath_, this);
    dialog.loadProject(project);
    if (dialog.exec() != QDialog::Accepted) return;

    project.name = dialog.projectName();
    project.authorName = dialog.projectAuthor();
    project.genre = dialog.projectGenre();
    project.tags = dialog.projectTags();
    project.synopsis = dialog.projectSynopsis();
    project.updatedAt = QDateTime::currentDateTimeUtc();

    if (!ProjectStore::saveProject(project, projectsPath_, error)) {
        QMessageBox::warning(this, "保存作品信息失败", error);
        return;
    }

    emit projectInfoChanged(project);
    refreshProjects();
    selectProject(id);
}

void Paper::showProjectDetails(const QString &projectId)
{
    for (const auto &project : bookProjects) {
        if (project.id != projectId) {
            continue;
        }

        bookNameLabel->setText(project.name);
        genreLabel->setText(project.genre);
        tagsLabel->setText(project.tags.isEmpty()
            ? QString("标签：暂无标签")
            : QString("标签：%1").arg(project.tags.join(" · ")));
        updatedLabel->setText(
            QString("最近编辑：%1").arg(project.updated)
        );

        chapterCountLabel->setText(
            QString::number(project.chapterCount)
        );
        wordCountLabel->setText(
            QLocale().toString(project.wordCount)
        );

        synopsisLabel->setText(project.synopsis);
        lastChapterLabel->setText(project.lastChapter);
        return;
    }
}

void Paper::createProject()
{
    CreateProjectDialog dialog(projectsPath_, this);
    dialog.setDefaultAuthorName(authorName);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    Project project;
    project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.name = dialog.projectName();
    project.genre = dialog.projectGenre();
    project.tags = dialog.projectTags();
    project.synopsis = dialog.projectSynopsis();
    project.createdAt = QDateTime::currentDateTimeUtc();
    project.updatedAt = project.createdAt;
    project.authorName = dialog.projectAuthor();
    QString error;

    if (!ProjectStore::saveProject(project, projectsPath_, error)) {
        QMessageBox::warning(this, "创建作品失败", error);
        return;
    }

    bookProjects.prepend(makePreview(project));
    renderProjectList();
    projectList->setCurrentRow(0);

}

void Paper::headerStyle() {
    setStyleSheet(R"(
    /* 作品库页面背景 */
    QWidget#libraryPage {
        background-color: #1c1e22;
    }

    /* 标题和说明的公共样式 */
    QLabel#pageTitle,
    QLabel#pageDescription {
        background-color: transparent;
        border: none;
        padding: 0px;
        font-family: "Microsoft YaHei UI";
    }

    /* 页面标题 */
    QLabel#pageTitle {
        color: #edf0f4;
        font-size: 24px;
        font-weight: bold;
    }

    /* 页面说明 */
    QLabel#pageDescription {
        color: #a1a9b4;
        font-size: 12px;
    }

    /* 顶部按钮的公共样式 */
    QPushButton#importButton,
    QPushButton#archiveButton,
    QPushButton#createButton {
        border: 1px solid #373c43;
        border-radius: 6px;
        padding: 8px 15px;
        font-family: "Microsoft YaHei UI";
        font-size: 12px;
    }

    /* 导入按钮 */
    QPushButton#importButton,
    QPushButton#archiveButton {
        background-color: #25282d;
        color: #edf0f4;
    }

    QPushButton#importButton:hover,
    QPushButton#archiveButton:hover {
        background-color: #30343a;
    }

    QPushButton#importButton:pressed,
    QPushButton#archiveButton:pressed {
        background-color: #202327;
    }

    /* 新建作品按钮 */
    QPushButton#createButton {
        background-color: #303958;
        border-color: #4b5887;
        color: #bac5ff;
        font-weight: bold;
    }

    QPushButton#createButton:hover {
        background-color: #394567;
    }

    QPushButton#createButton:pressed {
        background-color: #29324d;
    }

    /* 键盘焦点 */
    QPushButton#importButton:focus,
    QPushButton#archiveButton:focus,
    QPushButton#createButton:focus {
        border-color: #9caaff;
    }

    /* 禁用状态 */
    QPushButton#importButton:disabled,
    QPushButton#archiveButton:disabled,
    QPushButton#createButton:disabled {
        background-color: #202327;
        border-color: #373c43;
        color: #68717d;
    }
)");
}

void Paper::emptyStyle() {
    emptyPage->setStyleSheet(R"(
    QFrame#emptyPage {
        background-color: #202327;
        border: 1px dashed #3d434d;
        border-radius: 9px;
    }

    QLabel {
        background-color: transparent;
        color: #a1a9b4;
        border: none;
        font-family: "Microsoft YaHei UI";
        font-size: 12px;
    }

    QLabel#emptyIcon {
        background-color: #303958;
        border: 1px solid #454f74;
        border-radius: 18px;
    }

    QLabel#emptyTitle {
        color: #edf0f4;
        font-size: 19px;
        font-weight: bold;
    }

    QLabel#emptyDescription {
        color: #a1a9b4;
        font-size: 12px;
    }

    QLabel#emptyNote {
        color: #7e8794;
        font-size: 11px;
        padding-top: 4px;
    }

    QPushButton {
        background-color: #25282d;
        color: #edf0f4;
        border: 1px solid #373c43;
        border-radius: 6px;
        padding: 8px 15px;
        font-family: "Microsoft YaHei UI";
        font-size: 12px;
    }

    QPushButton:hover {
        background-color: #30343a;
    }

    QPushButton:pressed {
        background-color: #1c1e22;
    }

    QPushButton#emptyCreateButton {
        background-color: #303958;
        border-color: #4b5887;
        color: #bac5ff;
        font-weight: bold;
    }

    QPushButton#emptyCreateButton:hover {
        background-color: #394567;
    }

    QPushButton#emptyCreateButton:pressed {
        background-color: #29324d;
    }

    QPushButton:focus {
        border-color: #9caaff;
    }
)");
}

void Paper::fileStyle() {
    filledPage->setStyleSheet(R"(
    QFrame#listPanel,
    QFrame#detailPanel {
        background-color: #25282d;
        border: 1px solid #373c43;
        border-radius: 9px;
    }
    QLabel {
        background-color: transparent;
        color: #edf0f4;
        border: none;
        font-size: 13px;
    }
/* 列表区域 */
QListWidget#projectList {
    background-color: transparent;
    color: #cbd1da;
    border: none;
    padding: 0px;
    font-family: "Microsoft YaHei UI";
    font-size: 12px;
     outline: 0px;
}

/* 普通作品卡片 */
QListWidget#projectList::item {
    background-color: #202327;
    color: #cbd1da;
    border: 1px solid transparent;
    border-radius: 7px;
    padding: 0px;
}

/* 鼠标悬停 */
QListWidget#projectList::item:hover {
    background-color: #2c3037;
    border-color: #3d434d;
}

/* 当前选中 */
QListWidget#projectList::item:selected {
    background-color: #303958;
    border-color: #4b5887;
    color: #dce2ff;
}

/* 焦点移到其他控件后，保留选中效果 */
QListWidget#projectList::item:selected:!active {
    background-color: #303958;
    border-color: #4b5887;
    color: #dce2ff;
}

/* 已选中卡片的悬停效果 */
QListWidget#projectList::item:selected:hover {
    background-color: #374263;
    border-color: #6271a4;
}
/* 垂直滚动条：透明轨道 */
QListWidget#projectList QScrollBar:vertical {
    background-color: transparent;
    width: 10px;
    margin: 0px;
    border: none;
}
QLabel#listTitle {
    color: #edf0f4;
    font-family: "Microsoft YaHei UI";
    font-size: 13px;
    font-weight: bold;
    padding-bottom: 6px;
}
/* 滑块 */
QListWidget#projectList QScrollBar::handle:vertical {
    background-color: #4b5260;
    min-height: 28px;
    border-radius: 5px;
}

QListWidget#projectList QScrollBar::handle:vertical:hover {
    background-color: #68717d;
}

QListWidget#projectList QScrollBar::handle:vertical:pressed {
    background-color: #7d89b7;
}

/* 隐藏上下箭头按钮 */
QListWidget#projectList QScrollBar::add-line:vertical,
QListWidget#projectList QScrollBar::sub-line:vertical {
    height: 0px;
    border: none;
    background-color: transparent;
}

QListWidget#projectList QScrollBar::up-arrow:vertical,
QListWidget#projectList QScrollBar::down-arrow:vertical {
    width: 0px;
    height: 0px;
}

/* 滑块上下的轨道 */
QListWidget#projectList QScrollBar::add-page:vertical,
QListWidget#projectList QScrollBar::sub-page:vertical {
    background-color: transparent;
}
QLabel#bookCover {
    background-color: #303958;
    border: 1px solid #566080;
    border-radius: 5px;
}

QLabel#bookName {
    color: #edf0f4;
    font-size: 23px;
    font-weight: bold;
}
QLabel#bookGenre {
    background-color: #313847;
    color: #b4bfda;
    border-radius: 4px;
    padding: 3px 8px;
    font-size: 11px;
}

QLabel#bookTags {
    color: #b4bfda;
    font-size: 12px;
}

QLabel#bookUpdated {
    color: #a1a9b4;
    font-size: 11px;
}
QFrame#chapterStat,
QFrame#wordStat {
    background-color: #202327;
    border: 1px solid #31353c;
    border-radius: 7px;
}

QLabel#chapterCount,
QLabel#wordCount {
    color: #edf0f4;
    font-size: 22px;
}

QLabel#chapterCaption,
QLabel#wordCaption {
    color: #a1a9b4;
    font-size: 11px;
}
QLabel#synopsisTitle,
QLabel#resumeCaption {
    color: #a1a9b4;
    font-size: 11px;
}

QLabel#synopsisText {
    color: #d2d7df;
    font-size: 12px;
}

QFrame#resumeFrame {
    background-color: #22262c;
    border: 1px solid #373c43;
    border-radius: 6px;
}

QLabel#lastChapter {
    color: #dbe0e8;
    font-size: 12px;
}
QPushButton {
    background-color: #25282d;
    color: #edf0f4;
    border: 1px solid #373c43;
    border-radius: 6px;
    padding: 9px 15px;
    font-family: "Microsoft YaHei UI";
    font-size: 12px;
}

QPushButton:hover {
    background-color: #30343a;
}

QPushButton#continueButton {
    background-color: #303958;
    border-color: #4b5887;
    color: #bac5ff;
    font-weight: bold;
}

QPushButton#continueButton:hover {
    background-color: #394567;
}

QPushButton#moreButton {
    background-color: transparent;
    color: #a1a9b4;
    padding-right: 24px;
}

QPushButton#moreButton:hover {
    background-color: #30343a;
}

/* 给按钮自带的下拉箭头留出位置 */
QPushButton#moreButton::menu-indicator {
    subcontrol-origin: padding;
    subcontrol-position: right center;
    right: 7px;
}

QMenu {
    background-color: #2c3037;
    color: #edf0f4;
    border: 1px solid #454b55;
    border-radius: 7px;
    padding: 5px;
    font-size: 12px;
}

QMenu::item {
    padding: 8px 16px;
    border-radius: 4px;
}

QMenu::item:selected {
    background-color: #3a414c;
}

QMenu::separator {
    height: 1px;
    background-color: #454b55;
    margin: 4px 8px;
}
)");


}

ProjectPreview Paper::makePreview(const Project &project) const {
    ProjectPreview preview;
    preview.name = project.name;
    preview.genre = project.genre;
    preview.tags = project.tags;
    preview.id = project.id;
    preview.updated=project.updatedAt.toLocalTime().toString("yyyy-MM-dd HH:mm");
    preview.chapterCount = project.chapters.size();
    preview.wordCount = 0;
    preview.lastChapter = project.chapters.isEmpty()
        ? "尚未创建章节"
        : "尚未开始写作";
    for (auto &chapter : project.chapters) {
        preview.wordCount += chapter.wordCount;
        if (chapter.id == project.lastChapterId) {
            preview.lastChapter = chapter.title;
        }
    }
    preview.synopsis = project.synopsis.trimmed().isEmpty()
     ? "还没有作品简介。"
     : project.synopsis;



    return preview;
}


void Paper::loadBooksList() {
    bookProjects.clear();
    QStringList warnings;
    const QList<Project> projects =
        ProjectStore::loadProjects(projectsPath_, warnings);
    for (const auto &project : projects) {
        bookProjects.append(makePreview(project));
    }
    if (!warnings.isEmpty()) {
        QTimer::singleShot(0, this, [this, warnings]() {
            QMessageBox::warning(
                this,
                "部分作品读取失败",
                warnings.join("\n")
            );
        });
    }
}
void Paper::renderProjectList()
{
    projectList->clear();

    for (const auto &project : bookProjects) {
        const QString summary =
            QString("%1 个章节 · %2 字")
                .arg(project.chapterCount)
                .arg(QLocale().toString(project.wordCount));

        auto *item = new QListWidgetItem(
            QIcon(":/icons/library.png"), project.name
        );
        item->setData(Qt::UserRole, project.id);
        item->setData(ProjectListDelegate::SummaryRole, summary);
        item->setData(Qt::AccessibleTextRole,
                      QString("%1，%2").arg(project.name, summary));
        item->setToolTip(QString("%1\n%2").arg(project.name, summary));
        projectList->addItem(item);
    }
    projectCountLabel->setText(
    QString("共 %1 部作品").arg(projectList->count())
);

    bodyStack->setCurrentIndex(bookProjects.isEmpty() ? 0 : 1);
}
