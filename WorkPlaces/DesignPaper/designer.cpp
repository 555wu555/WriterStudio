//
// Created by shijie on 2026/10/3.
//

#include "designer.h"
#include "CreateEntityDialog.h"
#include "../../models/Project.h"
#include "../../models/StorySetup.h"
#include "../../storage/ProjectStore.h"
#include "../../storage/StoragePaths.h"
#include "../../storage/SetUp/SetupStore.h"
#include "../../storage/GlobalWritingConventionStore.h"
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include  <QListWidget>
#include <QListView>
#include  <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include  <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStringList>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QUuid>
#include <QVBoxLayout>
#include "../../storage/EntityStore/EntityStore.h"
namespace {
QString entityKindText(const QString &kind) {
    if (kind == QStringLiteral("person")) return QStringLiteral("人物");
    if (kind == QStringLiteral("place")) return QStringLiteral("地点");
    if (kind == QStringLiteral("organization")) return QStringLiteral("组织");
    if (kind == QStringLiteral("item")) return QStringLiteral("物件");
    return kind.isEmpty() ? QStringLiteral("未分类") : kind;
}

QLabel *fieldLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("setupFieldLabel");
    return label;
}

QLabel *entityDetailText(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setObjectName("setupEntityDetailText");
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QPlainTextEdit *textField(const QString &placeholder, int minimumHeight,
                          QWidget *parent) {
    auto *edit = new QPlainTextEdit(parent);
    edit->setObjectName("setupTextEdit");
    edit->setPlaceholderText(placeholder);
    edit->setMinimumHeight(minimumHeight);
    return edit;
}

QScrollArea *scrollPage(QWidget *content, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("setupScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    return scroll;
}
}

designer::designer(QWidget *parent) : QWidget(parent) {
    buildUi();
    applyStyle();
}

void designer::buildUi() {
    setObjectName("setupPage");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(0);

    pageStack_ = new QStackedWidget(this);
    pageStack_->setObjectName("setupPageStack");
    root->addWidget(pageStack_);

    auto *statePage = new QWidget(pageStack_);
    auto *stateLayout = new QVBoxLayout(statePage);
    stateLayout->setContentsMargins(24, 24, 24, 24);
    stateLabel_ = new QLabel("请先在作品库打开一部作品", statePage);
    stateLabel_->setObjectName("setupStateLabel");
    stateLabel_->setAlignment(Qt::AlignCenter);
    stateLabel_->setWordWrap(true);
    stateLayout->addWidget(stateLabel_);
    pageStack_->addWidget(statePage);

    auto *editorPage = new QWidget(pageStack_);
    editorPage->setObjectName("setupEditorPage");
    auto *editorLayout = new QVBoxLayout(editorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(18);

    auto *header = new QHBoxLayout;
    header->setSpacing(16);
    auto *headingTexts = new QVBoxLayout;
    headingTexts->setSpacing(4);
    projectNameLabel_ = new QLabel(editorPage);
    projectNameLabel_->setObjectName("setupTitle");
    projectMetaLabel_ = new QLabel(editorPage);
    projectMetaLabel_->setObjectName("setupSubtitle");
    headingTexts->addWidget(projectNameLabel_);
    headingTexts->addWidget(projectMetaLabel_);
    header->addLayout(headingTexts, 1);

    saveStatusLabel_ = new QLabel("● 已保存", editorPage);
    saveStatusLabel_->setObjectName("setupSaveStatus");
    saveStatusLabel_->setProperty("state", "saved");
    saveButton_ = new QPushButton("保存设定", editorPage);
    saveButton_->setObjectName("setupSaveButton");
    saveButton_->setEnabled(false);
    saveButton_->setToolTip("保存当前作品的设定");
    header->addWidget(saveStatusLabel_, 0, Qt::AlignVCenter);
    header->addWidget(saveButton_, 0, Qt::AlignVCenter);
    editorLayout->addLayout(header);

    auto *body = new QSplitter(Qt::Horizontal, editorPage);
    body->setObjectName("setupMainSplitter");
    body->setChildrenCollapsible(false);
    body->setHandleWidth(12);
    editorLayout->addWidget(body, 1);

    auto *nav = new QFrame(editorPage);
    nav->setObjectName("setupNav");
    nav->setMinimumWidth(145);
    nav->setMaximumWidth(320);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(12, 16, 12, 14);
    navLayout->setSpacing(10);
    auto *navTitle = new QLabel("作品资料", nav);
    navTitle->setObjectName("setupNavTitle");
    sectionList_ = new QListWidget(nav);
    sectionList_->setObjectName("setupSectionList");
    sectionList_->addItems({"故事大纲", "写作约定", "人物与世界"});
    sectionList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sectionList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navLayout->addWidget(navTitle);
    navLayout->addWidget(sectionList_, 1);
    body->addWidget(nav);

    sectionStack_ = new QStackedWidget(editorPage);
    sectionStack_->setObjectName("setupSectionStack");
    body->addWidget(sectionStack_);
    body->setStretchFactor(0, 0);
    body->setStretchFactor(1, 1);
    body->setSizes({188, 900});
    connect(sectionList_, &QListWidget::currentRowChanged,
            sectionStack_, &QStackedWidget::setCurrentIndex);

    auto *storyContent = new QWidget;
    storyContent->setObjectName("setupContent");
    auto *storyLayout = new QVBoxLayout(storyContent);
    storyLayout->setContentsMargins(0, 0, 12, 0);
    storyLayout->setSpacing(0);
    auto *storyCard = new QFrame(storyContent);
    storyCard->setObjectName("setupCard");
    storyCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *storyFields = new QVBoxLayout(storyCard);
    storyFields->setContentsMargins(26, 24, 26, 28);
    storyFields->setSpacing(12);
    auto *storyTitle = new QLabel("故事大纲", storyCard);
    storyTitle->setObjectName("setupSectionTitle");
    auto *storyHint = new QLabel("记录故事的方向。所有内容都可以稍后补充和调整。", storyCard);
    storyHint->setObjectName("setupDescription");
    storyHint->setWordWrap(true);
    coreIdeaEdit_ = textField("用几句话写下故事的核心冲突、主角目标或独特设想…",
                              120, storyCard);
    outlineEdit_ = textField("自由记录故事主线；也可参考三幕结构：\n"
                             "第一幕｜人物与起点\n第二幕｜冲突升级\n第三幕｜转折与结局",
                           310, storyCard);
    storyFields->addWidget(storyTitle);
    storyFields->addWidget(storyHint);
    storyFields->addSpacing(8);
    storyFields->addWidget(fieldLabel("核心设想", storyCard));
    storyFields->addWidget(coreIdeaEdit_);
    storyFields->addSpacing(8);
    storyFields->addWidget(fieldLabel("整体大纲", storyCard));
    storyFields->addWidget(outlineEdit_);
    storyLayout->addWidget(storyCard);
    storyLayout->addStretch();
    sectionStack_->addWidget(scrollPage(storyContent, sectionStack_));

    auto *rulesContent = new QWidget;
    rulesContent->setObjectName("setupContent");
    auto *rulesLayout = new QVBoxLayout(rulesContent);
    rulesLayout->setContentsMargins(0, 0, 12, 0);
    rulesLayout->setSpacing(0);
    auto *rulesCard = new QFrame(rulesContent);
    rulesCard->setObjectName("setupCard");
    rulesCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *rulesFields = new QVBoxLayout(rulesCard);
    rulesFields->setContentsMargins(26, 24, 26, 28);
    rulesFields->setSpacing(12);
    auto *rulesTitle = new QLabel("写作约定", rulesCard);
    rulesTitle->setObjectName("setupSectionTitle");
    auto *rulesHint = new QLabel("这部作品只要填写任意一项，就使用整组作品约定；全部留空时使用设置页的全局约定。", rulesCard);
    rulesHint->setObjectName("setupDescription");
    rulesHint->setWordWrap(true);
    viewpointCombo_ = new QComboBox(rulesCard);
    viewpointCombo_->setObjectName("setupCombo");
    viewpointCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    viewpointCombo_->addItems({"未指定", "第一人称", "第三人称限知", "第三人称全知", "多视角"});
    viewpointCombo_->setEditable(true);
    tenseCombo_ = new QComboBox(rulesCard);
    tenseCombo_->setObjectName("setupCombo");
    tenseCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tenseCombo_->addItems({"未指定", "过去时", "现在时", "交替使用"});
    tenseCombo_->setEditable(true);
    auto *shortFields = new QFormLayout;
    shortFields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    shortFields->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    shortFields->setHorizontalSpacing(20);
    shortFields->setVerticalSpacing(14);
    shortFields->addRow(fieldLabel("叙事视角", rulesCard), viewpointCombo_);
    shortFields->addRow(fieldLabel("叙事时态", rulesCard), tenseCombo_);
    styleEdit_ = textField("例如：短句为主；情绪通过动作呈现；对话简洁…",
                           145, rulesCard);
    rhythmEdit_ = textField("例如：关键场景用短句；平静段落放慢叙述…",
                            145, rulesCard);
    constraintsEdit_ = textField("例如：不要提前揭示人物秘密；避免使用全知视角…",
                                 145, rulesCard);
    rulesFields->addWidget(rulesTitle);
    rulesFields->addWidget(rulesHint);
    rulesFields->addSpacing(8);
    rulesFields->addLayout(shortFields);
    rulesFields->addSpacing(8);
    rulesFields->addWidget(fieldLabel("语言风格", rulesCard));
    rulesFields->addWidget(styleEdit_);
    rulesFields->addSpacing(8);
    rulesFields->addWidget(fieldLabel("遣词与节奏", rulesCard));
    rulesFields->addWidget(rhythmEdit_);
    rulesFields->addSpacing(8);
    rulesFields->addWidget(fieldLabel("必须遵守或避免的事项", rulesCard));
    rulesFields->addWidget(constraintsEdit_);
    auto *effectiveCard = new QFrame(rulesContent);
    effectiveCard->setObjectName("setupCard");
    auto *effectiveLayout = new QVBoxLayout(effectiveCard);
    effectiveLayout->setContentsMargins(26, 20, 26, 22);
    effectiveLayout->setSpacing(10);
    ruleSourceLabel_ = new QLabel("当前生效的写作约定", effectiveCard);
    ruleSourceLabel_->setObjectName("setupSectionTitle");
    effectiveRulesLabel_ = new QLabel(effectiveCard);
    effectiveRulesLabel_->setObjectName("setupDescription");
    effectiveRulesLabel_->setTextFormat(Qt::PlainText);
    effectiveRulesLabel_->setWordWrap(true);
    effectiveRulesLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    effectiveLayout->addWidget(ruleSourceLabel_);
    effectiveLayout->addWidget(effectiveRulesLabel_);
    rulesLayout->addWidget(effectiveCard);
    rulesLayout->addSpacing(16);
    rulesLayout->addWidget(rulesCard);
    rulesLayout->addStretch();
    sectionStack_->addWidget(scrollPage(rulesContent, sectionStack_));

    auto *entityPage = new QWidget(sectionStack_);
    entityPage->setObjectName("setupEntityPage");
    auto *entityLayout = new QVBoxLayout(entityPage);
    entityLayout->setContentsMargins(0, 0, 0, 0);
    entityLayout->setSpacing(0);

    auto *entitySplitter = new QSplitter(Qt::Horizontal, entityPage);
    entitySplitter->setObjectName("setupEntitySplitter");
    entitySplitter->setChildrenCollapsible(false);
    entityLayout->addWidget(entitySplitter, 1);

    auto *listPane = new QFrame;
    listPane->setObjectName("setupEntityPane");
    listPane->setMinimumWidth(190);
    auto *listLayout = new QVBoxLayout(listPane);
    listLayout->setContentsMargins(16, 18, 16, 16);
    listLayout->setSpacing(12);
    auto *listTitle = new QLabel("资料列表", listPane);
    listTitle->setObjectName("setupEntityPaneTitle");
    auto *listHeader = new QHBoxLayout;
    listHeader->setSpacing(8);
    listHeader->addWidget(listTitle);
    listHeader->addStretch();
    auto *newEntityButton = new QPushButton("＋ 新建", listPane);
    newEntityButton->setObjectName("setupNewEntityButton");
    listHeader->addWidget(newEntityButton);
    entityFilterCombo_ = new QComboBox(listPane);
    entityFilterCombo_->setObjectName("setupEntityFilter");
    entityFilterCombo_->setAccessibleName("按资料类别筛选");
    entityFilterCombo_->addItem("全部资料", QString());
    entityFilterCombo_->addItem("人物", "person");
    entityFilterCombo_->addItem("地点", "place");
    entityFilterCombo_->addItem("组织", "organization");
    entityFilterCombo_->addItem("物件", "item");
    auto *filterPopup = new QListView(entityFilterCombo_);
    filterPopup->setObjectName("setupEntityFilterPopup");
    entityFilterCombo_->setView(filterPopup);
    entityList_ = new QListWidget(listPane);
    entityList_->setObjectName("setupEntityList");
    entityList_->setVisible(false);
    entityEmptyLabel_ = new QLabel("还没有人物或世界资料", listPane);
    entityEmptyLabel_->setObjectName("setupEntityEmptyText");
    entityEmptyLabel_->setAlignment(Qt::AlignCenter);
    entityEmptyLabel_->setWordWrap(true);
    listLayout->addLayout(listHeader);
    listLayout->addWidget(entityFilterCombo_);
    listLayout->addWidget(entityList_, 1);
    listLayout->addWidget(entityEmptyLabel_, 1);
    entitySplitter->addWidget(listPane);

    auto *detailPane = new QFrame;
    detailPane->setObjectName("setupEntityPane");
    detailPane->setMinimumWidth(260);
    auto *detailLayout = new QVBoxLayout(detailPane);
    detailLayout->setContentsMargins(20, 20, 20, 20);
    entityDetailStack_ = new QStackedWidget(detailPane);
    entityDetailStack_->setObjectName("setupEntityDetailStack");
    detailLayout->addWidget(entityDetailStack_);

    auto *detailEmptyPage = new QWidget(entityDetailStack_);
    auto *detailEmptyLayout = new QVBoxLayout(detailEmptyPage);
    detailEmptyLayout->setContentsMargins(0, 0, 0, 0);
    auto *detailEmptyLabel = new QLabel("选择一条资料查看详情", detailEmptyPage);
    detailEmptyLabel->setObjectName("setupEntityEmptyText");
    detailEmptyLabel->setAlignment(Qt::AlignCenter);
    detailEmptyLabel->setWordWrap(true);
    detailEmptyLayout->addWidget(detailEmptyLabel, 1);
    entityDetailStack_->addWidget(detailEmptyPage);

    auto *detailContent = new QWidget;
    detailContent->setObjectName("setupEntityDetailContent");
    auto *detailFields = new QVBoxLayout(detailContent);
    detailFields->setContentsMargins(4, 4, 12, 20);
    detailFields->setSpacing(16);
    auto *detailHeader = new QHBoxLayout;
    detailHeader->setSpacing(12);
    entityNameLabel_ = entityDetailText(detailContent);
    entityNameLabel_->setObjectName("setupEntityDetailName");
    viewInGraphButton_ = new QPushButton("查看图谱", detailContent);
    viewInGraphButton_->setObjectName("setupViewInGraphButton");
    viewInGraphButton_->setToolTip("在关系图谱中定位这条资料");
    viewInGraphButton_->setEnabled(false);
    auto *editEntityButton = new QPushButton("编辑", detailContent);
    editEntityButton->setObjectName("setupEditEntityButton");
    auto *removeEntityButton = new QPushButton("删除", detailContent);
    removeEntityButton->setObjectName("setupRemoveEntityButton");
    detailHeader->addWidget(entityNameLabel_, 1);
    detailHeader->addWidget(viewInGraphButton_, 0, Qt::AlignTop);
    detailHeader->addWidget(editEntityButton, 0, Qt::AlignTop);
    detailHeader->addWidget(removeEntityButton, 0, Qt::AlignTop);
    entityKindLabel_ = new QLabel(detailContent);
    entityKindLabel_->setObjectName("setupEntityDetailKind");
    entityKindLabel_->setTextFormat(Qt::PlainText);
    detailFields->addLayout(detailHeader);
    detailFields->addWidget(entityKindLabel_, 0, Qt::AlignLeft);
    detailFields->addSpacing(2);

    entityRoleRow_ = new QFrame(detailContent);
    entityRoleRow_->setObjectName("setupEntityDetailCard");
    auto *roleLayout = new QVBoxLayout(entityRoleRow_);
    roleLayout->setContentsMargins(18, 15, 18, 17);
    roleLayout->setSpacing(9);
    auto *roleHeading = new QLabel("角色定位", entityRoleRow_);
    roleHeading->setObjectName("setupEntityDetailHeading");
    entityRoleLabel_ = entityDetailText(entityRoleRow_);
    roleLayout->addWidget(roleHeading);
    roleLayout->addWidget(entityRoleLabel_);
    detailFields->addWidget(entityRoleRow_);

    auto *summaryCard = new QFrame(detailContent);
    summaryCard->setObjectName("setupEntityDetailCard");
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 15, 18, 17);
    summaryLayout->setSpacing(9);
    auto *summaryHeading = new QLabel("简介", summaryCard);
    summaryHeading->setObjectName("setupEntityDetailHeading");
    entitySummaryLabel_ = entityDetailText(summaryCard);
    summaryLayout->addWidget(summaryHeading);
    summaryLayout->addWidget(entitySummaryLabel_);
    detailFields->addWidget(summaryCard);

    entityBackgroundRow_ = new QFrame(detailContent);
    entityBackgroundRow_->setObjectName("setupEntityDetailCard");
    auto *backgroundLayout = new QVBoxLayout(entityBackgroundRow_);
    backgroundLayout->setContentsMargins(18, 15, 18, 17);
    backgroundLayout->setSpacing(9);
    auto *backgroundHeading = new QLabel("人物背景", entityBackgroundRow_);
    backgroundHeading->setObjectName("setupEntityDetailHeading");
    entityBackgroundLabel_ = entityDetailText(entityBackgroundRow_);
    backgroundLayout->addWidget(backgroundHeading);
    backgroundLayout->addWidget(entityBackgroundLabel_);
    detailFields->addWidget(entityBackgroundRow_);

    auto *notesCard = new QFrame(detailContent);
    notesCard->setObjectName("setupEntityDetailCard");
    auto *notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(18, 15, 18, 19);
    notesLayout->setSpacing(10);
    auto *notesHeading = new QLabel("详细设定", notesCard);
    notesHeading->setObjectName("setupEntityDetailHeading");
    entityNotesLabel_ = entityDetailText(notesCard);
    notesLayout->addWidget(notesHeading);
    notesLayout->addWidget(entityNotesLabel_);
    detailFields->addWidget(notesCard);
    detailFields->addStretch();

    entityDetailScroll_ = scrollPage(detailContent, entityDetailStack_);
    entityDetailScroll_->setObjectName("setupEntityDetailScroll");
    entityDetailStack_->addWidget(entityDetailScroll_);
    entityDetailStack_->setCurrentIndex(0);
    entitySplitter->addWidget(detailPane);
    entitySplitter->setStretchFactor(0, 0);
    entitySplitter->setStretchFactor(1, 1);
    entitySplitter->setSizes({270, 700});
    sectionStack_->addWidget(entityPage);
    connect(newEntityButton, &QPushButton::clicked, this, [this]() {
        CreateEntityDialog dialog(this);
        if (dialog.exec()!=QDialog::Accepted) return;
        Entity entity;
        entity.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entity.kind = dialog.kind();
        entity.name = dialog.name();
        entity.notes = dialog.notes();
        entity.summary = dialog.summary();
        entity.background = dialog.background();
        entity.role = dialog.role();
        const QString projectDir = StoragePaths::projectDirectory(projectsPath_, id_);
        WorldData candidate;
        QString error;
        if (!EntityStore::load(projectDir, candidate, error)) {
            QMessageBox::warning(this, "新建资料失败", QString("读取最新资料失败：%1").arg(error));
            return;
        }
        candidate.entities.append(entity);
        if (!EntityStore::save(projectDir,candidate,error)) {
            QMessageBox::critical(this,"新建资料失败",QString("新建失败:%1").arg(error));
            return;
        }
        worldData_ = candidate;
        if (!entityFilterCombo_->currentData().toString().isEmpty() &&
            entityFilterCombo_->currentData().toString() != entity.kind) {
            const QSignalBlocker blocker(entityFilterCombo_);
            entityFilterCombo_->setCurrentIndex(0);
        }
        refreshEntityList(entity.id);
    });
    connect(entityList_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) {
                showSelectedEntity(current);
            });
    connect(entityFilterCombo_, &QComboBox::currentIndexChanged,
            this, [this]() { refreshEntityList(); });
    connect(viewInGraphButton_, &QPushButton::clicked, this, [this]() {
        const QListWidgetItem *item = entityList_->currentItem();
        if (!item || id_.isEmpty()) return;
        const QString entityId = item->data(Qt::UserRole).toString();
        if (!entityId.isEmpty()) emit locateEntityInGraphRequested(entityId);
    });
    connect(editEntityButton, &QPushButton::clicked,
            this, &designer::editSelectedEntity);
    connect(removeEntityButton, &QPushButton::clicked,
            this, &designer::removeSelectedEntity);

    sectionList_->setCurrentRow(0);
    connect(saveButton_, &QPushButton::clicked, this, &designer::saveSetup);
    connect(coreIdeaEdit_, &QPlainTextEdit::textChanged, this, &designer::markDirty);
    connect(outlineEdit_, &QPlainTextEdit::textChanged, this, &designer::markDirty);
    connect(styleEdit_, &QPlainTextEdit::textChanged, this, &designer::markDirty);
    connect(rhythmEdit_, &QPlainTextEdit::textChanged, this, &designer::markDirty);
    connect(constraintsEdit_, &QPlainTextEdit::textChanged, this, &designer::markDirty);
    connect(viewpointCombo_, &QComboBox::currentTextChanged, this, &designer::markDirty);
    connect(tenseCombo_, &QComboBox::currentTextChanged, this, &designer::markDirty);
    pageStack_->addWidget(editorPage);
    pageStack_->setCurrentIndex(0);
}

