//
// Created by shijie on 2026/10/3.
//

#include "Setter.h"
#include "ModelDebugDialog.h"
#include "../../storage/AppSettings.h"
#include "../../storage/GlobalWritingConventionStore.h"
#include "../../storage/StoragePaths.h"
#include "../../storage/DataRootStore.h"
#include "../../storage/InlineSuggestionSettings.h"
#include "../../storage/InlinePromptSettingsStore.h"
#include "../../storage/CloudProfileStore.h"
#include "../../storage/CloudCredentialStore.h"
#include "../../storage/ActiveModelServiceStore.h"
#include "../../storage/ModelServiceIdentity.h"
#include "../Generation/InlinePrompt.h"
#include "../Generation/CloudChatApi.h"
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QFrame>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSslSocket>
#include <QStackedWidget>
#include <QStyle>
#include <QUrl>
#include <QUuid>
#include <algorithm>
namespace {
QSettings authorSettings() {
    return AppSettings::open();
}

void addModelCandidate(QComboBox *combo, const QString &name) {
    combo->addItem(name);
    combo->setItemData(combo->count() - 1, QSize(0, 38), Qt::SizeHintRole);
}

QPlainTextEdit *conventionEdit(const QString &placeholder, QWidget *parent) {
    auto *edit = new QPlainTextEdit(parent);
    edit->setObjectName("settingsConventionEdit");
    edit->setPlaceholderText(placeholder);
    edit->setMinimumHeight(110);
    return edit;
}

QComboBox *conventionCombo(const QStringList &choices, QWidget *parent) {
    auto *combo = new QComboBox(parent);
    combo->setObjectName("settingsConventionCombo");
    combo->addItems(choices);
    combo->setEditable(true);
    return combo;
}

QScrollArea *scrollablePage(QWidget *content, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("settingsScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    return scroll;
}

QScrollArea *plannedPage(const QString &title, const QString &description,
                         QWidget *parent) {
    auto *content = new QWidget;
    content->setObjectName("settingsContent");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 12, 0);
    layout->setSpacing(16);

    auto *card = new QFrame(content);
    card->setObjectName("settingsCard");
    card->setMaximumWidth(760);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 26, 28, 28);
    cardLayout->setSpacing(12);

    auto *badge = new QLabel("规划中", card);
    badge->setObjectName("settingsBadge");
    auto *heading = new QLabel(title, card);
    heading->setObjectName("settingsSectionTitle");
    auto *details = new QLabel(description, card);
    details->setObjectName("settingsBodyText");
    details->setWordWrap(true);
    cardLayout->addWidget(badge, 0, Qt::AlignLeft);
    cardLayout->addWidget(heading);
    cardLayout->addWidget(details);

    layout->addWidget(card, 0, Qt::AlignTop);
    layout->addStretch();
    return scrollablePage(content, parent);
}
}

