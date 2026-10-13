//
// Created by shijie on 2026/10/5.
//

#include "WriterWorkspace.h"
#include "TabCloseButton.h"
#include <QTabBar>
#include <QVBoxLayout>
#include <QIcon>
WriterWorkspace::WriterWorkspace(QWidget *parent) :QWidget(parent){
    setUi();
}

bool WriterWorkspace::openProject(const QString &projectsPath, const QString &projectId,
                                  QString &error, bool resumeWriting) {
   error.clear();

   if (openedProjects_.contains(projectId)) {
       auto *page = openedProjects_[projectId];
       if (resumeWriting && !page->resumeLastChapter()) return false;
       page->refreshReferences();
       tabs_->setCurrentWidget(page);
       bodyStack_->setCurrentIndex(1);
       return true;
   }
    auto *page = new Writer();
    page->setInlineSuggestionEnabled(inlineSuggestionEnabled_);

    if (!page->openProject(projectsPath, projectId, error)) {
        page->deleteLater();
        return false;
    }
    if (resumeWriting && !page->resumeLastChapter()) {
        page->deleteLater();
        return false;
    }

    connect(page, &Writer::libraryRequested,
        this, &WriterWorkspace::libraryRequested);
    connect(page, &Writer::modelAvailabilityChanged,
            this, &WriterWorkspace::modelAvailabilityChanged);

    openedProjects_.insert(projectId, page);
    const int index = tabs_->addTab(
     page,
     QIcon(":/icons/library.png"),
     page->projectName()
 );

    tabs_->setTabToolTip(index, page->projectName());

    // 替换默认叉号；点击时按页面查找索引，关闭其他标签后也能找到正确的作品。
    auto *oldCloseButton = tabs_->tabBar()->tabButton(index, QTabBar::RightSide);
    auto *closeButton = new TabCloseButton(tabs_->tabBar());
    tabs_->tabBar()->setTabButton(index, QTabBar::RightSide, closeButton);
    if (oldCloseButton) oldCloseButton->deleteLater();
    connect(closeButton, &QAbstractButton::clicked, this, [this, page] {
        const int currentIndex = tabs_->indexOf(page);
        if (currentIndex >= 0) emit tabs_->tabCloseRequested(currentIndex);
    });

    tabs_->setCurrentWidget(page);
    bodyStack_->setCurrentIndex(1);


    return true;

}

void WriterWorkspace::setInlineSuggestionEnabled(bool enabled) {
    if (inlineSuggestionEnabled_ == enabled) return;
    inlineSuggestionEnabled_ = enabled;
    for (Writer *page : openedProjects_)
        page->setInlineSuggestionEnabled(enabled);
}

QString WriterWorkspace::currentProjectId() const {
    auto * page = qobject_cast<Writer *>(tabs_->currentWidget());

    if (!page)return {};

    return page->projectId();

}

bool WriterWorkspace::hasActiveChapter() const {
    const auto *page = qobject_cast<Writer *>(tabs_->currentWidget());
    return page && page->hasActiveChapter();
}

void WriterWorkspace::refreshCurrentReferences() {
    if (auto *page = qobject_cast<Writer *>(tabs_->currentWidget()))
        page->refreshReferences();
}

void WriterWorkspace::setFocusMode(bool enabled) {
    if (focusMode_ == enabled) return;

    if (enabled) {
        auto *page = qobject_cast<Writer *>(tabs_->currentWidget());
        if (!page || !page->hasActiveChapter()) return;
        focusedPage_ = page;
        page->setFocusMode(true);
        tabs_->tabBar()->hide();
        layout()->setContentsMargins(0, 0, 0, 0);
    } else {
        if (focusedPage_) focusedPage_->setFocusMode(false);
        focusedPage_ = nullptr;
        tabs_->tabBar()->show();
        layout()->setContentsMargins(16, 14, 16, 14);
    }
    focusMode_ = enabled;
}

bool WriterWorkspace::confirmCloseAll() {

    for (int row = 0 ; row < tabs_->count(); row++) {
        tabs_->setCurrentIndex(row);
        auto *page = qobject_cast<Writer *>(tabs_->currentWidget());
        if (!page) return false;
        if (!page->confirmClose()) return false;

    }

    return true;
}