void designer::clearForm() {
    applySetup(storySetup{});
}

void designer::applySetup(const storySetup &setup) {
    QScopedValueRollback<bool> loading(loadingForm_, true);
    coreIdeaEdit_->setPlainText(setup.coreIdea);
    outlineEdit_->setPlainText(setup.outline);
    styleEdit_->setPlainText(setup.style);
    rhythmEdit_->setPlainText(setup.rhythm);
    constraintsEdit_->setPlainText(setup.constraints);
    if (setup.viewpoint.isEmpty())
        viewpointCombo_->setCurrentIndex(0);
    else
        viewpointCombo_->setCurrentText(setup.viewpoint);
    if (setup.tense.isEmpty())
        tenseCombo_->setCurrentIndex(0);
    else
        tenseCombo_->setCurrentText(setup.tense);
    dirty_ = false;
    saveButton_->setEnabled(false);
    setSaveStatus("saved", "● 已保存");
    refreshEffectiveConvention();
}

storySetup designer::collectSetup() const {
    storySetup setup;
    setup.coreIdea = coreIdeaEdit_->toPlainText();
    setup.outline = outlineEdit_->toPlainText();
    setup.style = styleEdit_->toPlainText();
    setup.rhythm = rhythmEdit_->toPlainText();
    setup.constraints = constraintsEdit_->toPlainText();

    const QString viewpoint = viewpointCombo_->currentText().trimmed();
    const QString tense = tenseCombo_->currentText().trimmed();
    setup.viewpoint = viewpoint == "未指定" ? QString() : viewpoint;
    setup.tense = tense == "未指定" ? QString() : tense;
    return setup;
}