Setter::Setter(QWidget *parent) : QWidget(parent) {
    setObjectName("settingsPage");
    setAttribute(Qt::WA_StyledBackground, true);

    QSettings settings = authorSettings();
    savedAuthorName_ = settings.value("author/defaultName").toString();
    const bool settingsReadOk = settings.status() == QSettings::NoError;
    QString conventionError;
    const bool conventionReadOk = GlobalWritingConventionStore::load(
        savedConvention_, conventionError);
    QString suggestionSettingsError;
    InlineSuggestionSettings::load(inlineSuggestionEnabled_, suggestionSettingsError);
    QString modelSettingsError;
    const bool modelSettingsReadOk = ModelSettingsStore::load(
        savedModelSettings_, modelSettingsError);
    QString promptSettingsError;
    const bool promptSettingsReadOk = InlinePromptSettingsStore::load(
        savedSystemRules_, promptSettingsError);
    if (!promptSettingsReadOk)
        savedSystemRules_ = InlinePrompt::defaultSystemRules();
    QString cloudSettingsError;
    const bool cloudSettingsReadOk = CloudProfileStore::load(
        cloudProfiles_, activeCloudProfileId_, cloudSettingsError);
    QString serviceSettingsError;
    const bool serviceSettingsReadOk = ActiveModelServiceStore::load(
        activeModelService_, serviceSettingsError);
    modelNetwork_ = new QNetworkAccessManager(this);
    avatarPath_ = QFileInfo(settings.fileName()).dir().filePath(
        StoragePaths::AuthorAvatarFileName);
    if (QFileInfo::exists(avatarPath_)) {
        QImageReader reader(avatarPath_);
        reader.setAutoTransform(true);
        savedAvatarImage_ = reader.read();
    }

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 26, 28, 24);
    root->setSpacing(22);

    auto *pageTitle = new QLabel("设置", this);
    pageTitle->setObjectName("settingsTitle");
    auto *pageDescription = new QLabel("管理作者资料和写作工作台的偏好。", this);
    pageDescription->setObjectName("settingsDescription");
    root->addWidget(pageTitle);
    root->addWidget(pageDescription);

    auto *body = new QHBoxLayout;
    body->setSpacing(18);
    root->addLayout(body, 1);

    auto *nav = new QFrame(this);
    nav->setObjectName("settingsNav");
    nav->setFixedWidth(190);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(12, 16, 12, 14);
    navLayout->setSpacing(10);
    auto *navTitle = new QLabel("设置分类", nav);
    navTitle->setObjectName("settingsNavTitle");
    categoryList_ = new QListWidget(nav);
    categoryList_->setObjectName("settingsCategoryList");
    categoryList_->addItems({"作者资料", "写作与外观", "文件与数据", "快捷键", "模型与提示"});
    categoryList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categoryList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *navNote = new QLabel("设置保存在这台设备上", nav);
    navNote->setObjectName("settingsNavNote");
    navNote->setWordWrap(true);
    navLayout->addWidget(navTitle);
    navLayout->addWidget(categoryList_, 1);
    navLayout->addWidget(navNote);
    body->addWidget(nav);

    auto *sections = new QStackedWidget(this);
    sections->setObjectName("settingsSections");
    body->addWidget(sections, 1);

    auto *authorContent = new QWidget;
    authorContent->setObjectName("settingsContent");
    auto *authorLayout = new QVBoxLayout(authorContent);
    authorLayout->setContentsMargins(0, 0, 12, 0);
    authorLayout->setSpacing(16);

    auto *authorCard = new QFrame(authorContent);
    authorCard->setObjectName("settingsCard");
    authorCard->setMaximumWidth(760);
    auto *cardLayout = new QVBoxLayout(authorCard);
    cardLayout->setContentsMargins(28, 26, 28, 28);
    cardLayout->setSpacing(16);

    auto *sectionTitle = new QLabel("作者资料", authorCard);
    sectionTitle->setObjectName("settingsSectionTitle");
    auto *sectionDescription = new QLabel("设置在这台设备上使用的默认作者名。", authorCard);
    sectionDescription->setObjectName("settingsBodyText");
    sectionDescription->setWordWrap(true);
    cardLayout->addWidget(sectionTitle);
    cardLayout->addWidget(sectionDescription);

    auto *previewRow = new QHBoxLayout;
    previewRow->setSpacing(14);
    avatarPreview_ = new QLabel(authorCard);
    avatarPreview_->setObjectName("settingsAvatarPreview");
    avatarPreview_->setFixedSize(58, 58);
    avatarPreview_->setAlignment(Qt::AlignCenter);
    avatarPreview_->setAccessibleName("作者头像预览");
    auto *previewTexts = new QVBoxLayout;
    previewTexts->setSpacing(4);
    authorPreviewName_ = new QLabel("未设置作者", authorCard);
    authorPreviewName_->setObjectName("settingsAuthorName");
    auto *previewNote = new QLabel("头像仅保存在这台设备上", authorCard);
    previewNote->setObjectName("settingsMutedText");
    previewTexts->addWidget(authorPreviewName_);
    previewTexts->addWidget(previewNote);
    previewRow->addWidget(avatarPreview_);
    previewRow->addLayout(previewTexts);
    previewRow->addStretch();
    cardLayout->addLayout(previewRow);

    auto *avatarActions = new QHBoxLayout;
    avatarActions->setSpacing(10);
    auto *chooseAvatarButton = new QPushButton("更换图片", authorCard);
    chooseAvatarButton->setObjectName("settingsAvatarAction");
    chooseAvatarButton->setCursor(Qt::PointingHandCursor);
    resetAvatarButton_ = new QPushButton("恢复默认", authorCard);
    resetAvatarButton_->setObjectName("settingsAvatarAction");
    resetAvatarButton_->setCursor(Qt::PointingHandCursor);
    avatarActions->addWidget(chooseAvatarButton);
    avatarActions->addWidget(resetAvatarButton_);
    avatarActions->addStretch();
    cardLayout->addLayout(avatarActions);

    auto *divider = new QFrame(authorCard);
    divider->setObjectName("settingsDivider");
    divider->setFrameShape(QFrame::HLine);
    cardLayout->addWidget(divider);

    auto *fieldLabel = new QLabel("默认作者名", authorCard);
    fieldLabel->setObjectName("settingsFieldLabel");
    authorNameEdit_ = new QLineEdit(authorCard);
    authorNameEdit_->setObjectName("settingsAuthorEdit");
    authorNameEdit_->setPlaceholderText("输入你的名字或笔名");
    authorNameEdit_->setMaxLength(80);
    authorNameEdit_->setText(savedAuthorName_);
    auto *fieldNote = new QLabel("已有作品的署名可以在作品库的“编辑作品信息”中修改。", authorCard);
    fieldNote->setObjectName("settingsMutedText");
    fieldNote->setWordWrap(true);
    cardLayout->addWidget(fieldLabel);
    cardLayout->addWidget(authorNameEdit_);
    cardLayout->addWidget(fieldNote);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(12);
    saveStatus_ = new QLabel(authorCard);
    saveStatus_->setObjectName("settingsSaveStatus");
    saveButton_ = new QPushButton("保存设置", authorCard);
    saveButton_->setObjectName("settingsSaveButton");
    saveButton_->setCursor(Qt::PointingHandCursor);
    saveButton_->setEnabled(false);
    actions->addWidget(saveStatus_);
    actions->addStretch();
    actions->addWidget(saveButton_);
    cardLayout->addLayout(actions);
    authorLayout->addWidget(authorCard);

    auto *noteCard = new QFrame(authorContent);
    noteCard->setObjectName("settingsNoteCard");
    noteCard->setMaximumWidth(760);
    auto *noteLayout = new QVBoxLayout(noteCard);
    noteLayout->setContentsMargins(20, 17, 20, 18);
    noteLayout->setSpacing(6);
    auto *noteTitle = new QLabel("关于作品署名", noteCard);
    noteTitle->setObjectName("settingsNoteTitle");
    auto *noteText = new QLabel("每部作品可以使用不同的作者名；在作品库中编辑作品信息即可修改。", noteCard);
    noteText->setObjectName("settingsBodyText");
    noteText->setWordWrap(true);
    noteLayout->addWidget(noteTitle);
    noteLayout->addWidget(noteText);
    authorLayout->addWidget(noteCard);
    authorLayout->addStretch();
    sections->addWidget(scrollablePage(authorContent, sections));

    auto *writingContent = new QWidget;
    writingContent->setObjectName("settingsContent");
    auto *writingLayout = new QVBoxLayout(writingContent);
    writingLayout->setContentsMargins(0, 0, 12, 0);
    writingLayout->setSpacing(16);
    auto *writingCard = new QFrame(writingContent);
    writingCard->setObjectName("settingsCard");
    auto *writingFields = new QVBoxLayout(writingCard);
    writingFields->setContentsMargins(28, 26, 28, 28);
    writingFields->setSpacing(12);
    auto *writingTitle = new QLabel("全局写作约定", writingCard);
    writingTitle->setObjectName("settingsSectionTitle");
    auto *writingHint = new QLabel(
        "作品的写作约定全部留空时，才会使用这里的整组约定。作品只要填写任意一项，就使用作品自己的整组约定。",
        writingCard);
    writingHint->setObjectName("settingsBodyText");
    writingHint->setWordWrap(true);
    writingFields->addWidget(writingTitle);
    writingFields->addWidget(writingHint);

    globalViewpointCombo_ = conventionCombo(
        {"未指定", "第一人称", "第三人称限知", "第三人称全知", "多视角"}, writingCard);
    globalTenseCombo_ = conventionCombo(
        {"未指定", "过去时", "现在时", "交替使用"}, writingCard);
    auto *shortFields = new QFormLayout;
    shortFields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    shortFields->setHorizontalSpacing(20);
    shortFields->setVerticalSpacing(14);
    auto *viewpointLabel = new QLabel("叙事视角", writingCard);
    viewpointLabel->setObjectName("settingsFieldLabel");
    auto *tenseLabel = new QLabel("叙事时态", writingCard);
    tenseLabel->setObjectName("settingsFieldLabel");
    shortFields->addRow(viewpointLabel, globalViewpointCombo_);
    shortFields->addRow(tenseLabel, globalTenseCombo_);
    writingFields->addLayout(shortFields);

    auto addLongField = [writingCard, writingFields](const QString &title,
                                                       QPlainTextEdit *edit) {
        auto *label = new QLabel(title, writingCard);
        label->setObjectName("settingsFieldLabel");
        writingFields->addWidget(label);
        writingFields->addWidget(edit);
    };
    globalStyleEdit_ = conventionEdit("例如：简洁、克制，以动作呈现情绪…", writingCard);
    globalRhythmEdit_ = conventionEdit("例如：短句为主；紧张场景加快节奏…", writingCard);
    globalConstraintsEdit_ = conventionEdit("例如：避免提前揭示秘密…", writingCard);
    addLongField("语言风格", globalStyleEdit_);
    addLongField("遣词与节奏", globalRhythmEdit_);
    addLongField("必须遵守或避免的事项", globalConstraintsEdit_);

    auto *writingActions = new QHBoxLayout;
    globalSaveStatus_ = new QLabel(writingCard);
    globalSaveStatus_->setObjectName("settingsSaveStatus");
    globalSaveButton_ = new QPushButton("保存全局约定", writingCard);
    globalSaveButton_->setObjectName("settingsSaveButton");
    globalSaveButton_->setEnabled(false);
    writingActions->addWidget(globalSaveStatus_);
    writingActions->addStretch();
    writingActions->addWidget(globalSaveButton_);
    writingFields->addLayout(writingActions);
    writingLayout->addWidget(writingCard);

    auto *suggestionCard = new QFrame(writingContent);
    suggestionCard->setObjectName("settingsCard");
    auto *suggestionLayout = new QVBoxLayout(suggestionCard);
    suggestionLayout->setContentsMargins(28, 24, 28, 24);
    suggestionLayout->setSpacing(12);
    auto *suggestionTitle = new QLabel("短语补全", suggestionCard);
    suggestionTitle->setObjectName("settingsSectionTitle");
    inlineSuggestionCheck_ = new QCheckBox("自动显示短语建议", suggestionCard);
    inlineSuggestionCheck_->setObjectName("settingsInlineSuggestionCheck");
    inlineSuggestionCheck_->setChecked(inlineSuggestionEnabled_);
    auto *suggestionHint = new QLabel(
        "停止输入约 1 秒后，使用“模型与提示”中选定的本地模型生成短语建议。Tab 接受，Esc 忽略。",
        suggestionCard);
    suggestionHint->setObjectName("settingsBodyText");
    suggestionHint->setWordWrap(true);
    suggestionLayout->addWidget(suggestionTitle);
    suggestionLayout->addWidget(inlineSuggestionCheck_);
    suggestionLayout->addWidget(suggestionHint);
    writingLayout->addWidget(suggestionCard);
    writingLayout->addStretch();
    sections->addWidget(scrollablePage(writingContent, sections));

    // 文件与数据：先展示当前作品库位置，目录搬迁在后续实现。
    QString dataRoot;
    QString pathError;
    const bool pathOk = DataRootStore::load(dataRoot, pathError);

    auto *fileContent = new QWidget;
    fileContent->setObjectName("settingsContent");
    auto *fileLayout = new QVBoxLayout(fileContent);
    fileLayout->setContentsMargins(0, 0, 12, 0);
    fileLayout->setSpacing(16);

    auto *fileCard = new QFrame(fileContent);
    fileCard->setObjectName("settingsCard");
    fileCard->setMaximumWidth(760);
    auto *fileCardLayout = new QVBoxLayout(fileCard);
    fileCardLayout->setContentsMargins(28, 26, 28, 28);
    fileCardLayout->setSpacing(14);

    auto *fileTitle = new QLabel("作品库位置", fileCard);
    fileTitle->setObjectName("settingsSectionTitle");
    auto *fileDescription = new QLabel("这里存放作品文件与归档作品。", fileCard);
    fileDescription->setObjectName("settingsBodyText");
    fileDescription->setWordWrap(true);
    auto *pathCaption = new QLabel("当前数据根目录", fileCard);
    pathCaption->setObjectName("settingsFieldLabel");
    auto *pathLabel = new QLabel(fileCard);
    pathLabel->setObjectName(pathOk ? "settingsDataPath" : "settingsDataPathError");
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setText(pathOk ? QDir::toNativeSeparators(dataRoot) : pathError);

    auto *openFolderButton = new QPushButton("打开文件夹", fileCard);
    openFolderButton->setObjectName("settingsAvatarAction");
    openFolderButton->setCursor(Qt::PointingHandCursor);
    openFolderButton->setEnabled(pathOk && QDir(dataRoot).exists());
    connect(openFolderButton, &QPushButton::clicked, this, [this, dataRoot] {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dataRoot)))
            QMessageBox::warning(this, "打开失败", "无法打开作品库目录。");
    });

    auto *changeFolderButton = new QPushButton("更换位置…", fileCard);
    changeFolderButton->setObjectName("settingsSaveButton");
    changeFolderButton->setCursor(Qt::PointingHandCursor);
    changeFolderButton->setEnabled(pathOk);
    connect(changeFolderButton, &QPushButton::clicked, this, [this, dataRoot] {
        if (saveButton_->isEnabled() || globalSaveButton_->isEnabled()) {
            QMessageBox::warning(this, "设置尚未保存",
                                 "请先保存作者资料和全局写作约定，再更换作品库位置。");
            return;
        }
        const QString target = QFileDialog::getExistingDirectory(
            this, "选择空的作品库目录", QFileInfo(dataRoot).dir().absolutePath(),
            QFileDialog::ShowDirsOnly);
        if (!target.isEmpty()) emit dataRootChangeRequested(target);
    });

    auto *folderActions = new QHBoxLayout;
    folderActions->setSpacing(10);
    folderActions->addWidget(openFolderButton);
    folderActions->addWidget(changeFolderButton);
    folderActions->addStretch();

    auto *fileNote = new QLabel(
        "选择空目录后，程序会复制并核对 projects 和 archive；完成后退出，重新打开即可使用新目录。旧目录会保留。",
        fileCard);
    fileNote->setObjectName("settingsMutedText");
    fileNote->setWordWrap(true);

    fileCardLayout->addWidget(fileTitle);
    fileCardLayout->addWidget(fileDescription);
    fileCardLayout->addSpacing(4);
    fileCardLayout->addWidget(pathCaption);
    fileCardLayout->addWidget(pathLabel);
    fileCardLayout->addLayout(folderActions);
    fileCardLayout->addSpacing(8);
    fileCardLayout->addWidget(fileNote);
    fileLayout->addWidget(fileCard);
    fileLayout->addStretch();
    sections->addWidget(scrollablePage(fileContent, sections));
    sections->addWidget(plannedPage("快捷键",
        "当前可用 Ctrl + S 保存章节，使用 Ctrl + 滚轮调整正文字号。快捷键设置会在这里逐步完善。", sections));

    auto *modelContent = new QWidget;
    modelContent->setObjectName("settingsContent");
    auto *modelLayout = new QVBoxLayout(modelContent);
    modelLayout->setContentsMargins(0, 0, 12, 0);
    modelLayout->setSpacing(16);

    auto *sourceCard = new QFrame(modelContent);
    sourceCard->setObjectName("settingsCard");
    sourceCard->setMaximumWidth(760);
    auto *sourceLayout = new QVBoxLayout(sourceCard);
    sourceLayout->setContentsMargins(24, 22, 24, 22);
    sourceLayout->setSpacing(12);
    auto *sourceTitle = new QLabel("选择模型来源", sourceCard);
    sourceTitle->setObjectName("settingsFieldLabel");
    cloudSourceButton_ = new QPushButton("云端服务\n联网使用服务商模型", sourceCard);
    cloudSourceButton_->setObjectName("settingsSourceChoice");
    ollamaSourceButton_ = new QPushButton("本地服务 · Ollama\n连接本机推理服务", sourceCard);
    ollamaSourceButton_->setObjectName("settingsSourceChoice");
    for (QPushButton *button : {cloudSourceButton_, ollamaSourceButton_}) {
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(72);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    auto *sourceChoices = new QHBoxLayout;
    sourceChoices->setSpacing(10);
    sourceChoices->addWidget(cloudSourceButton_, 1);
    sourceChoices->addWidget(ollamaSourceButton_, 1);
    activeServiceStatus_ = new QLabel(sourceCard);
    activeServiceStatus_->setObjectName("settingsMutedText");
    auto *debugButton = new QPushButton("查看请求记录", sourceCard);
    debugButton->setObjectName("settingsAvatarAction");
    auto *sourceFooter = new QHBoxLayout;
    sourceFooter->addWidget(activeServiceStatus_);
    sourceFooter->addStretch();
    sourceFooter->addWidget(debugButton);
    sourceLayout->addWidget(sourceTitle);
    sourceLayout->addLayout(sourceChoices);
    sourceLayout->addLayout(sourceFooter);
    modelLayout->addWidget(sourceCard);
    modelSourceStack_ = new QStackedWidget(modelContent);
    modelSourceStack_->setObjectName("settingsModelSourceStack");

    auto *modelCard = new QFrame(modelContent);
    modelCard->setObjectName("settingsCard");
    modelCard->setMaximumWidth(760);
    auto *modelFields = new QVBoxLayout(modelCard);
    modelFields->setContentsMargins(28, 26, 28, 28);
    modelFields->setSpacing(14);
    auto *modelTitle = new QLabel("模型服务 · Ollama", modelCard);
    modelTitle->setObjectName("settingsSectionTitle");
    auto *modelIntro = new QLabel("连接 Ollama、读取模型并测试；设置完成后可设为写作页使用的服务。",
                                  modelCard);
    modelIntro->setObjectName("settingsBodyText");
    modelIntro->setWordWrap(true);
    modelFields->addWidget(modelTitle);
    modelFields->addWidget(modelIntro);

    auto *useOllamaButton = new QPushButton("设为写作模型", modelCard);
    useOllamaButton->setObjectName("settingsSaveButton");
    useCloudButton_ = new QPushButton("使用选中的云端配置", modelContent);
    useCloudButton_->setObjectName("settingsSaveButton");

    auto *addressLabel = new QLabel("Ollama 服务地址", modelCard);
    addressLabel->setObjectName("settingsFieldLabel");
    modelUrlEdit_ = new QLineEdit(modelCard);
    modelUrlEdit_->setObjectName("settingsModelEdit");
    modelUrlEdit_->setPlaceholderText("http://127.0.0.1:11434");
    modelUrlEdit_->setText(savedModelSettings_.baseUrl);
    modelFields->addWidget(addressLabel);
    modelFields->addWidget(modelUrlEdit_);

    auto *modelNameLabel = new QLabel("当前模型", modelCard);
    modelNameLabel->setObjectName("settingsFieldLabel");
    modelCombo_ = new QComboBox(modelCard);
    modelCombo_->setObjectName("settingsModelCombo");
    modelCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont modelPopupFont = modelCombo_->view()->font();
    modelPopupFont.setPixelSize(16);
    modelCombo_->view()->setFont(modelPopupFont);
    for (const QString &name : savedModelSettings_.knownModels)
        addModelCandidate(modelCombo_, name);
    modelCombo_->setCurrentText(savedModelSettings_.modelName);
    modelFields->addWidget(modelNameLabel);
    auto *selectedModelRow = new QHBoxLayout;
    selectedModelRow->setSpacing(10);
    modelRemoveButton_ = new QPushButton("移除当前候选", modelCard);
    modelRemoveButton_->setObjectName("settingsAvatarAction");
    modelRemoveButton_->setEnabled(modelCombo_->count() > 1);
    selectedModelRow->addWidget(modelCombo_, 1);
    selectedModelRow->addWidget(modelRemoveButton_);
    modelFields->addLayout(selectedModelRow);
    auto *modelRoutingNote = new QLabel(modelCard);
    modelRoutingNote->setObjectName("settingsMutedText");
    modelRoutingNote->setWordWrap(true);
    const auto updateRoutingNote = [this, modelRoutingNote] {
        modelRoutingNote->setText(
            ModelSettingsStore::isOllamaCloudModel(modelCombo_->currentText())
            ? "当前是 Ollama 云端模型：作品设定和最近正文会经 Ollama 发送到云端。"
            : "当前是 Ollama 本地模型：请求发往上方填写的服务地址。");
    };
    updateRoutingNote();
    connect(modelCombo_, &QComboBox::currentTextChanged,
            this, updateRoutingNote);
    modelFields->addWidget(modelRoutingNote);

    auto *addLabel = new QLabel("添加已安装的模型", modelCard);
    addLabel->setObjectName("settingsFieldLabel");
    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(10);
    modelNameInput_ = new QLineEdit(modelCard);
    modelNameInput_->setObjectName("settingsModelEdit");
    modelNameInput_->setPlaceholderText("输入模型名称，例如 qwen3:8b");
    modelNameInput_->setMaxLength(120);
    auto *addModelButton = new QPushButton("加入候选", modelCard);
    addModelButton->setObjectName("settingsAvatarAction");
    addRow->addWidget(modelNameInput_, 1);
    addRow->addWidget(addModelButton);
    auto *addNote = new QLabel(
        "加入候选不会下载模型；本地模型需先在 Ollama 安装。名称以 :cloud 或 -cloud 结尾的模型由 Ollama 云端运行。",
        modelCard);
    addNote->setObjectName("settingsMutedText");
    addNote->setWordWrap(true);
    modelFields->addWidget(addLabel);
    modelFields->addLayout(addRow);
    modelFields->addWidget(addNote);

    auto *serviceActions = new QHBoxLayout;
    serviceActions->setSpacing(10);
    modelRefreshButton_ = new QPushButton("读取 Ollama 模型", modelCard);
    modelRefreshButton_->setObjectName("settingsAvatarAction");
    modelTestButton_ = new QPushButton("测试当前模型", modelCard);
    modelTestButton_->setObjectName("settingsAvatarAction");
    serviceActions->addWidget(modelRefreshButton_);
    serviceActions->addWidget(modelTestButton_);
    serviceActions->addStretch();
    modelConnectionStatus_ = new QLabel("尚未检测连接", modelCard);
    modelConnectionStatus_->setObjectName("settingsModelStatus");
    modelConnectionStatus_->setWordWrap(true);
    modelFields->addLayout(serviceActions);
    modelFields->addWidget(modelConnectionStatus_);

    auto *modelSaveRow = new QHBoxLayout;
    modelSaveStatus_ = new QLabel(modelCard);
    modelSaveStatus_->setObjectName("settingsSaveStatus");
    modelSaveButton_ = new QPushButton("保存模型设置", modelCard);
    modelSaveButton_->setObjectName("settingsSaveButton");
    modelSaveButton_->setEnabled(false);
    modelSaveRow->addWidget(modelSaveStatus_);
    modelSaveRow->addStretch();
    modelSaveRow->addWidget(modelSaveButton_);
    modelSaveRow->addWidget(useOllamaButton);
    modelFields->addLayout(modelSaveRow);
    modelSourceStack_->addWidget(modelCard);

    auto *cloudCard = new QFrame(modelContent);
    cloudCard->setObjectName("settingsCard");
    cloudCard->setMaximumWidth(760);
    auto *cloudLayout = new QVBoxLayout(cloudCard);
    cloudLayout->setContentsMargins(28, 26, 28, 28);
    cloudLayout->setSpacing(14);
    auto *cloudTitle = new QLabel("云端服务", cloudCard);
    cloudTitle->setObjectName("settingsSectionTitle");
    auto *cloudNote = new QLabel(
        "选择已保存配置即可查看和修改。新建时先选服务商；API Key 保存在 Windows 凭据管理器。",
        cloudCard);
    cloudNote->setObjectName("settingsBodyText");
    cloudNote->setWordWrap(true);
    cloudLayout->addWidget(cloudTitle);
    cloudLayout->addWidget(cloudNote);

    auto *cloudProfileLabel = new QLabel("已保存配置 · 选择后查看与修改", cloudCard);
    cloudProfileLabel->setObjectName("settingsFieldLabel");
    cloudProfileCombo_ = new QComboBox(cloudCard);
    cloudProfileCombo_->setObjectName("settingsModelCombo");
    cloudProfileCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cloudProfileCombo_->setPlaceholderText("新配置 · 尚未保存");
    QFont cloudPopupFont = cloudProfileCombo_->view()->font();
    cloudPopupFont.setPixelSize(16);
    cloudProfileCombo_->view()->setFont(cloudPopupFont);
    auto *profileRow = new QHBoxLayout;
    profileRow->setSpacing(10);
    auto *cloudNewButton = new QPushButton("新建", cloudCard);
    cloudNewButton->setObjectName("settingsAvatarAction");
    cloudDeleteButton_ = new QPushButton("删除", cloudCard);
    cloudDeleteButton_->setObjectName("settingsAvatarAction");
    profileRow->addWidget(cloudProfileCombo_, 1);
    profileRow->addWidget(cloudNewButton);
    profileRow->addWidget(cloudDeleteButton_);
    cloudLayout->addWidget(cloudProfileLabel);
    cloudLayout->addLayout(profileRow);
    auto *cloudDivider = new QFrame(cloudCard);
    cloudDivider->setObjectName("settingsDivider");
    cloudDivider->setFrameShape(QFrame::HLine);
    cloudLayout->addSpacing(4);
    cloudLayout->addWidget(cloudDivider);

    auto *providerLabel = new QLabel("服务商", cloudCard);
    providerLabel->setObjectName("settingsFieldLabel");
    cloudLayout->addWidget(providerLabel);

    cloudPresetCombo_ = new QComboBox(cloudCard);
    cloudPresetCombo_->setObjectName("settingsModelCombo");
    cloudPresetCombo_->addItem("DeepSeek", "deepseek");
    cloudPresetCombo_->addItem("阿里云百炼（通用地址）", "dashscope");
    cloudPresetCombo_->addItem("OpenAI", "openai");
    cloudPresetCombo_->addItem("自定义兼容接口", "custom");
    cloudPresetCombo_->view()->setFont(cloudPopupFont);
    for (int i = 0; i < cloudPresetCombo_->count(); ++i)
        cloudPresetCombo_->setItemData(i, QSize(0, 38), Qt::SizeHintRole);
    cloudLayout->addWidget(cloudPresetCombo_);

    auto addCloudField = [cloudCard, cloudLayout](const QString &labelText,
                                                  const QString &placeholder) {
        auto *label = new QLabel(labelText, cloudCard);
        label->setObjectName("settingsFieldLabel");
        auto *edit = new QLineEdit(cloudCard);
        edit->setObjectName("settingsModelEdit");
        edit->setPlaceholderText(placeholder);
        cloudLayout->addWidget(label);
        cloudLayout->addWidget(edit);
        return edit;
    };
    cloudNameEdit_ = addCloudField("配置名称", "例如：DeepSeek 写作模型");
    cloudUrlEdit_ = addCloudField("接口地址", "例如：https://api.deepseek.com");
    cloudModelEdit_ = addCloudField("模型", "例如：deepseek-flash");
    cloudKeyEdit_ = addCloudField("API 密钥", "输入新密钥；留空则使用已保存密钥");
    cloudKeyEdit_->setEchoMode(QLineEdit::Password);
    cloudKeyStatus_ = new QLabel(cloudCard);
    cloudKeyStatus_->setObjectName("settingsMutedText");
    cloudLayout->addWidget(cloudKeyStatus_);
    auto *cloudActions = new QHBoxLayout;
    cloudActions->setSpacing(10);
    cloudTestButton_ = new QPushButton("测试连接", cloudCard);
    cloudTestButton_->setObjectName("settingsAvatarAction");
    cloudSaveButton_ = new QPushButton("保存配置", cloudCard);
    cloudSaveButton_->setObjectName("settingsSaveButton");
    cloudActions->addWidget(cloudTestButton_);
    cloudActions->addStretch();
    cloudActions->addWidget(cloudSaveButton_);
    cloudActions->addWidget(useCloudButton_);
    cloudStatus_ = new QLabel(cloudCard);
    cloudStatus_->setObjectName("settingsModelStatus");
    cloudStatus_->setWordWrap(true);
    cloudLayout->addLayout(cloudActions);
    cloudLayout->addWidget(cloudStatus_);
    modelSourceStack_->addWidget(cloudCard);
    modelLayout->addWidget(modelSourceStack_);

    auto *promptCard = new QFrame(modelContent);
    promptCard->setObjectName("settingsCard");
    promptCard->setMaximumWidth(760);
    auto *promptLayout = new QVBoxLayout(promptCard);
    promptLayout->setContentsMargins(28, 26, 28, 28);
    promptLayout->setSpacing(14);
    auto *promptTitle = new QLabel("行内建议系统提示词", promptCard);
    promptTitle->setObjectName("settingsSectionTitle");
    auto *promptNote = new QLabel(
        "修改模型遵循的写作规则。保存后从下一次短语建议请求开始生效；"
        "本地模型的 JSON 格式要求和云端模型的纯文本格式要求由程序自动附加。",
        promptCard);
    promptNote->setObjectName("settingsBodyText");
    promptNote->setWordWrap(true);
    systemPromptEdit_ = new QPlainTextEdit(promptCard);
    systemPromptEdit_->setObjectName("settingsSystemPromptEdit");
    systemPromptEdit_->setMinimumHeight(260);
    systemPromptEdit_->setPlainText(savedSystemRules_);
    auto *promptActions = new QHBoxLayout;
    promptActions->setSpacing(10);
    systemPromptStatus_ = new QLabel(promptCard);
    systemPromptStatus_->setObjectName("settingsSaveStatus");
    systemPromptRestoreButton_ = new QPushButton("恢复默认", promptCard);
    systemPromptRestoreButton_->setObjectName("settingsAvatarAction");
    systemPromptSaveButton_ = new QPushButton("保存系统提示词", promptCard);
    systemPromptSaveButton_->setObjectName("settingsSaveButton");
    promptActions->addWidget(systemPromptStatus_);
    promptActions->addStretch();
    promptActions->addWidget(systemPromptRestoreButton_);
    promptActions->addWidget(systemPromptSaveButton_);
    promptLayout->addWidget(promptTitle);
    promptLayout->addWidget(promptNote);
    promptLayout->addWidget(systemPromptEdit_);
    promptLayout->addLayout(promptActions);
    modelLayout->addWidget(promptCard);

    auto *contextCard = new QFrame(modelContent);
    contextCard->setObjectName("settingsNoteCard");
    contextCard->setMaximumWidth(760);
    auto *contextLayout = new QVBoxLayout(contextCard);
    contextLayout->setContentsMargins(22, 19, 22, 20);
    contextLayout->setSpacing(8);
    auto *contextTitle = new QLabel("短语建议会读取哪些内容", contextCard);
    contextTitle->setObjectName("settingsNoteTitle");
    auto *contextText = new QLabel(
        "当前作品名、章节标题、核心设想、大纲、生效的写作约定，以及正文末尾约 500 字。"
        "这些内容会发送到当前启用的模型服务；使用云端服务时会经网络发送。不会自动发送整本正文。",
        contextCard);
    contextText->setObjectName("settingsBodyText");
    contextText->setWordWrap(true);
    contextLayout->addWidget(contextTitle);
    contextLayout->addWidget(contextText);
    modelLayout->addWidget(contextCard);
    modelLayout->addStretch();
    sections->addWidget(scrollablePage(modelContent, sections));

    connect(categoryList_, &QListWidget::currentRowChanged,
            sections, &QStackedWidget::setCurrentIndex);
    categoryList_->setCurrentRow(0);

    connect(authorNameEdit_, &QLineEdit::textChanged, this,
            [this](const QString &name) {
        updateAuthorPreview(name);
        const bool changed = name.trimmed() != savedAuthorName_;
        saveButton_->setEnabled(changed);
        setSaveStatus(changed ? "modified" : "saved",
                      changed ? "尚未保存" : "已保存");
    });
    connect(saveButton_, &QPushButton::clicked, this, &Setter::saveAuthorName);
    connect(chooseAvatarButton, &QPushButton::clicked, this, &Setter::chooseAvatar);
    connect(resetAvatarButton_, &QPushButton::clicked, this, &Setter::resetAvatar);
    globalViewpointCombo_->setCurrentText(savedConvention_.viewpoint.isEmpty()
                                               ? "未指定" : savedConvention_.viewpoint);
    globalTenseCombo_->setCurrentText(savedConvention_.tense.isEmpty()
                                          ? "未指定" : savedConvention_.tense);
    globalStyleEdit_->setPlainText(savedConvention_.style);
    globalRhythmEdit_->setPlainText(savedConvention_.rhythm);
    globalConstraintsEdit_->setPlainText(savedConvention_.constraints);
    connect(globalViewpointCombo_, &QComboBox::currentTextChanged,
            this, &Setter::updateGlobalSaveState);
    connect(globalTenseCombo_, &QComboBox::currentTextChanged,
            this, &Setter::updateGlobalSaveState);
    connect(globalStyleEdit_, &QPlainTextEdit::textChanged,
            this, &Setter::updateGlobalSaveState);
    connect(globalRhythmEdit_, &QPlainTextEdit::textChanged,
            this, &Setter::updateGlobalSaveState);
    connect(globalConstraintsEdit_, &QPlainTextEdit::textChanged,
            this, &Setter::updateGlobalSaveState);
    connect(globalSaveButton_, &QPushButton::clicked,
            this, &Setter::saveGlobalConvention);
    connect(inlineSuggestionCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        QString error;
        if (!InlineSuggestionSettings::save(enabled, error)) {
            const QSignalBlocker blocker(inlineSuggestionCheck_);
            inlineSuggestionCheck_->setChecked(inlineSuggestionEnabled_);
            QMessageBox::warning(this, "无法保存设置", error);
            return;
        }
        inlineSuggestionEnabled_ = enabled;
        emit inlineSuggestionEnabledChanged(enabled);
    });
    connect(modelUrlEdit_, &QLineEdit::textChanged,
            this, &Setter::updateModelSaveState);
    connect(modelCombo_, &QComboBox::currentTextChanged,
            this, &Setter::updateModelSaveState);
    connect(modelCombo_, &QComboBox::currentTextChanged, this, [this] {
        modelRemoveButton_->setEnabled(modelCombo_->count() > 1);
        setModelConnectionStatus("neutral", "当前模型尚未测试。");
    });
    connect(modelNameInput_, &QLineEdit::returnPressed,
            this, &Setter::addKnownModel);
    connect(addModelButton, &QPushButton::clicked,
            this, &Setter::addKnownModel);
    connect(modelRemoveButton_, &QPushButton::clicked,
            this, &Setter::removeSelectedModel);
    connect(modelRefreshButton_, &QPushButton::clicked, this,
            &Setter::fetchInstalledModels);
    connect(modelTestButton_, &QPushButton::clicked, this,
            &Setter::testSelectedModel);
    connect(debugButton, &QPushButton::clicked, this, [this] {
        if (!modelDebugDialog_)
            modelDebugDialog_ = new ModelDebugDialog(this);
        modelDebugDialog_->show();
        modelDebugDialog_->raise();
        modelDebugDialog_->activateWindow();
    });
    connect(modelSaveButton_, &QPushButton::clicked,
            this, &Setter::saveModelSettings);
    connect(systemPromptEdit_, &QPlainTextEdit::textChanged,
            this, &Setter::updateSystemPromptSaveState);
    connect(systemPromptSaveButton_, &QPushButton::clicked,
            this, &Setter::saveSystemPromptRules);
    connect(systemPromptRestoreButton_, &QPushButton::clicked,
            this, &Setter::restoreDefaultSystemPrompt);
    connect(useOllamaButton, &QPushButton::clicked, this, [this] {
        if (modelSaveButton_->isEnabled()) {
            setModelConnectionStatus("error", "请先保存 Ollama 模型设置，再设为写作模型。");
            return;
        }
        QString error;
        if (!ActiveModelServiceStore::save(ActiveModelService::Ollama, error)) {
            setCloudStatus("error", error);
            return;
        }
        activeModelService_ = ActiveModelService::Ollama;
        refreshActiveServiceStatus();
        setModelConnectionStatus("saved", "写作页现在使用 Ollama 模型。");
        const auto identity = ModelServiceIdentityStore::forOllama(savedModelSettings_);
        emit activeModelChanged(identity.key, identity.displayName);
    });
    connect(cloudSourceButton_, &QPushButton::clicked, this,
            [this] { showModelSource(ActiveModelService::Cloud); });
    connect(ollamaSourceButton_, &QPushButton::clicked, this,
            [this] { showModelSource(ActiveModelService::Ollama); });
    connect(useCloudButton_, &QPushButton::clicked,
            this, &Setter::activateCloudProfile);
    connect(cloudNewButton, &QPushButton::clicked,
            this, &Setter::createCloudProfile);
    connect(cloudDeleteButton_, &QPushButton::clicked,
            this, &Setter::deleteCloudProfile);
    connect(cloudSaveButton_, &QPushButton::clicked,
            this, &Setter::saveCloudProfile);
    connect(cloudTestButton_, &QPushButton::clicked,
            this, &Setter::testCloudProfile);
    connect(cloudProfileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        const QString id = cloudProfileCombo_->itemData(index).toString();
        if (id == editingCloudProfileId_) return;
        if (!confirmDiscardCloudEdits()) {
            const QSignalBlocker blocker(cloudProfileCombo_);
            cloudProfileCombo_->setCurrentIndex(
                cloudProfileCombo_->findData(editingCloudProfileId_));
            return;
        }
        loadCloudProfileForm(id);
    });
    connect(cloudPresetCombo_, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index) {
        if (editingCloudProfileId_.isEmpty()) return;
        const QString preset = cloudPresetCombo_->itemData(index).toString();
        if (preset == "deepseek") {
            cloudUrlEdit_->setText("https://api.deepseek.com");
            cloudModelEdit_->setText("deepseek-flash");
        } else if (preset == "dashscope") {
            cloudUrlEdit_->setText("https://dashscope.aliyuncs.com/compatible-mode/v1");
            cloudModelEdit_->setText("qwen-plus");
        } else if (preset == "openai") {
            cloudUrlEdit_->setText("https://api.openai.com/v1");
            cloudModelEdit_->setText("gpt-4.1-mini");
        }
    });
    for (QLineEdit *edit : {cloudNameEdit_, cloudUrlEdit_, cloudModelEdit_, cloudKeyEdit_}) {
        connect(edit, &QLineEdit::textChanged,
                this, &Setter::updateCloudSaveState);
    }

    applyStyle();
    updateAuthorPreview(savedAuthorName_);
    updateAvatarPreview();
    setSaveStatus(settingsReadOk ? "saved" : "error",
                  settingsReadOk ? "已保存" : "读取设置失败");
    globalSaveStatus_->setText(conventionReadOk ? "已保存" : conventionError);
    if (!suggestionSettingsError.isEmpty())
        inlineSuggestionCheck_->setToolTip(suggestionSettingsError);
    modelSaveStatus_->setText(modelSettingsReadOk ? "已保存" : modelSettingsError);
    modelSaveButton_->setEnabled(!modelSettingsReadOk);
    modelSaveStatus_->setProperty("state", modelSettingsReadOk ? "saved" : "error");
    modelSaveStatus_->style()->unpolish(modelSaveStatus_);
    modelSaveStatus_->style()->polish(modelSaveStatus_);
    updateSystemPromptSaveState();
    if (!promptSettingsReadOk) {
        systemPromptStatus_->setText(promptSettingsError);
        systemPromptStatus_->setProperty("state", "error");
        systemPromptStatus_->style()->unpolish(systemPromptStatus_);
        systemPromptStatus_->style()->polish(systemPromptStatus_);
        systemPromptSaveButton_->setEnabled(true);
    }
    if (cloudSettingsReadOk) {
        editingCloudProfileId_ = activeCloudProfileId_.isEmpty() && !cloudProfiles_.isEmpty()
            ? cloudProfiles_.first().id : activeCloudProfileId_;
        refreshCloudProfileCombo();
        loadCloudProfileForm(editingCloudProfileId_);
    } else {
        loadCloudProfileForm({});
        setCloudStatus("error", cloudSettingsError);
    }
    if (!serviceSettingsReadOk) setCloudStatus("error", serviceSettingsError);
    refreshActiveServiceStatus();
    showModelSource(activeModelService_);
}