bool WriterWorkspace::closeProject(const QString &projectId) {
    auto *page = openedProjects_.value(projectId,nullptr);
    if (!page) return true;
    const int index = tabs_->indexOf(page);
    if (index < 0) return false;
    if (!page->confirmClose()) return false;

    page->clearReferences();
    openedProjects_.remove(projectId);
    tabs_->removeTab(index);
    page->deleteLater();

    if (tabs_->count() == 0)
        bodyStack_->setCurrentIndex(0);

    return true;
}

void WriterWorkspace::updateOpenProjectInfo(const Project &project) {
    auto *page = openedProjects_.value(project.id, nullptr);
    if (!page) return;

    page->updateProjectInfo(project);
    const int index = tabs_->indexOf(page);
    if (index < 0) return;
    tabs_->setTabText(index, project.name);
    tabs_->setTabToolTip(index, project.name);
}

void WriterWorkspace::setUi() {
    setObjectName("writerWorkspace");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);

    bodyStack_ = new QStackedWidget(this);
    bodyStack_->setObjectName("writerBodysStack");
    layout->addWidget(bodyStack_, 1);
    // 空页
    auto *emptyPage = new QFrame(bodyStack_);
    emptyPage->setObjectName("writerEmptysPage");
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(0);

    auto *emptyTitle = new QLabel("尚未打开作品", emptyPage);
    emptyTitle->setObjectName("writerEmptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyTitle->setText("打开作品，开始写作");




    auto *emptyDescription = new QLabel(
        "到作品库选择或新建一部作品，然后继续写作。", emptyPage);
    emptyDescription->setText(
       "在作品库中双击作品，或选中后点击“继续写作”。\n"
       "打开多部作品后，可通过顶部标签切换。"
   );
    emptyDescription->setObjectName("writerEmptyDescription");
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyDescription->setWordWrap(true);

    auto *libraryButton = new QPushButton("前往作品库", emptyPage);
    libraryButton->setObjectName("writerLibraryButton");
    libraryButton->setText("前往作品库");
    libraryButton->setCursor(Qt::PointingHandCursor);

    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyTitle);
    // 控件铺满可用宽度，文字在控件内居中，换行高度由布局计算。
    emptyLayout->addWidget(emptyDescription);
    emptyLayout->addSpacing(8);
    emptyLayout->addWidget(libraryButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    bodyStack_->addWidget(emptyPage);

    //
    tabs_  = new QTabWidget(this);
    tabs_->setObjectName("writerProjectTabs");
    tabs_->tabBar()->setObjectName("writerProjectTabBar");
    tabs_->setTabsClosable(true);

    connect(tabs_, &QTabWidget::tabCloseRequested,
            this, &WriterWorkspace::closeProjectTab);
    tabs_->setDocumentMode(true);
    tabs_->setUsesScrollButtons(true);
    tabs_->setElideMode(Qt::ElideRight);
    tabs_->setIconSize(QSize(16, 16));

    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setDrawBase(false);
    bodyStack_->addWidget(tabs_);

    bodyStack_->setCurrentIndex(0);
    connect(tabs_,&QTabWidget::currentChanged,this,[this](int) {
        refreshCurrentReferences();
        if (focusMode_) {
            auto *page = qobject_cast<Writer *>(tabs_->currentWidget());
            if (!page || !page->hasActiveChapter()) {
                emit focusModeExitRequested();
            } else if (page != focusedPage_) {
                if (focusedPage_) focusedPage_->setFocusMode(false);
                focusedPage_ = page;
                focusedPage_->setFocusMode(true);
            }
        }
        emit currentProjectChanged(currentProjectId());
    });
    connect(libraryButton, &QPushButton::clicked, this, &WriterWorkspace::libraryRequested);
    uiStyle();
}

