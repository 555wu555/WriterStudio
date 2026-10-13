#include "CreateEntityDialog.h"
#include "../../models/Entity.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
QLabel *fieldLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("entityFieldLabel");
    return label;
}
}

CreateEntityDialog::CreateEntityDialog(QWidget *parent) : QDialog(parent) {
    setObjectName("createEntityDialog");
    setWindowTitle("新建资料");
    resize(540, 650);
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(12);

    titleLabel_ = new QLabel("新建资料", this);
    titleLabel_->setObjectName("entityDialogTitle");
    descriptionLabel_ = new QLabel("记录人物或世界要素，之后可以在关系图谱中引用。", this);
    descriptionLabel_->setObjectName("entityDialogDescription");
    descriptionLabel_->setWordWrap(true);
    root->addWidget(titleLabel_);
    root->addWidget(descriptionLabel_);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("entityDialogScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *form = new QWidget(scroll);
    form->setObjectName("entityDialogForm");
    auto *fields = new QVBoxLayout(form);
    fields->setContentsMargins(0, 8, 8, 8);
    fields->setSpacing(9);

    fields->addWidget(fieldLabel("类别", form));
    kindCombo_ = new QComboBox(form);
    kindCombo_->setObjectName("entityKindCombo");
    kindCombo_->addItem("人物", "person");
    kindCombo_->addItem("地点", "place");
    kindCombo_->addItem("组织", "organization");
    kindCombo_->addItem("物件", "item");
    auto *kindView = new QListView(kindCombo_);
    kindView->setObjectName("entityKindPopup");
    kindCombo_->setView(kindView);
    fields->addWidget(kindCombo_);
    fields->addSpacing(7);

    fields->addWidget(fieldLabel("名称", form));
    nameEdit_ = new QLineEdit(form);
    nameEdit_->setObjectName("entityDialogLineEdit");
    nameEdit_->setPlaceholderText("输入人物、地点、组织或物件的名称");
    fields->addWidget(nameEdit_);

    roleField_ = new QWidget(form);
    auto *roleLayout = new QVBoxLayout(roleField_);
    roleLayout->setContentsMargins(0, 7, 0, 0);
    roleLayout->setSpacing(9);
    roleLayout->addWidget(fieldLabel("角色定位（选填）", roleField_));
    roleEdit_ = new QLineEdit(roleField_);
    roleEdit_->setObjectName("entityDialogLineEdit");
    roleEdit_->setPlaceholderText("例如：主角、配角、反派");
    roleLayout->addWidget(roleEdit_);
    fields->addWidget(roleField_);
    fields->addSpacing(7);

    fields->addWidget(fieldLabel("简介（选填）", form));
    summaryEdit_ = new QPlainTextEdit(form);
    summaryEdit_->setObjectName("entityDialogTextEdit");
    summaryEdit_->setPlaceholderText("用一两句话概括，便于在列表和图谱中快速辨认");
    summaryEdit_->setMinimumHeight(90);
    fields->addWidget(summaryEdit_);
    fields->addSpacing(7);

    backgroundField_ = new QWidget(form);
    auto *backgroundLayout = new QVBoxLayout(backgroundField_);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->setSpacing(9);
    backgroundLayout->addWidget(fieldLabel("人物背景（选填）", backgroundField_));
    backgroundEdit_ = new QPlainTextEdit(backgroundField_);
    backgroundEdit_->setObjectName("entityDialogTextEdit");
    backgroundEdit_->setPlaceholderText("例如：成长经历、出身，以及故事开始前的重要事件…");
    backgroundEdit_->setMinimumHeight(120);
    backgroundLayout->addWidget(backgroundEdit_);
    fields->addWidget(backgroundField_);
    fields->addSpacing(7);

    fields->addWidget(fieldLabel("详细设定（选填）", form));
    notesEdit_ = new QPlainTextEdit(form);
    notesEdit_->setObjectName("entityDialogTextEdit");
    notesEdit_->setPlaceholderText("记录其他细节、世界规则或补充资料…");
    notesEdit_->setMinimumHeight(150);
    fields->addWidget(notesEdit_);

    scroll->setWidget(form);
    root->addWidget(scroll, 1);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("entityDialogError");
    errorLabel_->setMinimumHeight(18);
    root->addWidget(errorLabel_);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    actions->addStretch();
    auto *cancelButton = new QPushButton("取消", this);
    cancelButton->setObjectName("entityDialogCancel");
    confirmButton_ = new QPushButton("创建资料", this);
    confirmButton_->setObjectName("entityDialogConfirm");
    confirmButton_->setDefault(true);
    actions->addWidget(cancelButton);
    actions->addWidget(confirmButton_);
    root->addLayout(actions);

    connect(kindCombo_, &QComboBox::currentIndexChanged, this, [this]() {
        const bool isPerson = kind() == QStringLiteral("person");
        roleField_->setVisible(isPerson);
        backgroundField_->setVisible(isPerson);
    });
    connect(nameEdit_, &QLineEdit::textChanged, errorLabel_, &QLabel::clear);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton_, &QPushButton::clicked, this, [this]() {
        if (name().isEmpty()) {
            errorLabel_->setText("请输入资料名称");
            nameEdit_->setFocus();
            return;
        }
        accept();
    });

    applyStyle();
    nameEdit_->setFocus();
}