ModelSettings Setter::collectModelSettings() const {
    ModelSettings settings;
    settings.baseUrl = modelUrlEdit_->text().trimmed();
    while (settings.baseUrl.endsWith('/')) settings.baseUrl.chop(1);
    settings.modelName = modelCombo_->currentText().trimmed();
    settings.knownModels.clear();
    for (int i = 0; i < modelCombo_->count(); ++i)
        settings.knownModels.append(modelCombo_->itemText(i));
    return settings;
}

void Setter::updateModelSaveState() {
    const ModelSettings current = collectModelSettings();
    const bool changed = current.baseUrl != savedModelSettings_.baseUrl ||
        current.modelName != savedModelSettings_.modelName ||
        current.knownModels != savedModelSettings_.knownModels;
    modelSaveButton_->setEnabled(changed);
    modelSaveStatus_->setText(changed ? "尚未保存" : "已保存");
    modelSaveStatus_->setProperty("state", changed ? "modified" : "saved");
    modelSaveStatus_->style()->unpolish(modelSaveStatus_);
    modelSaveStatus_->style()->polish(modelSaveStatus_);
}

void Setter::saveModelSettings() {
    ModelSettings current = collectModelSettings();
    QString error;
    if (!ModelSettingsStore::save(current, error)) {
        modelSaveStatus_->setText(error);
        modelSaveStatus_->setProperty("state", "error");
        modelSaveStatus_->style()->unpolish(modelSaveStatus_);
        modelSaveStatus_->style()->polish(modelSaveStatus_);
        return;
    }
    savedModelSettings_ = current;
    updateModelSaveState();
    if (activeModelService_ == ActiveModelService::Ollama) {
        const auto identity = ModelServiceIdentityStore::forOllama(current);
        emit activeModelChanged(identity.key, identity.displayName);
    }
}