void designer::markDirty() {
    if (loadingForm_ || id_.isEmpty()) return;
    refreshEffectiveConvention();
    dirty_ = true;
    saveButton_->setEnabled(true);
    setSaveStatus("modified", "● 未保存");
}

bool designer::saveSetup() {
    if (id_.isEmpty() || !dirty_) return true;

    QString error;
    const QString projectDir = StoragePaths::projectDirectory(projectsPath_, id_);
    if (!SetupStore::save(projectDir, collectSetup(), error)) {
        setSaveStatus("error", "● 保存失败");
        QMessageBox::warning(this, "保存设定失败", error);
        return false;
    }

    dirty_ = false;
    saveButton_->setEnabled(false);
    setSaveStatus("saved", "● 已保存");
    refreshEffectiveConvention();
    return true;
}

void designer::refreshEffectiveConvention() {
    if (id_.isEmpty()) return;
    const storySetup project = collectSetup();
    const WritingConvention local = project.writingConvention();
    const bool useGlobal = local.isEmpty();
    WritingConvention global;
    QString error;
    if (useGlobal && !GlobalWritingConventionStore::load(global, error)) {
        ruleSourceLabel_->setText("全局约定读取失败");
        effectiveRulesLabel_->setText(error);
        return;
    }
    const WritingConvention effective = effectiveWritingConvention(project, global);
    ruleSourceLabel_->setText(useGlobal ? "当前生效：全局写作约定"
                                        : "当前生效：本作品写作约定");
    if (effective.isEmpty()) {
        effectiveRulesLabel_->setText("尚未填写写作约定。");
        return;
    }
    QStringList lines;
    auto add = [&lines](const QString &name, const QString &value) {
        if (!value.trimmed().isEmpty()) lines << name + "：" + value.trimmed();
    };
    add("叙事视角", effective.viewpoint);
    add("叙事时态", effective.tense);
    add("语言风格", effective.style);
    add("遣词与节奏", effective.rhythm);
    add("必须遵守或避免的事项", effective.constraints);
    effectiveRulesLabel_->setText(lines.join("\n\n"));
}