void WriterWorkspace::uiStyle()
{
    setStyleSheet(R"QSS(
        /* 工作区背景 */
        QWidget#writerWorkspace {
            background-color: #1c1e22;
        }

        QStackedWidget#writerBodysStack {
            background-color: transparent;
            border: none;
        }

        /* 空页面板 */
        QFrame#writerEmptysPage {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 12px;
        }

        /* 限定在外层空页中的标签 */
        QFrame#writerEmptysPage QLabel {
            background-color: transparent;
            border: none;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
            color: #929dab;
        }

        QFrame#writerEmptysPage QLabel#writerEmptyTitle {
            color: #e1e6ee;
            font-size: 22px;
            font-weight: bold;
            padding: 4px 0;
        }

        QFrame#writerEmptysPage QLabel#writerEmptyDescription {
            color: #929dab;
            font-size: 13px;
            padding: 4px 0;
            min-height: 40px;
        }

        /* 空页入口按钮 */
        QFrame#writerEmptysPage QPushButton#writerLibraryButton {
            background-color: #303958;
            color: #c2cbff;
            border: 1px solid #4b5887;
            border-radius: 8px;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
            font-weight: bold;
            padding: 9px 20px;
            min-width: 110px;
            min-height: 22px;
            outline: none;
        }

        QFrame#writerEmptysPage QPushButton#writerLibraryButton:hover {
            background-color: #394567;
            border-color: #6575aa;
            color: #dde3ff;
        }

        QFrame#writerEmptysPage QPushButton#writerLibraryButton:pressed {
            background-color: #29324b;
            border-color: #46537c;
        }

        QFrame#writerEmptysPage QPushButton#writerLibraryButton:focus {
            border-color: #8a96c0;
        }

        /* 标签容器：去掉默认外框 */
        QTabWidget#writerProjectTabs {
            background-color: #1c1e22;
            border: none;
        }

        QTabWidget#writerProjectTabs::pane {
            background-color: #1c1e22;
            border: none;
        }

        QTabWidget#writerProjectTabs::tab-bar {
            alignment: left;
            left: 0px;
        }

        /* 标签栏背景 */
        QTabBar#writerProjectTabBar {
            background-color: #1c1e22;
            border: none;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }

        /* 未选中的标签 */
        QTabBar#writerProjectTabBar::tab {
            background-color: transparent;
            color: #a9b2c0;
            border: 1px solid transparent;
            border-radius: 7px;
            padding: 8px 20px 8px 14px;
            /* 上下留白一致，使标题、书本图标和关闭按钮的中心线重合 */
            margin: 6px 6px 6px 0px;
            min-width: 90px;
            max-width: 200px;
        }

        /* 鼠标经过 */
        QTabBar#writerProjectTabBar::tab:hover {
            background-color: #272d37;
            color: #dce1e8;
            border-color: #3d4655;
        }

        /* 当前选中的作品 */
        QTabBar#writerProjectTabBar::tab:selected {
            background-color: #2d3651;
            color: #dde4ff;
            border-color: #54648d;
        }

        QTabBar#writerProjectTabBar::tab:selected:hover {
            background-color: #343f5c;
            border-color: #697ba6;
        }

        QTabBar#writerProjectTabBar::tab:disabled {
            background-color: transparent;
            color: #626c7b;
            border-color: transparent;
        }

        /* 默认按钮的尺寸与位置；叉号及状态颜色由 TabCloseButton 绘制 */
        QTabBar#writerProjectTabBar::close-button {
            image: url(:/icons/tab-close.png);
            subcontrol-position: right center;
            subcontrol-origin: padding;
            width: 20px;
            height: 20px;
            border: none;
            border-radius: 4px;
            background-color: transparent;
        }

        QTabBar#writerProjectTabBar::close-button:hover {
            image: url(:/icons/tab-close-hover.png);
            background-color: #3d4655;
        }

        QTabBar#writerProjectTabBar::close-button:pressed {
            image: url(:/icons/tab-close-pressed.png);
            background-color: #303958;
        }

        /* 标签过多时的滚动按钮 */
        QTabBar#writerProjectTabBar::scroller {
            width: 26px;
        }

        QTabBar#writerProjectTabBar QToolButton {
            background-color: #25282d;
            color: #b8c3ed;
            border: 1px solid #3d4655;
            border-radius: 5px;
            padding: 0px;
            margin: 4px 2px 8px 2px;
        }

        QTabBar#writerProjectTabBar QToolButton:hover {
            background-color: #303958;
            border-color: #596b95;
        }

        QTabBar#writerProjectTabBar QToolButton:pressed {
            background-color: #29324b;
        }

        QTabBar#writerProjectTabBar QToolButton:disabled {
            background-color: #202327;
            color: #596270;
            border-color: #30353d;
        }
    )QSS");
}

void WriterWorkspace::closeProjectTab(int index) {
    auto * page = qobject_cast<Writer*>(tabs_->widget(index));

    if (page) closeProject(page->projectId());

}