void Setter::updateSystemPromptSaveState() {
    const QString current = systemPromptEdit_->toPlainText();
    const bool changed = current != savedSystemRules_;
    systemPromptSaveButton_->setEnabled(changed);
    systemPromptRestoreButton_->setEnabled(
        current != InlinePrompt::defaultSystemRules() ||
        savedSystemRules_ != InlinePrompt::defaultSystemRules());
    systemPromptStatus_->setText(changed ? "尚未保存" : "已保存");
    systemPromptStatus_->setProperty("state", changed ? "modified" : "saved");
    systemPromptStatus_->style()->unpolish(systemPromptStatus_);
    systemPromptStatus_->style()->polish(systemPromptStatus_);
}

void Setter::saveSystemPromptRules() {
    const QString current = systemPromptEdit_->toPlainText();
    QString error;
    if (!InlinePromptSettingsStore::save(current, error)) {
        systemPromptStatus_->setText(error);
        systemPromptStatus_->setProperty("state", "error");
        systemPromptStatus_->style()->unpolish(systemPromptStatus_);
        systemPromptStatus_->style()->polish(systemPromptStatus_);
        return;
    }
    savedSystemRules_ = current;
    updateSystemPromptSaveState();
}

void Setter::restoreDefaultSystemPrompt() {
    const QString defaults = InlinePrompt::defaultSystemRules();
    QString error;
    if (!InlinePromptSettingsStore::save(defaults, error)) {
        systemPromptStatus_->setText(error);
        systemPromptStatus_->setProperty("state", "error");
        systemPromptStatus_->style()->unpolish(systemPromptStatus_);
        systemPromptStatus_->style()->polish(systemPromptStatus_);
        return;
    }
    savedSystemRules_ = defaults;
    systemPromptEdit_->setPlainText(defaults);
    updateSystemPromptSaveState();
    systemPromptStatus_->setText("已恢复默认并保存");
}