void designer::setSaveStatus(const QString &state, const QString &text) {
    saveStatusLabel_->setText(text);
    saveStatusLabel_->setProperty("state", state);
    saveStatusLabel_->style()->unpolish(saveStatusLabel_);
    saveStatusLabel_->style()->polish(saveStatusLabel_);
}

bool designer::confirmLeave() {
    if (!dirty_) return true;

    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Question);
    dialog.setWindowTitle("作品设定尚未保存");
    dialog.setText("当前作品的设定有未保存的修改。");
    dialog.setInformativeText("离开前要如何处理这些修改？");
    auto *saveButton = dialog.addButton("保存", QMessageBox::AcceptRole);
    auto *discardButton = dialog.addButton("放弃修改", QMessageBox::DestructiveRole);
    auto *cancelButton = dialog.addButton("取消", QMessageBox::RejectRole);
    dialog.setDefaultButton(saveButton);
    dialog.setEscapeButton(cancelButton);
    dialog.exec();

    if (dialog.clickedButton() == saveButton) return saveSetup();
    if (dialog.clickedButton() == discardButton) {
        storySetup savedSetup;
        QString error;
        const QString projectDir = StoragePaths::projectDirectory(projectsPath_, id_);
        if (!SetupStore::load(projectDir, savedSetup, error)) {
            QMessageBox::warning(this, "放弃修改失败", error);
            return false;
        }
        applySetup(savedSetup);
        return true;
    }
    return false;
}

