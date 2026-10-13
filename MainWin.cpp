//
// Created by shijie on 2026/10/2.
//

#include "MainWin.h"
#include <QMainWindow>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include "WorkPlaces/workingPaper/Paper.h"
#include "WorkPlaces/WritePaper/Writer.h"
#include "WorkPlaces/DesignPaper/designer.h"
#include "WorkPlaces/RelationPaper/Relater.h"
#include "WorkPlaces/SettingPaper/Setter.h"
#include "storage/DataRootMigration.h"
#include "storage/DataRootStore.h"
#include "storage/StoragePaths.h"
#include "storage/ModelServiceIdentity.h"
#include <QApplication>
#include <QIcon>
#include <QStringList>
#include <QSize>
#include <QMessageBox>
#include <QDebug>
#include <QCloseEvent>
#include <QShowEvent>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QShortcut>
#include <QEventLoop>
#include <QProgressDialog>
#include <QFrame>
#include <QStyle>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif
MainWin::MainWin(  const QString &projectsPath,QWidget *parent) : QMainWindow(parent),projectsPath_(projectsPath){
    setWindowTitle("写作工具");
    resize(1100, 720);
   setUi();

}

void MainWin::closeEvent(QCloseEvent *event) {
    if (designerPage && !designerPage->confirmLeave()) {
        event->ignore();
        return;
    }

    if (writerWorkspace_ && !writerWorkspace_->confirmCloseAll()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWin::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const BOOL darkMode = TRUE;
    constexpr DWORD useDarkMode = 20;
    constexpr DWORD borderColorAttribute = 34;
    constexpr DWORD captionColorAttribute = 35;
    constexpr DWORD textColorAttribute = 36;
    const COLORREF background = RGB(28, 30, 34);
    const COLORREF foreground = RGB(237, 240, 244);
    DwmSetWindowAttribute(hwnd, useDarkMode, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, borderColorAttribute, &background, sizeof(background));
    DwmSetWindowAttribute(hwnd, captionColorAttribute, &background, sizeof(background));
    DwmSetWindowAttribute(hwnd, textColorAttribute, &foreground, sizeof(foreground));
#endif
}

void MainWin::setFocusMode(bool enabled) {
    if (focusMode_ == enabled) return;

    if (enabled) {
        if (workPages->currentWidget() != writerWorkspace_ ||
            !writerWorkspace_->hasActiveChapter()) return;
        mainSplitterState_ = mainSplitter_->saveState();
        writerWorkspace_->setFocusMode(true);
        leftNav->hide();
    } else {
        writerWorkspace_->setFocusMode(false);
        leftNav->show();
        mainSplitter_->restoreState(mainSplitterState_);
    }
    focusMode_ = enabled;
    if (exitFocusShortcut_) exitFocusShortcut_->setEnabled(enabled);
}

void MainWin::setUi() {
    central = new QWidget(this);
    central->setObjectName("mainCentral");
    central->setAttribute(Qt::WA_StyledBackground, true);
    central->setStyleSheet(R"(
        QWidget#mainCentral { background-color: #1c1e22; }
        QSplitter::handle { background-color: #1c1e22; }
    )");
    setCentralWidget(central);
    auto * centtalLayout = new QHBoxLayout(central);
    centtalLayout->setContentsMargins(0, 0, 0, 0);

    auto * spliter = new QSplitter(Qt::Horizontal,central);
    mainSplitter_ = spliter;

    centtalLayout->addWidget(spliter);

    // 左右两区
     leftNav =new QWidget(spliter);
     leftNav->setMaximumWidth(220);
     leftNav->setObjectName("leftNav");
     leftNav->setAttribute(Qt::WA_StyledBackground, true);
     workPlace = new QWidget(spliter);

    spliter->addWidget(leftNav);
    spliter->addWidget(workPlace);
    spliter->setSizes({180, 900});       // 初始宽度分配
    spliter->setStretchFactor(0, 0);
    spliter->setStretchFactor(1, 1);     // 窗口变宽时优先扩展右侧


    // 右分区
    auto * workLayout = new QVBoxLayout(workPlace);
    workLayout->setContentsMargins(0, 0, 0, 0);

    workPages = new QStackedWidget(workPlace);
    auto *libraryPage = new Paper(projectsPath_,workPages);
    writerWorkspace_ = new WriterWorkspace(workPages);
    workPages->addWidget(libraryPage);
    workPages->addWidget(writerWorkspace_);
    designerPage = new designer(workPages);
    workPages->addWidget(designerPage);
    relaterPage_ = new Relater(workPages);
    workPages->addWidget(relaterPage_);
    settingsPage_ = new Setter(workPages);
    libraryPage->setDefaultAuthorName(settingsPage_->defaultAuthorName());
    writerWorkspace_->setInlineSuggestionEnabled(
        settingsPage_->inlineSuggestionEnabled());
    workPages->addWidget(settingsPage_);
    workLayout->addWidget(workPages);
    workPages->setCurrentIndex(0);



    // 左侧功能栏


    auto *leftLayout = new QVBoxLayout(leftNav);
    leftLayout->setContentsMargins(12, 18, 12, 12);
    leftLayout->setSpacing(6);
    auto * titleLab = new QLabel(leftNav);
    auto * subtitleLab = new QLabel(leftNav);
    titleLab->setText("Writer Studio");
    subtitleLab->setText("小说写作工作台");
    titleLab->setObjectName("brandTitle");
    subtitleLab->setObjectName("brandSubtitle");
    leftLayout->addWidget(titleLab);
    leftLayout->addWidget(subtitleLab);
    buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(true);
    const QStringList iconPaths = {
        ":/icons/library.png",
        ":/icons/writing.png",
        ":/icons/planning.png",
        ":/icons/graph.png",
        ":/icons/settings.png"
    };

    int idex = 0;
    for (auto &name :funIcons) {
        auto * btn = new QPushButton(name,leftNav);
        btn->setIcon(QIcon(iconPaths[idex]));
        btn->setToolTip(name);
        btn->setIconSize(QSize(18, 18));
        btn->setCheckable(true);
        buttonGroup->addButton(btn,idex);
        leftLayout->addWidget(btn);
        connect(btn,&QPushButton::clicked,this,[this,idex]() {
            if (workPages->currentWidget() == designerPage && idex !=2 && !designerPage->confirmLeave()) {
                buttonGroup->button(2)->setChecked(true);
                return;
            }
            workPages->setCurrentIndex(idex);
        });
        idex++;
    }
    buttonGroup->button(0)->setChecked(true);
    leftLayout->addStretch();

    modelAvailabilityBadge_ = new QFrame(leftNav);
    modelAvailabilityBadge_->setObjectName("modelAvailabilityBadge");
    modelAvailabilityBadge_->setAccessibleName("模型服务状态");
    auto *modelBadgeLayout = new QHBoxLayout(modelAvailabilityBadge_);
    modelBadgeLayout->setContentsMargins(10, 7, 10, 7);
    modelBadgeLayout->setSpacing(7);
    modelAvailabilityDot_ = new QLabel(modelAvailabilityBadge_);
    modelAvailabilityDot_->setObjectName("modelAvailabilityDot");
    modelAvailabilityDot_->setFixedSize(9, 9);
    modelAvailabilityText_ = new QLabel(modelAvailabilityBadge_);
    modelAvailabilityText_->setObjectName("modelAvailabilityText");
    modelAvailabilityText_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modelBadgeLayout->addWidget(modelAvailabilityDot_, 0, Qt::AlignVCenter);
    modelBadgeLayout->addWidget(modelAvailabilityText_, 1, Qt::AlignVCenter);
    leftLayout->addWidget(modelAvailabilityBadge_);

    auto *authorEntry = new QFrame(leftNav);
    authorEntry->setObjectName("authorEntry");
    auto *authorEntryLayout = new QHBoxLayout(authorEntry);
    authorEntryLayout->setContentsMargins(8, 12, 4, 2);
    authorEntryLayout->setSpacing(9);
    authorAvatarButton_ = new QPushButton(authorEntry);
    authorAvatarButton_->setObjectName("authorAvatarButton");
    authorAvatarButton_->setFixedSize(42, 42);
    authorAvatarButton_->setIconSize(QSize(40, 40));
    authorAvatarButton_->setCursor(Qt::PointingHandCursor);
    authorAvatarButton_->setAccessibleName("打开作者设置");
    authorNameButton_ = new QPushButton("设置作者", authorEntry);
    authorNameButton_->setObjectName("authorNameButton");
    authorNameButton_->setCursor(Qt::PointingHandCursor);
    authorNameButton_->setAccessibleName("打开作者设置");
    authorEntryLayout->addWidget(authorAvatarButton_);
    authorEntryLayout->addWidget(authorNameButton_, 1);
    leftLayout->addWidget(authorEntry);

    updateAuthorEntry(settingsPage_->defaultAuthorName());
    updateAuthorAvatar();
    const auto showAuthorSettings = [this]() {
        if (workPages->currentWidget() == designerPage && !designerPage->confirmLeave()) return;
        settingsPage_->showAuthorSection();
        workPages->setCurrentIndex(4);
        buttonGroup->button(4)->setChecked(true);
    };
    connect(authorAvatarButton_, &QPushButton::clicked, this, showAuthorSettings);
    connect(authorNameButton_, &QPushButton::clicked, this, showAuthorSettings);
    connect(settingsPage_, &Setter::defaultAuthorNameChanged,
            this, &MainWin::updateAuthorEntry);
    connect(settingsPage_, &Setter::authorAvatarChanged,
            this, &MainWin::updateAuthorAvatar);
    connect(settingsPage_, &Setter::globalWritingConventionChanged,
            designerPage, &designer::refreshEffectiveConvention);
    connect(settingsPage_, &Setter::inlineSuggestionEnabledChanged,
            writerWorkspace_, &WriterWorkspace::setInlineSuggestionEnabled);
    connect(settingsPage_, &Setter::activeModelChanged,
            this, &MainWin::selectModelIdentity);
    const auto updateModelBadge = [this](const QString &key, const QString &name,
                                          const QString &state, const QString &details) {
        if (key.isEmpty() || key != modelStatusKey_) return;
        modelStatusName_ = name;
        setModelAvailability(state, details);
    };
    connect(settingsPage_, &Setter::modelAvailabilityChanged,
            this, updateModelBadge);
    connect(writerWorkspace_, &WriterWorkspace::modelAvailabilityChanged,
            this, updateModelBadge);
    connect(settingsPage_, &Setter::dataRootChangeRequested,
            this, [this](const QString &targetRoot) {
        const QString sourceRoot = QFileInfo(projectsPath_).dir().absolutePath();
        QString error;
        if (!DataRootMigration::validateDestination(sourceRoot, targetRoot, error)) {
            QMessageBox::warning(this, "无法更换作品库位置", error);
            return;
        }
        if (writerWorkspace_->hasOpenProjects()) {
            QMessageBox::information(this, "请先关闭作品",
                                     "请保存并关闭写作页的所有作品标签，再更换作品库位置。");
            return;
        }
        if (designerPage && !designerPage->confirmLeave()) return;

        const QString message = QString(
            "将作品和归档复制到新目录：\n%1\n\n复制并核对成功后，程序会保存新位置并退出。"
            "重新打开程序即可使用新目录；旧目录会保留。")
                                    .arg(QDir::toNativeSeparators(targetRoot));
        if (QMessageBox::question(this, "确认更换作品库位置", message,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes)
            return;

        QProgressDialog progress("正在复制作品库…", QString(), 0, 0, this);
        progress.setWindowTitle("更换作品库位置");
        progress.setCancelButton(nullptr);
        progress.setWindowModality(Qt::ApplicationModal);
        progress.setMinimumDuration(0);
        progress.show();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        const bool copied = DataRootMigration::copyLibrary(sourceRoot, targetRoot, error);
        bool verified = false;
        if (copied) {
            progress.setLabelText("正在核对复制的文件…");
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            verified = DataRootMigration::verifyCopy(sourceRoot, targetRoot, error);
        }
        QApplication::restoreOverrideCursor();
        progress.close();
        if (!verified) {
            QMessageBox::warning(this, "更换作品库位置失败",
                                 QString("%1\n\n旧作品库仍在使用。新目录可能留有部分副本：\n%2")
                                     .arg(error, QDir::toNativeSeparators(targetRoot)));
            return;
        }
        if (!DataRootStore::save(targetRoot, error)) {
            QMessageBox::warning(this, "保存新位置失败",
                                 QString("%1\n\n旧作品库仍在使用；复制到新目录的文件已保留。")
                                     .arg(error));
            return;
        }
        QMessageBox::information(this, "作品库位置已更换",
                                 "作品和归档已复制并核对。程序即将退出；重新打开后使用新位置。"
                                 "旧目录仍保留，可作为备份。");
        QCoreApplication::quit();
    });
    leftNavStyle();
    ModelServiceIdentity identity;
    QString modelError;
    if (ModelServiceIdentityStore::loadActive(identity, modelError))
        selectModelIdentity(identity.key, identity.displayName);
    else {
        modelStatusName_ = "模型";
        setModelAvailability("error", modelError);
    }

    connect(writerWorkspace_, &WriterWorkspace::libraryRequested,
        this, [this]() {
    workPages->setCurrentIndex(0);
    buttonGroup->button(0)->setChecked(true);
});

    auto *focusShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    focusShortcut->setContext(Qt::WindowShortcut);
    focusShortcut->setAutoRepeat(false);
    connect(focusShortcut, &QShortcut::activated, this,
            [this] { setFocusMode(!focusMode_); });

    exitFocusShortcut_ = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    exitFocusShortcut_->setContext(Qt::WindowShortcut);
    exitFocusShortcut_->setEnabled(false);
    connect(exitFocusShortcut_, &QShortcut::activated, this,
            [this] { if (focusMode_) setFocusMode(false); });

    connect(writerWorkspace_, &WriterWorkspace::focusModeExitRequested,
            this, [this] { setFocusMode(false); });
    connect(workPages, &QStackedWidget::currentChanged, this,
            [this](int) {
                if (focusMode_ && workPages->currentWidget() != writerWorkspace_)
                    setFocusMode(false);
            });

    connect(libraryPage, &Paper::openProjectRequested,
        this, [this](const QString &id) {
        QString error;

        if (!writerWorkspace_->openProject(projectsPath_, id, error)) {
            if (!error.isEmpty()) QMessageBox::warning(this, "打开作品失败", error);
            return;
        }


        workPages->setCurrentIndex(1);
        buttonGroup->button(1)->setChecked(true);
    });

    connect(libraryPage, &Paper::openPlanningRequested,
            this, [this](const QString &id) {
        QString error;
        if (!writerWorkspace_->openProject(projectsPath_, id, error, false)) {
            if (!error.isEmpty()) QMessageBox::warning(this, "打开作品失败", error);
            return;
        }
        workPages->setCurrentWidget(designerPage);
        buttonGroup->button(2)->setChecked(true);
        designerPage->showOutline();
    });

    connect(libraryPage,&Paper::archiveProjectRequested,this,[this,libraryPage](const QString &id) {
        const QString oldPath = StoragePaths::projectDirectory(projectsPath_, id);
        const QString rootPath = StoragePaths::archiveDirectory(projectsPath_);
        const QString newPath = QDir(rootPath).filePath(id);

        if (!QFileInfo(oldPath).isDir()) {
            QMessageBox::critical(this,"归档失败","目录不存在");
            return ;
        }

        if (!QFileInfo::exists(rootPath)) {
            if (!QDir().mkdir(rootPath)) {
                QMessageBox::critical(this,"归档失败","归档目录创建失败");
                return;
            }
        }
        if (QFileInfo::exists(newPath)) {
            QMessageBox::warning(this, "归档失败", "归档目录中已存在这部作品");
            return;
        }

        if (!writerWorkspace_->closeProject(id))return ;

        if (!QDir().rename(oldPath, newPath)) {
            QMessageBox::warning(this,"归档失败","移动失败");
            return;
        }

        libraryPage->refreshProjects();

    });
    connect(settingsPage_, &Setter::defaultAuthorNameChanged,libraryPage,[this,libraryPage]() {
        libraryPage->setDefaultAuthorName(settingsPage_->defaultAuthorName());
    });

    connect(libraryPage, &Paper::restoreProjectRequested,
            this, [this, libraryPage](const QString &id) {
        const QString archiveRoot = StoragePaths::archiveDirectory(projectsPath_);
        const QString source = QDir(archiveRoot).filePath(id);
        const QString target = StoragePaths::projectDirectory(projectsPath_, id);

        if (!QFileInfo(source).isDir()) {
            QMessageBox::warning(this, "恢复失败", "归档中的作品目录不存在");
            return;
        }
        if (QFileInfo::exists(target)) {
            QMessageBox::warning(this, "恢复失败", "作品库中已存在同一部作品");
            return;
        }
        if (!QDir().rename(source, target)) {
            QMessageBox::warning(this, "恢复失败",
                                 QString("无法将作品移回作品库：\n%1\n→\n%2")
                                     .arg(source, target));
            return;
        }

        libraryPage->refreshProjects();
        libraryPage->selectProject(id);
        QMessageBox::information(this, "恢复成功", "作品已恢复到作品库");
    });
    connect(writerWorkspace_, &WriterWorkspace::currentProjectChanged,
            this, [this](const QString &id) {
        currentProjectId = id;
        designerPage->setProjectId(projectsPath_, id);
        relaterPage_->setProjectId(projectsPath_, id);
    });
    designerPage->setProjectId(projectsPath_, writerWorkspace_->currentProjectId());
    relaterPage_->setProjectId(projectsPath_, writerWorkspace_->currentProjectId());
    connect(libraryPage, &Paper::projectInfoChanged,
            writerWorkspace_, &WriterWorkspace::updateOpenProjectInfo);
    connect(libraryPage, &Paper::projectInfoChanged,
            this, [this](const Project &project) {
        if (project.id == currentProjectId) {
            designerPage->setProjectId(projectsPath_, project.id);
            relaterPage_->setProjectId(projectsPath_, project.id);
        }
    });
    connect(relaterPage_, &Relater::entityOpenRequested,
            this, [this](const QString &entityId) {
        if (entityId.isEmpty()) return;
        workPages->setCurrentWidget(designerPage);
        buttonGroup->button(2)->setChecked(true);
        designerPage->selectEntityById(entityId);
    });
    connect(designerPage, &designer::locateEntityInGraphRequested,
            this, [this](const QString &entityId) {
        if (entityId.isEmpty() || !designerPage->confirmLeave()) return;
        workPages->setCurrentWidget(relaterPage_);
        buttonGroup->button(3)->setChecked(true);
        if (!relaterPage_->locateEntityById(entityId))
            QMessageBox::warning(this, "定位资料失败", "关系图谱中找不到这条资料，请重新打开作品后重试。");
    });
    connect(workPages, &QStackedWidget::currentChanged,
        this, [this, libraryPage](int index) {
        if (index == 0) {
            libraryPage->refreshProjects();
        } else if (index == 1) {
            writerWorkspace_->refreshCurrentReferences();
        } else if (index == 2) {
            designerPage->setProjectId(projectsPath_, writerWorkspace_->currentProjectId());
        } else if (index == 3) {
            relaterPage_->setProjectId(projectsPath_, writerWorkspace_->currentProjectId());
        }
    });

}

void MainWin::updateAuthorEntry(const QString &name) {
    const QString displayName = name.trimmed();
    authorNameButton_->setText(displayName.isEmpty()
        ? "设置作者"
        : QFontMetrics(authorNameButton_->font()).elidedText(
              displayName, Qt::ElideRight, 108));
    authorNameButton_->setToolTip(displayName.isEmpty() ? "打开作者设置" : displayName);
}

void MainWin::updateAuthorAvatar() {
    authorAvatarButton_->setIcon(QIcon(settingsPage_->avatarPixmap(40)));
}

void MainWin::selectModelIdentity(const QString &modelKey,
                                  const QString &modelName) {
    modelStatusKey_ = modelKey;
    modelStatusName_ = modelName;
    setModelAvailability("unknown", "尚未检测；测试模型或请求短语建议后更新。");
}

void MainWin::setModelAvailability(const QString &state,
                                    const QString &details) {
    const QString stateText = state == "available" ? "可用"
        : state == "pending" ? "检测中"
        : state == "error" ? "请求失败" : "尚未检测";
    const QString fullText = modelStatusName_.isEmpty()
        ? QStringLiteral("模型 · %1").arg(stateText)
        : QStringLiteral("%1 · %2").arg(modelStatusName_, stateText);
    modelAvailabilityText_->setText(
        modelAvailabilityText_->fontMetrics().elidedText(fullText, Qt::ElideRight, 156));
    const QString description = QStringLiteral("当前模型：%1\n%2")
        .arg(modelStatusName_.isEmpty() ? QStringLiteral("未配置") : modelStatusName_,
             details.isEmpty() ? stateText : details);
    modelAvailabilityBadge_->setToolTip(description);
    modelAvailabilityDot_->setToolTip(description);
    modelAvailabilityText_->setToolTip(description);
    modelAvailabilityBadge_->setAccessibleDescription(description);
    for (QWidget *widget : QList<QWidget *>{modelAvailabilityBadge_,
                                            modelAvailabilityDot_,
                                            modelAvailabilityText_}) {
        widget->setProperty("state", state);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
}

void MainWin::leftNavStyle() {
    leftNav->setStyleSheet(R"(
    /* 导航背景 */
    QWidget#leftNav {
        background-color: #25282d;
        border-right: 1px solid #373c43;
    }

    /* 普通标签：分组标题、底部说明 */
    QLabel {
        background-color: transparent;
        color: #a1a9b4;
        border: none;
        font-family: "Microsoft YaHei UI";
        font-size: 12px;
        padding: 2px 10px;
    }

    /* 应用名称 */
    QLabel#brandTitle {
        color: #edf0f4;
        font-size: 17px;
        font-weight: bold;
        padding: 4px 10px 0px 10px;
    }

    /* 应用副标题 */
    QLabel#brandSubtitle {
        color: #a1a9b4;
        font-size: 12px;
        padding: 0px 10px 14px 10px;
    }

    /* 导航按钮：普通状态 */
    QPushButton {
        background-color: transparent;
        color: #edf0f4;
        border: 1px solid transparent;
        border-radius: 6px;
        padding: 9px 10px;
        text-align: left;
        font-family: "Microsoft YaHei UI";
        font-size: 14px;
        outline: none;
    }

    /* 鼠标悬停 */
    QPushButton:hover {
        background-color: #202327;
    }

    /* 鼠标按下 */
    QPushButton:pressed {
        background-color: #373c43;
    }

    /* 当前选中 */
    QPushButton:checked {
        background-color: #303958;
        color: #9caaff;
        font-weight: bold;
    }

    /* 选中按钮的悬停状态 */
    QPushButton:checked:hover {
        background-color: #374263;
    }

    /* 选中按钮的按下状态 */
    QPushButton:checked:pressed {
        background-color: #2b334d;
    }

    /* 键盘焦点 */
    QPushButton:focus {

    }

    /* 禁用状态 */
    QPushButton:disabled {
        background-color: transparent;
        color: #68717d;
        border-color: transparent;
    }

    QFrame#modelAvailabilityBadge {
        background-color: #20242a;
        border: 1px solid #414751;
        border-radius: 7px;
    }
    QFrame#modelAvailabilityBadge[state="available"] {
        background-color: #21342c;
        border-color: #426e58;
    }
    QFrame#modelAvailabilityBadge[state="pending"] {
        background-color: #393125;
        border-color: #75613d;
    }
    QFrame#modelAvailabilityBadge[state="error"] {
        background-color: #3a292b;
        border-color: #845253;
    }
    QLabel#modelAvailabilityDot {
        background-color: #8b95a3;
        border-radius: 4px;
        padding: 0;
    }
    QLabel#modelAvailabilityDot[state="available"] { background-color: #81c9a2; }
    QLabel#modelAvailabilityDot[state="pending"] { background-color: #e0b769; }
    QLabel#modelAvailabilityDot[state="error"] { background-color: #e28e8e; }
    QLabel#modelAvailabilityText {
        color: #aab3c0;
        font-size: 12px;
        padding: 0;
    }
    QLabel#modelAvailabilityText[state="available"] { color: #a7dec0; }
    QLabel#modelAvailabilityText[state="pending"] { color: #efd098; }
    QLabel#modelAvailabilityText[state="error"] { color: #efaaaa; }

    QFrame#authorEntry {
        background-color: transparent;
        border-top: 1px solid #41464f;
    }
    QPushButton#authorAvatarButton {
        background-color: transparent;
        border: 1px solid transparent;
        border-radius: 21px;
        padding: 0;
        text-align: center;
    }
    QPushButton#authorAvatarButton:hover {
        border-color: #8293cb;
    }
    QPushButton#authorNameButton {
        background-color: transparent;
        color: #cbd1da;
        border: 1px solid transparent;
        border-radius: 5px;
        padding: 6px 2px;
        text-align: left;
        font-size: 12px;
    }
    QPushButton#authorNameButton:hover {
        color: #edf0f4;
        background-color: #30343a;
    }
)");

}