void Setter::setCloudStatus(const QString &state, const QString &message) {
    cloudStatus_->setText(message);
    cloudStatus_->setProperty("state", state);
    cloudStatus_->style()->unpolish(cloudStatus_);
    cloudStatus_->style()->polish(cloudStatus_);
}

void Setter::refreshActiveServiceStatus() {
    if (activeModelService_ == ActiveModelService::Ollama) {
        activeServiceStatus_->setText("当前用于短语建议：Ollama");
    } else {
        QString name = "未选择配置";
        for (const CloudProfile &profile : cloudProfiles_) {
            if (profile.id == activeCloudProfileId_) {
                name = profile.name;
                break;
            }
        }
        activeServiceStatus_->setText(
            QStringLiteral("当前用于短语建议：云端 · %1").arg(name));
    }
    bool savedSelection = false;
    for (const CloudProfile &profile : cloudProfiles_)
        if (profile.id == editingCloudProfileId_) savedSelection = true;
    useCloudButton_->setEnabled(savedSelection);
}

void Setter::showModelSource(ActiveModelService source) {
    modelSourceStack_->setCurrentIndex(source == ActiveModelService::Cloud ? 1 : 0);
    cloudSourceButton_->setChecked(source == ActiveModelService::Cloud);
    ollamaSourceButton_->setChecked(source == ActiveModelService::Ollama);
}

void Setter::refreshCloudProfileCombo() {
    const QSignalBlocker blocker(cloudProfileCombo_);
    cloudProfileCombo_->clear();
    for (const CloudProfile &profile : cloudProfiles_)
    {
        cloudProfileCombo_->addItem(profile.name, profile.id);
        cloudProfileCombo_->setItemData(cloudProfileCombo_->count() - 1,
                                        QSize(0, 38), Qt::SizeHintRole);
    }
    cloudProfileCombo_->setCurrentIndex(
        cloudProfileCombo_->findData(editingCloudProfileId_));
    cloudDeleteButton_->setEnabled(
        cloudProfileCombo_->findData(editingCloudProfileId_) >= 0);
    refreshActiveServiceStatus();
}

void Setter::loadCloudProfileForm(const QString &id) {
    editingCloudProfileId_ = id;
    const auto found = std::find_if(cloudProfiles_.cbegin(), cloudProfiles_.cend(),
        [&id](const CloudProfile &profile) { return profile.id == id; });
    const CloudProfile profile = found == cloudProfiles_.cend()
        ? CloudProfile{id, {}, {}, {}} : *found;
    const QSignalBlocker blockName(cloudNameEdit_);
    const QSignalBlocker blockUrl(cloudUrlEdit_);
    const QSignalBlocker blockModel(cloudModelEdit_);
    const QSignalBlocker blockKey(cloudKeyEdit_);
    cloudNameEdit_->setText(profile.name);
    cloudUrlEdit_->setText(profile.baseUrl);
    cloudModelEdit_->setText(profile.modelName);
    cloudKeyEdit_->clear();
    if (found != cloudProfiles_.cend()) {
        const QString host = QUrl(profile.baseUrl).host().toLower();
        const QString preset = host == "api.deepseek.com" ? "deepseek"
            : host.endsWith("dashscope.aliyuncs.com") ? "dashscope"
            : host == "api.openai.com" ? "openai" : "custom";
        cloudPresetCombo_->setCurrentIndex(cloudPresetCombo_->findData(preset));
    }
    for (QLineEdit *edit : {cloudNameEdit_, cloudUrlEdit_, cloudModelEdit_, cloudKeyEdit_})
        edit->setEnabled(!id.isEmpty());
    cloudDeleteButton_->setEnabled(found != cloudProfiles_.cend());
    QString key;
    QString error;
    if (!id.isEmpty() && !CloudCredentialStore::load(id, key, error))
        cloudKeyStatus_->setText(error);
    else
        cloudKeyStatus_->setText(key.isEmpty() ? "尚未保存 API Key" : "API Key 已保存在 Windows 凭据管理器");
    updateCloudSaveState();
    refreshActiveServiceStatus();
}

CloudProfile Setter::collectCloudProfile() const {
    return CloudProfile{editingCloudProfileId_,
                        cloudNameEdit_->text().trimmed(),
                        cloudUrlEdit_->text().trimmed(),
                        cloudModelEdit_->text().trimmed()};
}

bool Setter::cloudFormHasChanges() const {
    if (editingCloudProfileId_.isEmpty()) return false;
    const CloudProfile current = collectCloudProfile();
    for (const CloudProfile &saved : cloudProfiles_) {
        if (saved.id == editingCloudProfileId_)
            return saved.name != current.name || saved.baseUrl != current.baseUrl ||
                   saved.modelName != current.modelName ||
                   !cloudKeyEdit_->text().isEmpty();
    }
    return true;
}

bool Setter::confirmDiscardCloudEdits() {
    if (!cloudFormHasChanges()) return true;
    return QMessageBox::question(this, "未保存的云端配置",
        "当前云端配置有未保存的修改。放弃这些修改吗？",
        QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel) == QMessageBox::Discard;
}