void CreateEntityDialog::loadEntity(const Entity &entity) {
    setWindowTitle("编辑资料");
    titleLabel_->setText("编辑资料");
    descriptionLabel_->setText("修改这条资料的内容，已有关系会继续关联它。");
    confirmButton_->setText("保存修改");

    int kindIndex = kindCombo_->findData(entity.kind);
    if (kindIndex < 0) {
        kindCombo_->addItem(entity.kind.isEmpty() ? QStringLiteral("未分类") : entity.kind,
                            entity.kind);
        kindIndex = kindCombo_->count() - 1;
    }
    kindCombo_->setCurrentIndex(kindIndex);
    nameEdit_->setText(entity.name);
    roleEdit_->setText(entity.role);
    summaryEdit_->setPlainText(entity.summary);
    backgroundEdit_->setPlainText(entity.background);
    notesEdit_->setPlainText(entity.notes);
    roleField_->setVisible(kind() == QStringLiteral("person"));
    backgroundField_->setVisible(kind() == QStringLiteral("person"));
    errorLabel_->clear();
}

QString CreateEntityDialog::kind() const { return kindCombo_->currentData().toString(); }
QString CreateEntityDialog::name() const { return nameEdit_->text().trimmed(); }
QString CreateEntityDialog::role() const {
    return kind() == QStringLiteral("person") ? roleEdit_->text().trimmed() : QString{};
}
QString CreateEntityDialog::summary() const { return summaryEdit_->toPlainText(); }
QString CreateEntityDialog::background() const {
    return kind() == QStringLiteral("person") ? backgroundEdit_->toPlainText() : QString{};
}
QString CreateEntityDialog::notes() const { return notesEdit_->toPlainText(); }

void CreateEntityDialog::applyStyle() {
    setStyleSheet(R"QSS(
        QDialog#createEntityDialog { background-color: #1c1e22; color: #edf0f4; }
        QWidget { font-family: "Microsoft YaHei UI"; font-size: 13px; }
        QWidget#entityDialogForm, QScrollArea#entityDialogScroll {
            background: transparent;
            border: none;
        }
        QLabel { color: #a4aebe; background: transparent; }
        QLabel#entityDialogTitle { color: #edf0f4; font-size: 21px; font-weight: bold; }
        QLabel#entityDialogDescription { color: #929eaf; font-size: 12px; }
        QLabel#entityFieldLabel { color: #dce1e8; font-weight: bold; }
        QLabel#entityDialogError { color: #f38e9d; font-size: 12px; }
        QLineEdit#entityDialogLineEdit, QPlainTextEdit#entityDialogTextEdit,
        QComboBox#entityKindCombo {
            background-color: #202327;
            color: #edf0f4;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 9px 11px;
            selection-background-color: #394567;
        }
        QLineEdit#entityDialogLineEdit:focus, QPlainTextEdit#entityDialogTextEdit:focus,
        QComboBox#entityKindCombo:focus { border-color: #7890cd; }
        QComboBox#entityKindCombo { padding-right: 40px; }
        QComboBox#entityKindCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 34px;
            background-color: #282d36;
            border-left: 1px solid #414751;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
        }
        QComboBox#entityKindCombo::down-arrow {
            image: url(:/icons/chevron-down.xpm);
            width: 12px;
            height: 8px;
        }
        QListView#entityKindPopup {
            background-color: #252a32;
            color: #edf0f4;
            border: 1px solid #566179;
            outline: none;
            selection-background-color: #394567;
        }
        QListView#entityKindPopup::item { padding: 8px 10px; }
        QPushButton {
            background-color: #252a32;
            color: #dce1e8;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 9px 17px;
        }
        QPushButton:hover { background-color: #303642; border-color: #65728e; }
        QPushButton#entityDialogConfirm {
            background-color: #33416b;
            color: #e2e8ff;
            border-color: #566ba1;
            font-weight: bold;
        }
        QPushButton#entityDialogConfirm:hover {
            background-color: #415284;
            border-color: #7a90c7;
        }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 2px 0; }
        QScrollBar::handle:vertical {
            background-color: #515b6c;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background-color: #73809a; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )QSS");
}
