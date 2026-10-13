//
// Created by shijie on 2026/10/4.
//

#include "CreateProjectDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListView>
CreateProjectDialog::CreateProjectDialog(const QString &projectsPath, QWidget *parent): QDialog(parent) {
    setObjectName("createProjectDialog");
    setWindowTitle("新建作品");
    resize(520, 640);
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);

    titleLabel = new QLabel("新建作品", this);
    titleLabel->setObjectName("dialogTitle");
    layout->addWidget(titleLabel);

    descriptionLabel = new QLabel("填写基本信息，创建后即可开始写作。", this);
    descriptionLabel->setObjectName("dialogDescription");
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    layout->addWidget(new QLabel("作品名称",this));

    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("请输入作品名称"));
    layout->addWidget(nameEdit);

    layout->addWidget(new QLabel("作者(选填)",this));
    authorEdit = new QLineEdit(this);
    authorEdit->setPlaceholderText("请输入作者");
    layout->addWidget(authorEdit);

    layout->addWidget(new QLabel("作品类型", this));
    genreCombo = new QComboBox(this);
    genreCombo->setObjectName("genreCombo");
    auto *genreView = new QListView(genreCombo);
    genreView->setObjectName("genrePopup");
    genreView->setUniformItemSizes(true);
    genreView->setMouseTracking(true);
    genreView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    genreCombo->setView(genreView);
    genreCombo->setMaxVisibleItems(7);
    genreCombo->addItems(tags);
    genreCombo->setCurrentText("待设定");
    layout->addWidget(genreCombo);

    layout->addWidget(new QLabel("作品标签（选填）", this));
    tagsEdit = new QLineEdit(this);
    tagsEdit->setPlaceholderText("多个标签用逗号分隔");
    layout->addWidget(tagsEdit);

    layout->addWidget(new QLabel("作品简介（选填）", this));
    synopsisEdit = new QPlainTextEdit(this);
    synopsisEdit->setPlaceholderText("简单介绍你的故事");
    synopsisEdit->setMinimumHeight(100);
    layout->addWidget(synopsisEdit, 1);
    errorLabel = new QLabel(this);
    errorLabel->setObjectName("formError");
    errorLabel->setWordWrap(true);
    errorLabel->setMinimumHeight(18);
    layout->addWidget(errorLabel);
    auto *pathLabel = new QLabel(
    QString("保存位置：%1").arg(projectsPath), this
);
    pathLabel->setObjectName("storagePath");
    pathLabel->setTextFormat(Qt::PlainText);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    auto *footLayout = new QHBoxLayout();
    footLayout->setSpacing(10);
    footLayout->addStretch();

    auto *cancelButton = new QPushButton("取消", this);
    cancelButton->setObjectName("dialogCancelButton");
    footLayout->addWidget(cancelButton);
    confirmButton = new QPushButton("创建作品", this);
    confirmButton->setObjectName("dialogCreateButton");
    confirmButton->setDefault(true);
    footLayout->addWidget(confirmButton);
    layout->addLayout(footLayout);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked, this, [this]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            errorLabel->setText("请输入作品名称");
            nameEdit->setFocus();
            return;
        }

        accept();
    });
    connect(nameEdit, &QLineEdit::textChanged,
        errorLabel, &QLabel::clear);

    uiStyle();
}

QString CreateProjectDialog::projectName() const {
    return nameEdit->text().trimmed();
}

QString CreateProjectDialog::projectGenre() const {
    return genreCombo->currentText();
}

QStringList CreateProjectDialog::projectTags() const {
    QString text = tagsEdit->text();
    text.replace("，", ",");
    QStringList tags;
    for (const auto &part : text.split(',', Qt::SkipEmptyParts)) {
        const QString tag = part.trimmed();

        if (!tag.isEmpty() && !tags.contains(tag)) {
            tags.append(tag);
        }
    }
    return tags;
}

QString CreateProjectDialog::projectSynopsis() const {
    return synopsisEdit->toPlainText().trimmed();
}

QString CreateProjectDialog::projectAuthor() const {
    return authorEdit->text().trimmed();
}

void CreateProjectDialog::loadProject(const Project &project) {
    titleLabel->setText("编辑作品信息");
    descriptionLabel->setText("修改作品资料，章节和正文会保留。");
    setWindowTitle("编辑作品信息");
    confirmButton->setText("保存修改");
    nameEdit->setText(project.name);
    const QString genre = project.genre.trimmed().isEmpty()
        ? QStringLiteral("待设定")
        : project.genre;
    if (genreCombo->findText(genre) < 0) {
        genreCombo->addItem(genre);
    }
    genreCombo->setCurrentText(genre);
    tagsEdit->setText(project.tags.join(","));

    authorEdit->setText(project.authorName);
    synopsisEdit->setPlainText(project.synopsis);



}