void Setter::updateCloudSaveState() {
    if (editingCloudProfileId_.isEmpty()) {
        cloudSaveButton_->setEnabled(false);
        cloudTestButton_->setEnabled(false);
        setCloudStatus("neutral", "选择已有配置，或先新建一个云端配置。");
        return;
    }
    const bool changed = cloudFormHasChanges();
    cloudSaveButton_->setEnabled(changed);
    cloudTestButton_->setEnabled(true);
    if (!cloudKeyEdit_->text().isEmpty())
        cloudKeyStatus_->setText("已输入新 API Key，保存后替换原密钥");
    setCloudStatus(changed ? "neutral" : "saved",
                   changed ? "配置尚未保存" : "配置已保存");
}

void Setter::createCloudProfile() {
    if (!confirmDiscardCloudEdits()) return;
    const QString preset = cloudPresetCombo_->currentData().toString();
    QString name;
    QString url;
    QString model;
    if (preset == "deepseek") {
        name = "DeepSeek";
        url = "https://api.deepseek.com";
        model = "deepseek-flash";
    } else if (preset == "dashscope") {
        name = "阿里云百炼";
        url = "https://dashscope.aliyuncs.com/compatible-mode/v1";
        model = "qwen-plus";
    } else if (preset == "openai") {
        name = "OpenAI";
        url = "https://api.openai.com/v1";
        model = "gpt-4.1-mini";
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    loadCloudProfileForm(id);
    const QSignalBlocker blockName(cloudNameEdit_);
    const QSignalBlocker blockUrl(cloudUrlEdit_);
    const QSignalBlocker blockModel(cloudModelEdit_);
    cloudNameEdit_->setText(name);
    cloudUrlEdit_->setText(url);
    cloudModelEdit_->setText(model);
    {
        const QSignalBlocker blocker(cloudProfileCombo_);
        cloudProfileCombo_->setCurrentIndex(-1);
    }
    updateCloudSaveState();
    cloudNameEdit_->setFocus();
}

void Setter::saveCloudProfile() {
    CloudProfile profile = collectCloudProfile();
    QString error;
    if (!CloudProfileStore::validate(profile, error)) {
        setCloudStatus("error", error);
        return;
    }
    QString previousKey;
    if (!CloudCredentialStore::load(profile.id, previousKey, error)) {
        setCloudStatus("error", error);
        return;
    }
    const QString newKey = cloudKeyEdit_->text().trimmed();
    if (newKey.isEmpty() && previousKey.isEmpty()) {
        setCloudStatus("error", "请输入 API Key 后再保存配置。");
        return;
    }
    if (!newKey.isEmpty() && !CloudCredentialStore::save(profile.id, newKey, error)) {
        setCloudStatus("error", error);
        return;
    }
    QVector<CloudProfile> updated = cloudProfiles_;
    bool replaced = false;
    for (CloudProfile &existing : updated) {
        if (existing.id == profile.id) {
            existing = profile;
            replaced = true;
            break;
        }
    }
    if (!replaced) updated.append(profile);
    const QString nextActiveId = activeCloudProfileId_.isEmpty()
        ? profile.id : activeCloudProfileId_;
    if (!CloudProfileStore::save(updated, nextActiveId, error)) {
        if (!newKey.isEmpty()) {
            QString rollbackError;
            if (previousKey.isEmpty())
                CloudCredentialStore::remove(profile.id, rollbackError);
            else
                CloudCredentialStore::save(profile.id, previousKey, rollbackError);
            if (!rollbackError.isEmpty()) error += " " + rollbackError;
        }
        setCloudStatus("error", error);
        return;
    }
    cloudProfiles_ = updated;
    activeCloudProfileId_ = nextActiveId;
    cloudKeyEdit_->clear();
    cloudKeyStatus_->setText("API Key 已保存在 Windows 凭据管理器");
    refreshCloudProfileCombo();
    updateCloudSaveState();
    setCloudStatus("saved", "配置已保存；点击“使用选中的云端配置”可切换写作页。");
    if (activeModelService_ == ActiveModelService::Cloud &&
        activeCloudProfileId_ == profile.id) {
        const auto identity = ModelServiceIdentityStore::forCloud(profile);
        emit activeModelChanged(identity.key, identity.displayName);
    }
}

void Setter::deleteCloudProfile() {
    const QString id = editingCloudProfileId_;
    if (id.isEmpty()) return;
    const auto found = std::find_if(cloudProfiles_.cbegin(), cloudProfiles_.cend(),
        [&id](const CloudProfile &profile) { return profile.id == id; });
    if (found == cloudProfiles_.cend()) {
        editingCloudProfileId_.clear();
        refreshCloudProfileCombo();
        loadCloudProfileForm({});
        return;
    }
    if (activeModelService_ == ActiveModelService::Cloud && id == activeCloudProfileId_) {
        setCloudStatus("error", "当前正在使用此配置；请先切换到 Ollama 或其他云端配置。");
        return;
    }
    if (QMessageBox::question(this, "删除云端配置",
        QStringLiteral("删除「%1」及其保存在 Windows 的 API Key？").arg(found->name),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel) != QMessageBox::Yes) return;
    QVector<CloudProfile> updated = cloudProfiles_;
    updated.erase(std::remove_if(updated.begin(), updated.end(),
        [&id](const CloudProfile &profile) { return profile.id == id; }), updated.end());
    const QString nextActiveId = id == activeCloudProfileId_
        ? (updated.isEmpty() ? QString() : updated.first().id)
        : activeCloudProfileId_;
    QString error;
    QString previousKey;
    if (!CloudCredentialStore::load(id, previousKey, error) ||
        !CloudCredentialStore::remove(id, error)) {
        setCloudStatus("error", error);
        return;
    }
    if (!CloudProfileStore::save(updated, nextActiveId, error)) {
        if (!previousKey.isEmpty()) {
            QString rollbackError;
            CloudCredentialStore::save(id, previousKey, rollbackError);
            if (!rollbackError.isEmpty()) error += " " + rollbackError;
        }
        setCloudStatus("error", error);
        return;
    }
    cloudProfiles_ = updated;
    activeCloudProfileId_ = nextActiveId;
    editingCloudProfileId_ = updated.isEmpty() ? QString() : updated.first().id;
    refreshCloudProfileCombo();
    loadCloudProfileForm(editingCloudProfileId_);
    setCloudStatus("saved", "配置和密钥已删除。");
}

void Setter::activateCloudProfile() {
    if (editingCloudProfileId_.isEmpty() || cloudFormHasChanges()) {
        setCloudStatus("error", "请先保存当前云端配置。");
        return;
    }
    QString key;
    QString error;
    if (!CloudCredentialStore::load(editingCloudProfileId_, key, error) || key.isEmpty()) {
        setCloudStatus("error", error.isEmpty() ? "此配置没有 API Key。" : error);
        return;
    }
    if (!CloudProfileStore::save(cloudProfiles_, editingCloudProfileId_, error) ||
        !ActiveModelServiceStore::save(ActiveModelService::Cloud, error)) {
        setCloudStatus("error", error);
        return;
    }
    activeCloudProfileId_ = editingCloudProfileId_;
    activeModelService_ = ActiveModelService::Cloud;
    refreshActiveServiceStatus();
    setCloudStatus("saved", "写作页现在使用选中的云端配置。");
    for (const CloudProfile &profile : cloudProfiles_) {
        if (profile.id == activeCloudProfileId_) {
            const auto identity = ModelServiceIdentityStore::forCloud(profile);
            emit activeModelChanged(identity.key, identity.displayName);
            break;
        }
    }
}

void Setter::testCloudProfile() {
    const CloudProfile profile = collectCloudProfile();
    const auto identity = ModelServiceIdentityStore::forCloud(profile);
    QString error;
    if (!CloudProfileStore::validate(profile, error)) {
        setCloudStatus("error", error);
        return;
    }
    if (profile.baseUrl.startsWith("https://", Qt::CaseInsensitive) &&
        !QSslSocket::supportsSsl()) {
        setCloudStatus("error", "无法使用 HTTPS：缺少 Qt TLS 后端，请检查 plugins/tls/qschannelbackend.dll。");
        return;
    }
    QString key = cloudKeyEdit_->text().trimmed();
    if (key.isEmpty() && !CloudCredentialStore::load(profile.id, key, error)) {
        setCloudStatus("error", error);
        return;
    }
    if (key.isEmpty()) {
        setCloudStatus("error", "请输入 API Key 后再测试。");
        return;
    }
    cloudTestButton_->setEnabled(false);
    setCloudStatus("neutral", QStringLiteral("正在测试「%1」…").arg(profile.name));
    emit modelAvailabilityChanged(identity.key, identity.displayName,
                                  "pending", "正在测试云端模型…");
    const QByteArray json = QJsonDocument(CloudChatApi::body(
        profile, "请简短回答。", "只回复：测试成功", 512))
        .toJson(QJsonDocument::Compact);
    QNetworkReply *reply = modelNetwork_->post(CloudChatApi::request(profile, key), json);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profile, identity] {
        cloudTestButton_->setEnabled(true);
        const auto networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        const int httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->isOpen() ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if (collectCloudProfile().id != profile.id ||
            collectCloudProfile().baseUrl != profile.baseUrl ||
            collectCloudProfile().modelName != profile.modelName) {
            setCloudStatus("neutral", "配置已变化，请重新测试。");
            emit modelAvailabilityChanged(identity.key, identity.displayName,
                                          "unknown", "配置已变化，请重新测试。");
            return;
        }
        const QString serverError = CloudChatApi::serverError(data);
        if (networkError != QNetworkReply::NoError || httpStatus >= 300 ||
            !serverError.isEmpty()) {
            const QString reason = !serverError.isEmpty() ? serverError
                : networkError != QNetworkReply::NoError ? networkErrorText
                : QStringLiteral("HTTP %1").arg(httpStatus);
            setCloudStatus("error", QStringLiteral("测试失败：%1").arg(reason));
            emit modelAvailabilityChanged(identity.key, identity.displayName,
                                          "error", reason);
            return;
        }
        QString content;
        QString parseError;
        if (!CloudChatApi::parseContent(data, content, parseError) ||
            content.trimmed().isEmpty()) {
            const QString reason = parseError.isEmpty()
                ? QStringLiteral("服务已响应，但没有生成文本。") : parseError;
            setCloudStatus("error", reason);
            emit modelAvailabilityChanged(identity.key, identity.displayName,
                                          "error", reason);
            return;
        }
        setCloudStatus("saved", QStringLiteral("「%1」测试成功：%2")
            .arg(profile.name, content.trimmed().left(60)));
        emit modelAvailabilityChanged(identity.key, identity.displayName,
                                      "available", "模型测试成功，已生成回复。");
    });
}