void designer::setProjectId(const QString &projectsPath, const QString &id) {
    if (id.isEmpty()) {
        id_.clear();
        projectsPath_ = projectsPath;
        worldData_ = WorldData{};
        refreshEntityList();
        clearForm();
        stateLabel_->setText("请先在作品库打开一部作品");
        pageStack_->setCurrentIndex(0);
        return;
    }

    const bool projectChanged = (id != id_ || projectsPath != projectsPath_);
    const QString projectDir = StoragePaths::projectDirectory(projectsPath, id);
    const QString projectFile = StoragePaths::projectFilePath(projectDir);
    Project project;
    QString error;
    if (!ProjectStore::loadProject(projectFile, project, error) || project.id != id) {
        stateLabel_->setText(QString("读取作品失败：%1")
                                 .arg(error.isEmpty() ? "作品 ID 与目录不一致" : error));
        pageStack_->setCurrentIndex(0);
        return;
    }

    storySetup setup;
    if (projectChanged && !SetupStore::load(projectDir, setup, error)) {
        stateLabel_->setText(QString("读取作品设定失败：%1").arg(error));
        pageStack_->setCurrentIndex(0);
        return;
    }
    WorldData candidate;
    if (!EntityStore::load(projectDir, candidate, error)) {
        stateLabel_->setText(QString("读取作品资料失败：%1").arg(error));
        pageStack_->setCurrentIndex(0);
        return;
    }

    if (projectChanged) {
        {
            const QSignalBlocker blocker(entityFilterCombo_);
            entityFilterCombo_->setCurrentIndex(0);
        }
        applySetup(setup);
        id_ = id;
        projectsPath_ = projectsPath;
        refreshEffectiveConvention();
    }
    // 同一作品返回设定页时只刷新资料，不覆盖尚未保存的设定表单。
    worldData_ = candidate;
    refreshEntityList();

    projectNameLabel_->setText(QString("作品设定 ·《%1》").arg(project.name));
    projectMetaLabel_->setText(project.genre.trimmed().isEmpty()
                                   ? "类型待设定"
                                   : QString("类型 · %1").arg(project.genre));
    pageStack_->setCurrentIndex(1);
}
void designer::showOutline() {
    sectionList_->setCurrentRow(0);
}