void CreateProjectDialog::setDefaultAuthorName(const QString &name) {
    authorEdit->setText(name);
}

void CreateProjectDialog::uiStyle() {
setStyleSheet(R"(
    QDialog#createProjectDialog {
        background-color: #1c1e22;
    }

    QWidget {
        font-family: "Microsoft YaHei UI";
        font-size: 13px;
    }

    QLabel {
        background-color: transparent;
        color: #a1a9b4;
        border: none;
    }

    QLabel#dialogTitle {
        color: #edf0f4;
        font-size: 22px;
        font-weight: bold;
    }

    QLabel#dialogDescription {
        color: #8e98a6;
        font-size: 12px;
        padding-bottom: 6px;
    }

    QLineEdit, QComboBox, QPlainTextEdit {
        background-color: #202327;
        color: #edf0f4;
        border: 1px solid #373c43;
        border-radius: 6px;
        padding: 9px 11px;
        selection-background-color: #394567;
        selection-color: #edf0f4;
    }

    QLineEdit, QPlainTextEdit {
        placeholder-text-color: #747e8b;
    }

    QLineEdit:hover, QComboBox:hover, QPlainTextEdit:hover {
        border-color: #4b5260;
    }

    QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
        border-color: #7d89b7;
        background-color: #242830;
    }

    QComboBox#genreCombo {
        padding-right: 40px;
        min-height: 20px;
    }

    QComboBox#genreCombo::drop-down {
        subcontrol-origin: padding;
        subcontrol-position: top right;
        width: 30px;
        background-color: #282d36;
        border-left: 1px solid #373c43;
        border-top-right-radius: 5px;
        border-bottom-right-radius: 5px;
    }

    QComboBox#genreCombo::drop-down:hover {
        background-color: #303958;
    }

    QComboBox#genreCombo::down-arrow {
        image: url(:/icons/chevron-down.xpm);
        width: 12px;
        height: 8px;
    }

    QComboBox#genreCombo::down-arrow:on {
        top: 1px;
    }

    QComboBox#genreCombo > QFrame {
        background-color: #25282d;
        border: 1px solid #4b5260;
        border-radius: 6px;
    }

    QListView#genrePopup {
        background-color: #25282d;
        color: #cbd1da;
        border: none;
        padding: 4px;
        outline: 0;
        selection-background-color: #303958;
        selection-color: #dce2ff;
    }

    QListView#genrePopup::item {
        min-height: 20px;
        padding: 6px 10px;
        border: 1px solid transparent;
        border-radius: 4px;
    }

    QListView#genrePopup::item:hover {
        background-color: #2f3540;
    }

    QListView#genrePopup::item:selected {
        background-color: #303958;
        color: #dce2ff;
        border-color: #4b5887;
    }

    QPushButton {
        background-color: #25282d;
        color: #cbd1da;
        border: 1px solid #373c43;
        border-radius: 6px;
        padding: 9px 18px;
    }

    QPushButton:hover {
        background-color: #30343a;
        border-color: #4b5260;
    }

    QPushButton:pressed {
        background-color: #202327;
    }

    QPushButton:focus {
        border-color: #7d89b7;
    }

    QPushButton#dialogCreateButton {
        background-color: #303958;
        color: #bac5ff;
        border-color: #4b5887;
        font-weight: bold;
    }

    QPushButton#dialogCreateButton:hover {
        background-color: #394567;
        border-color: #6271a4;
    }

    QPushButton#dialogCreateButton:pressed {
        background-color: #29324d;
    }

    QLabel#formError {
        color: #e2b05e;
        font-size: 12px;
    }

    QLabel#storagePath {
        background-color: #202327;
        color: #8993a1;
        border: 1px solid #31353c;
        border-radius: 6px;
        padding: 9px 11px;
        font-size: 11px;
    }

    QScrollBar:vertical {
        background-color: transparent;
        width: 8px;
        margin: 0;
        border: none;
    }

    QScrollBar::handle:vertical {
        background-color: #4b5260;
        min-height: 24px;
        border-radius: 4px;
    }

    QScrollBar::handle:vertical:hover {
        background-color: #68717d;
    }

    QScrollBar::add-line:vertical,
    QScrollBar::sub-line:vertical {
        height: 0;
        border: none;
        background-color: transparent;
    }

    QScrollBar::add-page:vertical,
    QScrollBar::sub-page:vertical {
        background-color: transparent;
    }
)");
}