void Setter::addKnownModel() {
    const QString name = modelNameInput_->text().trimmed();
    if (name.isEmpty()) return;
    for (const QChar ch : name) {
        if (ch.isSpace()) {
            setModelConnectionStatus("error", "模型名称不能包含空格或换行。");
            return;
        }
    }
    int index = modelCombo_->findText(name);
    if (index < 0) {
        addModelCandidate(modelCombo_, name);
        index = modelCombo_->count() - 1;
    }
    modelCombo_->setCurrentIndex(index);
    modelNameInput_->clear();
    modelRemoveButton_->setEnabled(modelCombo_->count() > 1);
    updateModelSaveState();
    setModelConnectionStatus("neutral", "已加入候选；可先测试当前模型，再保存设置。");
}

void Setter::removeSelectedModel() {
    if (modelCombo_->count() <= 1) return;
    const QString removed = modelCombo_->currentText();
    modelCombo_->removeItem(modelCombo_->currentIndex());
    modelRemoveButton_->setEnabled(modelCombo_->count() > 1);
    updateModelSaveState();
    setModelConnectionStatus("neutral",
        QStringLiteral("已移除候选「%1」；点击“保存模型设置”后生效。").arg(removed));
}

void Setter::setModelConnectionStatus(const QString &state,
                                      const QString &message) {
    modelConnectionStatus_->setText(message);
    modelConnectionStatus_->setProperty("state", state);
    modelConnectionStatus_->style()->unpolish(modelConnectionStatus_);
    modelConnectionStatus_->style()->polish(modelConnectionStatus_);
}

void Setter::fetchInstalledModels() {
    const ModelSettings settings = collectModelSettings();
    QString error;
    if (!ModelSettingsStore::validate(settings, error)) {
        setModelConnectionStatus("error", error);
        return;
    }

    modelRefreshButton_->setEnabled(false);
    modelTestButton_->setEnabled(false);
    setModelConnectionStatus("neutral", "正在连接 Ollama…");
    QNetworkRequest request(QUrl(settings.baseUrl + "/api/tags"));
    request.setTransferTimeout(10'000);
    QNetworkReply *reply = modelNetwork_->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestedUrl = settings.baseUrl] {
        modelRefreshButton_->setEnabled(true);
        modelTestButton_->setEnabled(true);
        const QByteArray data = reply->readAll();
        const auto networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        const int httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (collectModelSettings().baseUrl != requestedUrl) {
            setModelConnectionStatus("neutral", "服务地址已变化，请重新检测。");
            return;
        }
        if (networkError != QNetworkReply::NoError || httpStatus >= 400) {
            setModelConnectionStatus("error",
                QStringLiteral("连接失败：%1").arg(networkError != QNetworkReply::NoError
                    ? networkErrorText : QStringLiteral("HTTP %1").arg(httpStatus)));
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(data);
        if (!document.isObject() || !document.object().value("models").isArray()) {
            setModelConnectionStatus("error", "服务已响应，但模型列表格式不正确。");
            return;
        }
        const QJsonArray models = document.object().value("models").toArray();
        int discovered = 0;
        for (const QJsonValue &value : models) {
            const QString name = value.toObject().value("name").toString().trimmed();
            if (name.isEmpty()) continue;
            ++discovered;
            if (modelCombo_->findText(name) < 0)
                addModelCandidate(modelCombo_, name);
        }
        modelRemoveButton_->setEnabled(modelCombo_->count() > 1);
        updateModelSaveState();
        setModelConnectionStatus("saved", QStringLiteral("已读取 %1 个模型；请选择并测试当前模型，最后保存设置。")
            .arg(discovered));
    });
}