void designer::selectEntityById(const QString &entityId) {
    if (entityId.isEmpty() || id_.isEmpty()) return;
    bool exists = false;
    for (const Entity &entity : worldData_.entities) {
        if (entity.id == entityId) {
            exists = true;
            break;
        }
    }
    if (!exists) return;

    sectionList_->setCurrentRow(2); // “人物与世界”
    {
        const QSignalBlocker blocker(entityFilterCombo_);
        entityFilterCombo_->setCurrentIndex(0); // 显示全部，避免目标被分类筛选隐藏
    }
    refreshEntityList(entityId);
    if (entityList_->currentItem())
        entityList_->scrollToItem(entityList_->currentItem());
}

void designer::refreshEntityList(const QString &preferredId) {
    const QString selectedId = preferredId.isEmpty() && entityList_->currentItem()
        ? entityList_->currentItem()->data(Qt::UserRole).toString()
        : preferredId;
    const int previousRow = entityList_->currentRow();
    const QString filterKind = entityFilterCombo_->currentData().toString();
    entityList_->clear();
    int selectedRow = -1;
    for (const Entity &entity : worldData_.entities) {
        if (!filterKind.isEmpty() && entity.kind != filterKind) continue;
        const QString name = entity.name.trimmed().isEmpty()
                                 ? QStringLiteral("未命名资料") : entity.name;
        const QString text = QStringLiteral("%1 · %2")
                                 .arg(entityKindText(entity.kind), name);
        auto *item = new QListWidgetItem(text, entityList_);
        item->setData(Qt::UserRole, entity.id);
        if (entity.id == selectedId) selectedRow = entityList_->count() - 1;
    }

    const bool hasVisibleEntities = entityList_->count() > 0;
    entityList_->setVisible(hasVisibleEntities);
    entityEmptyLabel_->setVisible(!hasVisibleEntities);
    if (!hasVisibleEntities) {
        entityEmptyLabel_->setText(worldData_.entities.isEmpty()
                                       ? "还没有人物或世界资料"
                                       : "该分类暂无资料");
        showSelectedEntity(nullptr);
        return;
    }
    if (selectedRow < 0)
        selectedRow = qBound(0, previousRow, entityList_->count() - 1);
    entityList_->setCurrentRow(selectedRow);
}

void designer::showSelectedEntity(QListWidgetItem *item) {
    viewInGraphButton_->setEnabled(false);
    if (!item) {
        entityDetailStack_->setCurrentIndex(0);
        return;
    }

    const QString entityId = item->data(Qt::UserRole).toString();
    for (const Entity &entity : worldData_.entities) {
        if (entity.id != entityId) continue;

        entityNameLabel_->setText(entity.name.trimmed().isEmpty()
                                      ? QStringLiteral("未命名资料") : entity.name);
        entityKindLabel_->setText(entityKindText(entity.kind));
        const bool isPerson = entity.kind == QStringLiteral("person");
        entityRoleRow_->setVisible(isPerson);
        entityBackgroundRow_->setVisible(isPerson);
        if (isPerson) {
            entityRoleLabel_->setText(entity.role.trimmed().isEmpty()
                                          ? QStringLiteral("未填写") : entity.role);
            entityBackgroundLabel_->setText(entity.background.trimmed().isEmpty()
                                                ? QStringLiteral("未填写") : entity.background);
        }
        entitySummaryLabel_->setText(entity.summary.trimmed().isEmpty()
                                         ? QStringLiteral("未填写") : entity.summary);
        entityNotesLabel_->setText(entity.notes.trimmed().isEmpty()
                                       ? QStringLiteral("未填写") : entity.notes);
        entityDetailStack_->setCurrentIndex(1);
        viewInGraphButton_->setEnabled(!entityId.isEmpty());
        entityDetailScroll_->verticalScrollBar()->setValue(0);
        return;
    }

    entityDetailStack_->setCurrentIndex(0);
}