void Setter::testSelectedModel() {
    const ModelSettings settings = collectModelSettings();
    const auto identity = ModelServiceIdentityStore::forOllama(settings);
    QString error;
    if (!ModelSettingsStore::validate(settings, error)) {
        setModelConnectionStatus("error", error);
        return;
    }

    modelTestButton_->setEnabled(false);
    modelRefreshButton_->setEnabled(false);
    setModelConnectionStatus("neutral",
        QStringLiteral("正在测试「%1」生成回复…").arg(settings.modelName));
    emit modelAvailabilityChanged(identity.key, identity.displayName,
                                  "pending", "正在测试 Ollama 模型…");
    QNetworkRequest request(QUrl(settings.baseUrl + "/api/generate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    constexpr int testTimeoutMs = 120'000;
    request.setTransferTimeout(testTimeoutMs);
    QJsonObject body{{"model", settings.modelName},
                     {"prompt", QStringLiteral("请只回复：测试成功")},
                     {"stream", false},
                     {"options", QJsonObject{{"num_predict", 128}}}};
    body["think"] = settings.modelName.startsWith("gpt-oss", Qt::CaseInsensitive)
        ? QJsonValue(QStringLiteral("low")) : QJsonValue(false);
    QNetworkReply *reply = modelNetwork_->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QElapsedTimer elapsed;
    elapsed.start();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestedUrl = settings.baseUrl,
             requestedModel = settings.modelName, identity, elapsed] {
        modelTestButton_->setEnabled(true);
        modelRefreshButton_->setEnabled(true);
        const auto networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        const int httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 超时或取消后设备可能已关闭；此时 readAll() 会产生 Qt 警告。
        const QByteArray data = reply->isOpen() ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        const ModelSettings current = collectModelSettings();
        if (current.baseUrl != requestedUrl || current.modelName != requestedModel) {
            setModelConnectionStatus("neutral", "模型或服务地址已变化，请重新测试。");
            emit modelAvailabilityChanged(identity.key, identity.displayName,
                                          "unknown", "模型或地址已变化，请重新测试。");
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(data);
        const QJsonObject result = document.isObject() ? document.object() : QJsonObject{};
        const QString serverError = result.value("error").toString();
        if (networkError != QNetworkReply::NoError || httpStatus >= 400 ||
            !serverError.isEmpty()) {
            const bool timedOut = networkError == QNetworkReply::TimeoutError ||
                (networkError == QNetworkReply::OperationCanceledError &&
                 elapsed.elapsed() >= testTimeoutMs - 1'000);
            const QString reason = !serverError.isEmpty() ? serverError
                : timedOut ? QStringLiteral("等待模型回复超过 120 秒，请确认 Ollama 正在运行并检查模型加载状态")
                : networkError != QNetworkReply::NoError ? networkErrorText
                : QStringLiteral("HTTP %1").arg(httpStatus);
            setModelConnectionStatus("error",
                QStringLiteral("「%1」测试失败：%2").arg(requestedModel, reason));
            emit modelAvailabilityChanged(identity.key, identity.displayName,
                                          "error", reason);
            return;
        }
        if (!result.value("response").isString() ||
            result.value("response").toString().trimmed().isEmpty()) {
            setModelConnectionStatus("error",
                QStringLiteral("「%1」已响应，但没有生成文本。").arg(requestedModel));
            emit modelAvailabilityChanged(identity.key, identity.displayName,
                                          "error", "模型已响应，但没有生成文本。");
            return;
        }
        setModelConnectionStatus("saved",
            QStringLiteral("「%1」测试成功：模型已生成回复。%2")
                .arg(requestedModel, result.value("response").toString().trimmed().left(60)));
        emit modelAvailabilityChanged(identity.key, identity.displayName,
                                      "available", "模型测试成功，已生成回复。");
    });
}

WritingConvention Setter::collectGlobalConvention() const {
    WritingConvention convention;
    convention.viewpoint = globalViewpointCombo_->currentText().trimmed();
    convention.tense = globalTenseCombo_->currentText().trimmed();
    if (convention.viewpoint == "未指定") convention.viewpoint.clear();
    if (convention.tense == "未指定") convention.tense.clear();
    convention.style = globalStyleEdit_->toPlainText();
    convention.rhythm = globalRhythmEdit_->toPlainText();
    convention.constraints = globalConstraintsEdit_->toPlainText();
    return convention;
}

void Setter::updateGlobalSaveState() {
    const WritingConvention current = collectGlobalConvention();
    const bool changed = current.viewpoint != savedConvention_.viewpoint ||
                         current.tense != savedConvention_.tense ||
                         current.style != savedConvention_.style ||
                         current.rhythm != savedConvention_.rhythm ||
                         current.constraints != savedConvention_.constraints;
    globalSaveButton_->setEnabled(changed);
    globalSaveStatus_->setText(changed ? "尚未保存" : "已保存");
    globalSaveStatus_->setProperty("state", changed ? "modified" : "saved");
    globalSaveStatus_->style()->unpolish(globalSaveStatus_);
    globalSaveStatus_->style()->polish(globalSaveStatus_);
}

void Setter::saveGlobalConvention() {
    const WritingConvention current = collectGlobalConvention();
    QString error;
    if (!GlobalWritingConventionStore::save(current, error)) {
        globalSaveStatus_->setText(error);
        globalSaveStatus_->setProperty("state", "error");
        globalSaveStatus_->style()->unpolish(globalSaveStatus_);
        globalSaveStatus_->style()->polish(globalSaveStatus_);
        return;
    }
    savedConvention_ = current;
    updateGlobalSaveState();
    emit globalWritingConventionChanged();
}

QString Setter::defaultAuthorName() const {
    return savedAuthorName_;
}

QPixmap Setter::avatarPixmap(int size) const {
    if (size <= 0) return {};
    if (savedAvatarImage_.isNull()) {
        return QPixmap(":/avatars/default-avatar.png")
            .scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QPixmap avatar(size, size);
    avatar.fill(Qt::transparent);
    QPainter painter(&avatar);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.scale(size / 64.0, size / 64.0);

    const QRectF circle(1, 1, 62, 62);
    QPainterPath clip;
    clip.addEllipse(circle);
    painter.setClipPath(clip);
    const int side = qMin(savedAvatarImage_.width(), savedAvatarImage_.height());
    const QRectF source((savedAvatarImage_.width() - side) / 2.0,
                        (savedAvatarImage_.height() - side) / 2.0,
                        side, side);
    painter.drawImage(circle, savedAvatarImage_, source);
    painter.setClipping(false);
    painter.setPen(QPen(QColor("#8293bd"), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(circle);
    return avatar;
}

void Setter::showAuthorSection() {
    categoryList_->setCurrentRow(0);
}

void Setter::updateAuthorPreview(const QString &name) {
    const QString displayName = name.trimmed();
    authorPreviewName_->setText(displayName.isEmpty() ? "未设置作者" : displayName);
}

void Setter::updateAvatarPreview() {
    avatarPreview_->setPixmap(avatarPixmap(58));
    resetAvatarButton_->setEnabled(QFileInfo::exists(avatarPath_));
}

void Setter::chooseAvatar() {
    const QString sourcePath = QFileDialog::getOpenFileName(
        this, "选择头像图片", QDir::homePath(),
        "图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)");
    if (sourcePath.isEmpty()) return;

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid() &&
        (sourceSize.width() > 1024 || sourceSize.height() > 1024)) {
        reader.setScaledSize(sourceSize.scaled(1024, 1024, Qt::KeepAspectRatio));
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, "无法更换头像", "无法读取这张图片，请选择其他图片。");
        return;
    }

    const int side = qMin(image.width(), image.height());
    const QImage square = image.copy((image.width() - side) / 2,
                                     (image.height() - side) / 2,
                                     side, side)
                              .scaled(512, 512, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation);
    if (!QDir().mkpath(QFileInfo(avatarPath_).absolutePath())) {
        QMessageBox::warning(this, "无法更换头像", "无法创建头像保存目录。");
        return;
    }
    QSaveFile output(avatarPath_);
    if (!output.open(QIODevice::WriteOnly) ||
        !square.save(&output, "PNG") || !output.commit()) {
        output.cancelWriting();
        QMessageBox::warning(this, "无法更换头像", "无法保存头像图片。");
        return;
    }

    savedAvatarImage_ = square;
    updateAvatarPreview();
    emit authorAvatarChanged();
}

void Setter::resetAvatar() {
    if (!QFileInfo::exists(avatarPath_)) return;
    if (!QFile::remove(avatarPath_)) {
        QMessageBox::warning(this, "无法恢复默认", "无法删除已保存的头像图片。");
        return;
    }
    savedAvatarImage_ = QImage();
    updateAvatarPreview();
    emit authorAvatarChanged();
}

void Setter::setSaveStatus(const QString &state, const QString &text) {
    saveStatus_->setText(text);
    saveStatus_->setProperty("state", state);
    saveStatus_->style()->unpolish(saveStatus_);
    saveStatus_->style()->polish(saveStatus_);
}

void Setter::saveAuthorName() {
    const QString name = authorNameEdit_->text().trimmed();
    QSettings settings = authorSettings();
    settings.setValue("author/defaultName", name);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setSaveStatus("error", "保存失败，请检查设置目录");
        return;
    }

    savedAuthorName_ = name;
    authorNameEdit_->setText(name);
    saveButton_->setEnabled(false);
    setSaveStatus("saved", "已保存");
    emit defaultAuthorNameChanged(name);
}

void Setter::applyStyle() {
    setStyleSheet(R"QSS(
        QWidget#settingsPage {
            background-color: #1c1e22;
            color: #edf0f4;
            font-family: "Microsoft YaHei UI";
        }
        QLabel#settingsTitle {
            color: #edf0f4;
            font-size: 24px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#settingsDescription {
            color: #a1a9b4;
            font-size: 12px;
            background: transparent;
        }
        QFrame#settingsNav {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 9px;
        }
        QLabel#settingsNavTitle {
            color: #8e98a6;
            font-size: 11px;
            font-weight: bold;
            padding: 0 8px;
            background: transparent;
        }
        QLabel#settingsNavNote {
            color: #68717d;
            font-size: 11px;
            padding: 4px 8px;
            background: transparent;
        }
        QListWidget#settingsCategoryList {
            background: transparent;
            color: #cbd1da;
            border: none;
            outline: none;
            font-size: 13px;
        }
        QListWidget#settingsCategoryList::item {
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 10px 9px;
            margin: 2px 0;
        }
        QListWidget#settingsCategoryList::item:hover {
            background-color: #2b3038;
            color: #edf0f4;
        }
        QListWidget#settingsCategoryList::item:selected {
            background-color: #303958;
            border-color: #4b5887;
            color: #c5cfff;
            font-weight: bold;
        }
        QStackedWidget#settingsSections,
        QStackedWidget#settingsModelSourceStack,
        QScrollArea#settingsScroll,
        QWidget#settingsContent {
            background: transparent;
            border: none;
        }
        QFrame#settingsCard {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 10px;
        }
        QPushButton#settingsSourceChoice {
            background-color: #20242b;
            color: #d9e0ea;
            border: 1px solid #3d4654;
            border-radius: 8px;
            padding: 12px 16px;
            text-align: left;
            font-size: 13px;
        }
        QPushButton#settingsSourceChoice:hover {
            border-color: #697baa;
            background-color: #28303e;
        }
        QPushButton#settingsSourceChoice:checked {
            background-color: #303958;
            border-color: #8798d0;
            color: #edf1ff;
        }
        QFrame#settingsProviderActive,
        QFrame#settingsProviderPlanned {
            background-color: #1b1f25;
            border: 1px solid #3c4552;
            border-radius: 8px;
        }
        QFrame#settingsProviderActive {
            background-color: #283149;
            border-color: #5b6fa5;
        }
        QLabel#settingsProviderTitle {
            color: #e7ecf5;
            background: transparent;
            font-size: 13px;
            font-weight: bold;
        }
        QLabel#settingsProviderNote {
            color: #9eabba;
            background: transparent;
            font-size: 11px;
        }
        QLabel#settingsSectionTitle {
            color: #edf0f4;
            background: transparent;
            font-size: 19px;
            font-weight: bold;
        }
        QLabel#settingsBodyText {
            color: #a1a9b4;
            background: transparent;
            font-size: 12px;
        }
        QCheckBox#settingsInlineSuggestionCheck {
            color: #dce3ee;
            background: transparent;
            font-size: 14px;
            spacing: 10px;
        }
        QCheckBox#settingsInlineSuggestionCheck::indicator {
            width: 18px;
            height: 18px;
            background-color: #191c20;
            border: 1px solid #697588;
            border-radius: 5px;
        }
        QCheckBox#settingsInlineSuggestionCheck::indicator:hover {
            border-color: #8292cb;
        }
        QCheckBox#settingsInlineSuggestionCheck::indicator:checked {
            background-color: #4b5d96;
            border-color: #8292cb;
            image: url(:/icons/checkmark.xpm);
        }
        QLabel#settingsDataPath,
        QLabel#settingsDataPathError {
            background-color: #191c20;
            color: #dce3ee;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 12px;
            font-size: 13px;
        }
        QLabel#settingsDataPathError {
            color: #e28e8e;
            border-color: #805354;
        }
        QLabel#settingsAvatarPreview {
            background: transparent;
            border: none;
            border-radius: 29px;
        }
        QPushButton#settingsAvatarAction {
            background-color: #2b3039;
            color: #cbd4e3;
            border: 1px solid #46505f;
            border-radius: 6px;
            padding: 7px 13px;
            font-size: 12px;
        }
        QPushButton#settingsAvatarAction:hover {
            background-color: #354051;
            border-color: #7384ba;
            color: #edf0f4;
        }
        QPushButton#settingsAvatarAction:disabled {
            background-color: #252930;
            color: #697382;
            border-color: #363d47;
        }
        QLabel#settingsAuthorName {
            color: #edf0f4;
            background: transparent;
            font-size: 15px;
            font-weight: bold;
        }
        QLabel#settingsMutedText {
            color: #8893a1;
            background: transparent;
            font-size: 11px;
        }
        QFrame#settingsDivider {
            color: #373c43;
            background-color: #373c43;
            border: none;
            max-height: 1px;
        }
        QLabel#settingsFieldLabel {
            color: #dce1e8;
            background: transparent;
            font-size: 13px;
            font-weight: bold;
        }
        QLineEdit#settingsAuthorEdit,
        QLineEdit#settingsModelEdit {
            background-color: #191c20;
            color: #edf0f4;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 10px 12px;
            min-height: 23px;
            font-size: 13px;
            selection-background-color: #394567;
        }
        QLineEdit#settingsAuthorEdit:hover,
        QLineEdit#settingsModelEdit:hover {
            border-color: #5a6474;
        }
        QLineEdit#settingsAuthorEdit:focus,
        QLineEdit#settingsModelEdit:focus {
            border-color: #8292cb;
            background-color: #20242c;
        }
        QComboBox#settingsConventionCombo,
        QComboBox#settingsModelCombo,
        QPlainTextEdit#settingsConventionEdit,
        QPlainTextEdit#settingsSystemPromptEdit {
            background-color: #191c20;
            color: #edf0f4;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 9px 12px;
            selection-background-color: #394567;
        }
        QComboBox#settingsConventionCombo:focus,
        QComboBox#settingsModelCombo:focus,
        QPlainTextEdit#settingsConventionEdit:focus,
        QPlainTextEdit#settingsSystemPromptEdit:focus {
            border-color: #8292cb;
        }
        QPlainTextEdit#settingsSystemPromptEdit {
            font-size: 13px;
            padding: 12px 14px;
        }
        QComboBox#settingsConventionCombo,
        QComboBox#settingsModelCombo {
            min-height: 28px;
            padding-right: 24px;
        }
        QComboBox#settingsConventionCombo::drop-down,
        QComboBox#settingsModelCombo::drop-down {
            width: 24px;
            border: none;
        }
        QComboBox#settingsConventionCombo::down-arrow,
        QComboBox#settingsModelCombo::down-arrow {
            image: url(:/icons/chevron-down.xpm);
            width: 12px;
            height: 12px;
        }
        QComboBox#settingsConventionCombo QAbstractItemView,
        QComboBox#settingsModelCombo QAbstractItemView {
            background-color: #252a32;
            color: #edf0f4;
            border: 1px solid #46505f;
            selection-background-color: #394567;
        }
        QPushButton#settingsSaveButton {
            background-color: #303958;
            color: #d3dbff;
            border: 1px solid #56669b;
            border-radius: 7px;
            padding: 9px 20px;
            font-size: 12px;
            font-weight: bold;
            min-width: 92px;
        }
        QPushButton#settingsSaveButton:hover {
            background-color: #3a476c;
            border-color: #7384ba;
        }
        QPushButton#settingsSaveButton:pressed {
            background-color: #28314e;
        }
        QPushButton#settingsSaveButton:disabled {
            background-color: #2b2f38;
            color: #778092;
            border-color: #3b414c;
        }
        QLabel#settingsSaveStatus {
            color: #8893a1;
            background: transparent;
            font-size: 11px;
        }
        QLabel#settingsSaveStatus[state="modified"] { color: #e0b769; }
        QLabel#settingsSaveStatus[state="saved"] { color: #81bd98; }
        QLabel#settingsSaveStatus[state="error"] { color: #e28e8e; }
        QLabel#settingsModelStatus {
            color: #9aa6b6;
            background: transparent;
            font-size: 11px;
        }
        QLabel#settingsModelStatus[state="saved"] { color: #81bd98; }
        QLabel#settingsModelStatus[state="error"] { color: #e28e8e; }
        QFrame#settingsNoteCard {
            background-color: #222830;
            border: 1px solid #374251;
            border-radius: 8px;
        }
        QLabel#settingsNoteTitle {
            color: #c7d1e1;
            background: transparent;
            font-size: 12px;
            font-weight: bold;
        }
        QLabel#settingsBadge {
            color: #aeb8c8;
            background-color: #2b3442;
            border: 1px solid #43516a;
            border-radius: 6px;
            padding: 4px 9px;
            font-size: 11px;
        }
        QScrollBar:vertical {
            background-color: #202327;
            width: 8px;
            margin: 4px 1px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background-color: #4b5360;
            min-height: 28px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover { background-color: #697588; }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical { background: none; }
    )QSS");
}