void designer::editSelectedEntity() {
    const QListWidgetItem *selected = entityList_->currentItem();
    if (!selected || id_.isEmpty()) return;

    const QString entityId = selected->data(Qt::UserRole).toString();
    const QString projectDir = StoragePaths::projectDirectory(projectsPath_, id_);
    WorldData latest;
    QString error;
    if (!EntityStore::load(projectDir, latest, error)) {
        QMessageBox::warning(this, "编辑资料失败", QString("读取最新资料失败：%1").arg(error));
        return;
    }
    worldData_ = latest;
    refreshEntityList(entityId);
    CreateEntityDialog dialog(this);
    bool found = false;
    for (const Entity &entity : latest.entities) {
        if (entity.id != entityId) continue;
        dialog.loadEntity(entity);
        found = true;
        break;
    }
    if (!found) {
        QMessageBox::warning(this, "编辑资料失败", "这条资料已不存在。");
        return;
    }

    while (dialog.exec() == QDialog::Accepted) {
        WorldData candidate;
        if (!EntityStore::load(projectDir, candidate, error)) {
            QMessageBox::warning(this, "保存资料失败", QString("读取最新资料失败：%1").arg(error));
            continue;
        }
        bool updated = false;
        for (Entity &entity : candidate.entities) {
            if (entity.id != entityId) continue;
            entity.kind = dialog.kind();
            entity.name = dialog.name();
            entity.role = dialog.role();
            entity.summary = dialog.summary();
            entity.background = dialog.background();
            entity.notes = dialog.notes();
            updated = true;
            break;
        }
        if (!updated) {
            QMessageBox::warning(this, "保存资料失败", "这条资料已不存在。");
            return;
        }

        if (!EntityStore::save(projectDir, candidate, error)) {
            QMessageBox::warning(this, "保存资料失败", error);
            continue;
        }

        worldData_ = candidate;
        if (!entityFilterCombo_->currentData().toString().isEmpty() &&
            entityFilterCombo_->currentData().toString() != dialog.kind()) {
            const QSignalBlocker blocker(entityFilterCombo_);
            entityFilterCombo_->setCurrentIndex(0);
        }
        refreshEntityList(entityId);
        return;
    }
}

void designer::removeSelectedEntity() {
    const QListWidgetItem *selected = entityList_->currentItem();
    if (!selected || id_.isEmpty()) return;

    const QString entityId = selected->data(Qt::UserRole).toString();
    const QString projectDir = StoragePaths::projectDirectory(projectsPath_, id_);
    WorldData latest;
    QString error;
    if (!EntityStore::load(projectDir, latest, error)) {
        QMessageBox::warning(this, "删除资料失败", QString("读取最新资料失败：%1").arg(error));
        return;
    }
    worldData_ = latest;
    refreshEntityList(entityId);
    QString entityName;
    bool found = false;
    for (const Entity &entity : latest.entities) {
        if (entity.id != entityId) continue;
        entityName = entity.name;
        found = true;
        break;
    }
    if (!found) {
        QMessageBox::warning(this, "删除资料失败", "这条资料已不存在。");
        return;
    }

    int relatedCount = 0;
    for (const Relationship &relation : latest.relationships) {
        if (relation.fromId == entityId || relation.toId == entityId) ++relatedCount;
    }

    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle("删除资料");
    dialog.setText(QString("确定删除“%1”吗？").arg(entityName));
    dialog.setInformativeText(relatedCount == 0
        ? "删除后无法在应用内恢复。"
        : QString("同时会删除与它相关的 %1 条关系。删除后无法在应用内恢复。")
              .arg(relatedCount));
    auto *deleteButton = dialog.addButton("删除资料", QMessageBox::DestructiveRole);
    auto *cancelButton = dialog.addButton("取消", QMessageBox::RejectRole);
    dialog.setDefaultButton(cancelButton);
    dialog.setEscapeButton(cancelButton);
    dialog.exec();
    if (dialog.clickedButton() != deleteButton) return;

    WorldData candidate;
    if (!EntityStore::load(projectDir, candidate, error)) {
        QMessageBox::warning(this, "删除资料失败", QString("读取最新资料失败：%1").arg(error));
        return;
    }
    for (int index = candidate.relationships.size() - 1; index >= 0; --index) {
        const Relationship &relation = candidate.relationships.at(index);
        if (relation.fromId == entityId || relation.toId == entityId)
            candidate.relationships.removeAt(index);
    }
    for (int index = candidate.entities.size() - 1; index >= 0; --index) {
        if (candidate.entities.at(index).id == entityId)
            candidate.entities.removeAt(index);
    }

    if (!EntityStore::save(projectDir, candidate, error)) {
        QMessageBox::warning(this, "删除资料失败", error);
        return;
    }

    worldData_ = candidate;
    refreshEntityList();
}

void designer::applyStyle() {
    setStyleSheet(R"QSS(
        QWidget#setupPage {
            background-color: #1c1e22;
            color: #edf0f4;
            font-family: "Microsoft YaHei UI";
        }
        QStackedWidget#setupPageStack, QStackedWidget#setupSectionStack,
        QStackedWidget#setupEntityDetailStack,
        QWidget#setupEditorPage, QWidget#setupContent, QWidget#setupEntityPage,
        QWidget#setupEntityDetailContent,
        QScrollArea#setupScroll, QScrollArea#setupEntityDetailScroll {
            background: transparent;
            border: none;
        }
        QLabel#setupStateLabel {
            color: #aeb7c4;
            font-size: 16px;
            background: transparent;
        }
        QLabel#setupTitle {
            color: #edf0f4;
            font-size: 22px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#setupSubtitle, QLabel#setupDescription {
            color: #9da7b5;
            font-size: 12px;
            background: transparent;
        }
        QLabel#setupMutedText {
            color: #8793a5;
            font-size: 11px;
            background: transparent;
        }
        QLabel#setupSaveStatus {
            color: #8fc5a3;
            font-size: 12px;
            background: transparent;
        }
        QLabel#setupSaveStatus[state="modified"] { color: #f3c978; }
        QLabel#setupSaveStatus[state="error"] { color: #f38e9d; }
        QFrame#setupNav, QFrame#setupCard {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 10px;
        }
        QLabel#setupNavTitle {
            color: #a4aebe;
            font-size: 12px;
            font-weight: bold;
            padding: 0 8px;
            background: transparent;
        }
        QListWidget#setupSectionList {
            background: transparent;
            color: #cbd1da;
            border: none;
            outline: none;
            font-size: 13px;
        }
        QListWidget#setupSectionList::item {
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 10px 9px;
            margin: 2px 0;
        }
        QListWidget#setupSectionList::item:hover {
            background-color: #2b3038;
            color: #edf0f4;
        }
        QListWidget#setupSectionList::item:selected {
            background-color: #303958;
            border-color: #4b5887;
            color: #c5cfff;
            font-weight: bold;
        }
        QFrame#setupCard QLabel#setupSectionTitle {
            color: #edf0f4;
            font-size: 19px;
            font-weight: bold;
            background: transparent;
        }
        QFrame#setupEntityPane {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 10px;
        }
        QLabel#setupEntityPaneTitle {
            color: #dce1e8;
            font-size: 14px;
            font-weight: bold;
            background: transparent;
        }
        QComboBox#setupEntityFilter {
            background-color: #1b1f25;
            color: #dce4f0;
            border: 1px solid #414b5a;
            border-radius: 7px;
            padding: 7px 32px 7px 11px;
            min-height: 22px;
            font-size: 12px;
        }
        QComboBox#setupEntityFilter:hover,
        QComboBox#setupEntityFilter:focus {
            border-color: #7083b3;
        }
        QComboBox#setupEntityFilter::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border: none;
        }
        QComboBox#setupEntityFilter::down-arrow {
            image: url(:/icons/chevron-down.xpm);
            width: 12px;
            height: 8px;
        }
        QListView#setupEntityFilterPopup {
            background-color: #252a32;
            color: #edf0f4;
            border: 1px solid #566179;
            outline: none;
            selection-background-color: #394567;
        }
        QListView#setupEntityFilterPopup::item { padding: 8px 10px; }
        QLabel#setupEntityEmptyText {
            color: #8793a5;
            font-size: 13px;
            background: transparent;
        }
        QFrame#setupEntityDetailCard {
            background-color: #1b1f25;
            border: 1px solid #363d48;
            border-radius: 9px;
        }
        QLabel#setupEntityDetailName {
            color: #edf0f4;
            font-size: 20px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#setupEntityDetailKind {
            color: #cbd5ff;
            background-color: #303958;
            border: 1px solid #4b5887;
            border-radius: 6px;
            padding: 4px 10px;
            font-size: 12px;
        }
        QLabel#setupEntityDetailHeading {
            color: #9daac0;
            font-size: 12px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#setupEntityDetailText {
            color: #d3dce9;
            font-size: 13px;
            background: transparent;
        }
        QPushButton#setupNewEntityButton, QPushButton#setupEditEntityButton,
        QPushButton#setupViewInGraphButton {
            background-color: #292f3c;
            color: #cbd5ee;
            border: 1px solid #46526d;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }
        QPushButton#setupNewEntityButton:hover, QPushButton#setupEditEntityButton:hover,
        QPushButton#setupViewInGraphButton:hover {
            background-color: #35405b;
            border-color: #6679a7;
        }
        QPushButton#setupViewInGraphButton:disabled {
            background-color: #292e39;
            color: #7f8ba2;
            border-color: #3b4354;
        }
        QPushButton#setupRemoveEntityButton {
            background-color: #30282d;
            color: #eab0b9;
            border: 1px solid #67444c;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }
        QPushButton#setupRemoveEntityButton:hover {
            background-color: #463038;
            border-color: #b76574;
        }
        QListWidget#setupEntityList {
            background: transparent;
            color: #dce1e8;
            border: none;
            outline: none;
        }
        QListWidget#setupEntityList::item {
            border-radius: 6px;
            padding: 10px 8px;
            margin: 2px 0;
        }
        QListWidget#setupEntityList::item:hover { background-color: #2b3038; }
        QListWidget#setupEntityList::item:selected {
            background-color: #303958;
            color: #dce4ff;
        }
        QSplitter#setupMainSplitter::handle:horizontal,
        QSplitter#setupEntitySplitter::handle:horizontal {
            background-color: #1c1e22;
            width: 10px;
        }
        QSplitter#setupMainSplitter::handle:horizontal:hover,
        QSplitter#setupEntitySplitter::handle:horizontal:hover {
            background-color: #303744;
        }
        QFrame#setupCard QLabel#setupFieldLabel {
            color: #dce1e8;
            font-size: 13px;
            font-weight: bold;
            background: transparent;
        }
        QPlainTextEdit#setupTextEdit, QComboBox#setupCombo {
            background-color: #191c20;
            color: #edf0f4;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 9px 11px;
            font-size: 13px;
            selection-background-color: #394567;
        }
        QPlainTextEdit#setupTextEdit:focus, QComboBox#setupCombo:focus {
            border-color: #7890cd;
        }
        QComboBox#setupCombo::drop-down {
            border: none;
            width: 34px;
        }
        QComboBox#setupCombo::down-arrow {
            image: url(:/icons/chevron-down.xpm);
            width: 12px;
            height: 8px;
        }
        QComboBox#setupCombo QAbstractItemView {
            background-color: #252a32;
            color: #edf0f4;
            border: 1px solid #566179;
            selection-background-color: #394567;
            outline: none;
        }
        QPushButton#setupSaveButton {
            background-color: #33416b;
            color: #e2e8ff;
            border: 1px solid #566ba1;
            border-radius: 7px;
            padding: 9px 18px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#setupSaveButton:hover {
            background-color: #415284;
            border-color: #7a90c7;
        }
        QPushButton#setupSaveButton:disabled {
            background-color: #292e39;
            color: #7f8ba2;
            border-color: #3b4354;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 2px 0;
        }
        QScrollBar::handle:vertical {
            background-color: #515b6c;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background-color: #73809a; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )QSS");
}
