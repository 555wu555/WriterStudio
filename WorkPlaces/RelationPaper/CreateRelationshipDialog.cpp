#include "CreateRelationshipDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString kindText(const QString &kind) {
    if (kind == "person") return "人物";
    if (kind == "place") return "地点";
    if (kind == "organization") return "组织";
    if (kind == "item") return "物件";
    return "资料";
}
}

CreateRelationshipDialog::CreateRelationshipDialog(const QVector<Entity> &entities,
                                                   QWidget *parent)
    : QDialog(parent) {
    setObjectName("relationshipDialog");
    setWindowTitle("新建关系");
    resize(560, 610);
    setMinimumWidth(470);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(10);

    titleLabel_ = new QLabel("新建关系", this);
    titleLabel_->setObjectName("relationshipTitle");
    hintLabel_ = new QLabel(
        "箭头从称呼者指向对方。例如 A → B 填“母亲”，表示 A 称 B 为母亲；反方向可填“儿子”。",
        this);
    hintLabel_->setObjectName("relationshipHint");
    hintLabel_->setWordWrap(true);
    root->addWidget(titleLabel_);
    root->addWidget(hintLabel_);

    auto *endpoints = new QFormLayout;
    endpoints->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    endpoints->setHorizontalSpacing(16);
    endpoints->setVerticalSpacing(12);
    fromCombo_ = new QComboBox(this);
    toCombo_ = new QComboBox(this);
    fromCombo_->setObjectName("relationshipCombo");
    toCombo_->setObjectName("relationshipCombo");
    fromCombo_->setView(new QListView(fromCombo_));
    toCombo_->setView(new QListView(toCombo_));
    for (const Entity &entity : entities) {
        const QString name = entity.name.trimmed().isEmpty()
            ? "未命名资料" : entity.name;
        const QString display = QString("%1 · %2").arg(kindText(entity.kind), name);
        fromCombo_->addItem(display, entity.id);
        toCombo_->addItem(display, entity.id);
        fromCombo_->setItemData(fromCombo_->count() - 1, name, Qt::UserRole + 1);
        toCombo_->setItemData(toCombo_->count() - 1, name, Qt::UserRole + 1);
    }
    if (toCombo_->count() > 1) toCombo_->setCurrentIndex(1);
    auto *fromTitle = new QLabel("资料 A", this);
    auto *toTitle = new QLabel("资料 B", this);
    fromTitle->setObjectName("relationshipFieldLabel");
    toTitle->setObjectName("relationshipFieldLabel");
    endpoints->addRow(fromTitle, fromCombo_);
    endpoints->addRow(toTitle, toCombo_);
    root->addLayout(endpoints);

    auto *forwardCard = new QFrame(this);
    forwardCard->setObjectName("relationshipDirectionCard");
    auto *forwardLayout = new QVBoxLayout(forwardCard);
    forwardLayout->setContentsMargins(16, 13, 16, 15);
    forwardLayout->setSpacing(7);
    forwardLabel_ = new QLabel(forwardCard);
    forwardLabel_->setObjectName("relationshipDirectionTitle");
    forwardLabel_->setWordWrap(true);
    forwardHelpLabel_ = new QLabel(forwardCard);
    forwardHelpLabel_->setObjectName("relationshipDirectionHelp");
    forwardHelpLabel_->setWordWrap(true);
    forwardEdit_ = new QLineEdit(forwardCard);
    forwardEdit_->setObjectName("relationshipEdit");
    forwardEdit_->setPlaceholderText("例如：母亲、朋友、工作地点");
    forwardLayout->addWidget(forwardLabel_);
    forwardLayout->addWidget(forwardHelpLabel_);
    forwardLayout->addWidget(forwardEdit_);
    root->addWidget(forwardCard);

    auto *reverseCard = new QFrame(this);
    reverseCard->setObjectName("relationshipDirectionCard");
    auto *reverseLayout = new QVBoxLayout(reverseCard);
    reverseLayout->setContentsMargins(16, 13, 16, 15);
    reverseLayout->setSpacing(7);
    reverseLabel_ = new QLabel(reverseCard);
    reverseLabel_->setObjectName("relationshipDirectionTitle");
    reverseLabel_->setWordWrap(true);
    reverseHelpLabel_ = new QLabel(reverseCard);
    reverseHelpLabel_->setObjectName("relationshipDirectionHelp");
    reverseHelpLabel_->setWordWrap(true);
    sameLabelCheck_ = new QCheckBox("双方使用相同称呼（如“朋友”）", reverseCard);
    sameLabelCheck_->setObjectName("relationshipSameCheck");
    sameLabelCheck_->setCursor(Qt::PointingHandCursor);
    reverseEdit_ = new QLineEdit(reverseCard);
    reverseEdit_->setObjectName("relationshipEdit");
    reverseEdit_->setPlaceholderText("例如：儿子；留空表示暂未定义");
    reverseLayout->addWidget(reverseLabel_);
    reverseLayout->addWidget(reverseHelpLabel_);
    reverseLayout->addWidget(sameLabelCheck_);
    reverseLayout->addWidget(reverseEdit_);
    root->addWidget(reverseCard);

    auto *notesTitle = new QLabel("补充说明（选填）", this);
    notesTitle->setObjectName("relationshipFieldLabel");
    notesEdit_ = new QPlainTextEdit(this);
    notesEdit_->setObjectName("relationshipNotes");
    notesEdit_->setPlaceholderText("记录关系的背景或变化…");
    notesEdit_->setMinimumHeight(68);
    root->addWidget(notesTitle);
    root->addWidget(notesEdit_, 1);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("relationshipError");
    errorLabel_->setMinimumHeight(18);
    root->addWidget(errorLabel_);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    auto *cancelButton = new QPushButton("取消", this);
    cancelButton->setObjectName("relationshipCancel");
    confirmButton_ = new QPushButton("创建关系", this);
    confirmButton_->setObjectName("relationshipConfirm");
    confirmButton_->setDefault(true);
    actions->addWidget(cancelButton);
    actions->addWidget(confirmButton_);
    root->addLayout(actions);

    connect(fromCombo_, &QComboBox::currentIndexChanged,
            this, &CreateRelationshipDialog::updateForm);
    connect(toCombo_, &QComboBox::currentIndexChanged,
            this, &CreateRelationshipDialog::updateForm);
    connect(forwardEdit_, &QLineEdit::textChanged,
            this, [this]() {
        if (sameLabelCheck_->isChecked())
            reverseEdit_->setText(forwardEdit_->text());
        updateForm();
    });
    connect(sameLabelCheck_, &QCheckBox::toggled,
            this, [this](bool checked) {
        reverseEdit_->setEnabled(!checked);
        if (checked) reverseEdit_->setText(forwardEdit_->text());
        updateForm();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton_, &QPushButton::clicked, this, &QDialog::accept);

    applyStyle();
    updateForm();
}

bool CreateRelationshipDialog::setRelationshipForEditing(
    const Relationship &relationship) {
    const int fromIndex = fromCombo_->findData(relationship.fromId);
    const int toIndex = toCombo_->findData(relationship.toId);
    if (fromIndex < 0 || toIndex < 0) return false;

    setWindowTitle("编辑关系");
    titleLabel_->setText("编辑关系");
    hintLabel_->setText("箭头从称呼者指向对方。可修改双方的称呼或关系，两端资料保持不变。");
    confirmButton_->setText("保存修改");
    fromCombo_->setCurrentIndex(fromIndex);
    toCombo_->setCurrentIndex(toIndex);
    fromCombo_->setEnabled(false);
    toCombo_->setEnabled(false);
    forwardEdit_->setText(relationship.label);
    reverseEdit_->setText(relationship.reverseLabel);
    sameLabelCheck_->setChecked(!relationship.reverseLabel.isEmpty() &&
                                relationship.reverseLabel == relationship.label);
    notesEdit_->setPlainText(relationship.notes);
    updateForm();
    return true;
}

QString CreateRelationshipDialog::fromId() const {
    return fromCombo_->currentData().toString();
}
QString CreateRelationshipDialog::toId() const {
    return toCombo_->currentData().toString();
}
QString CreateRelationshipDialog::label() const {
    return forwardEdit_->text().trimmed();
}
QString CreateRelationshipDialog::reverseLabel() const {
    return sameLabelCheck_->isChecked() ? label() : reverseEdit_->text().trimmed();
}
QString CreateRelationshipDialog::notes() const {
    return notesEdit_->toPlainText();
}

void CreateRelationshipDialog::updateForm() {
    const QString fromName = fromCombo_->currentData(Qt::UserRole + 1).toString();
    const QString toName = toCombo_->currentData(Qt::UserRole + 1).toString();
    forwardLabel_->setText(QString("①  %1 → %2").arg(fromName, toName));
    reverseLabel_->setText(QString("②  %1 → %2").arg(toName, fromName));
    forwardHelpLabel_->setText(QString("%1 对 %2 的称呼或关系（必填）")
                                   .arg(fromName, toName));
    reverseHelpLabel_->setText(QString("%1 对 %2 的称呼或关系（选填）")
                                   .arg(toName, fromName));
    if (fromId().isEmpty() || toId().isEmpty()) {
        errorLabel_->setText("请先创建至少两条资料");
    } else if (fromId() == toId()) {
        errorLabel_->setText("请选择两条不同的资料");
    } else {
        errorLabel_->clear();
    }
    confirmButton_->setEnabled(errorLabel_->text().isEmpty() && !label().isEmpty());
}

void CreateRelationshipDialog::applyStyle() {
    setStyleSheet(R"QSS(
        QDialog#relationshipDialog {
            background-color: #1c1e22;
            color: #edf0f4;
            font-family: "Microsoft YaHei UI";
        }
        QLabel { color: #a4aebe; background: transparent; }
        QLabel#relationshipTitle {
            color: #edf0f4;
            font-size: 21px;
            font-weight: bold;
        }
        QLabel#relationshipHint { color: #929eaf; font-size: 12px; }
        QLabel#relationshipFieldLabel {
            color: #dce1e8;
            font-size: 12px;
            font-weight: bold;
        }
        QFrame#relationshipDirectionCard {
            background-color: #222831;
            border: 1px solid #3c485c;
            border-radius: 10px;
        }
        QLabel#relationshipDirectionTitle {
            color: #dce6ff;
            font-size: 14px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#relationshipDirectionHelp {
            color: #aab7ca;
            font-size: 12px;
            background: transparent;
        }
        QLabel#relationshipError { color: #f38e9d; font-size: 12px; }
        QComboBox#relationshipCombo,
        QLineEdit#relationshipEdit,
        QPlainTextEdit#relationshipNotes {
            background-color: #202327;
            color: #edf0f4;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 8px 11px;
            selection-background-color: #394567;
        }
        QComboBox#relationshipCombo:focus,
        QLineEdit#relationshipEdit:focus,
        QPlainTextEdit#relationshipNotes:focus { border-color: #7890cd; }
        QComboBox#relationshipCombo:disabled {
            background-color: #282c34;
            color: #aeb8c8;
            border-color: #3c4553;
        }
        QComboBox#relationshipCombo { padding-right: 32px; }
        QComboBox#relationshipCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border: none;
        }
        QComboBox#relationshipCombo::down-arrow {
            image: url(:/icons/chevron-down.xpm);
            width: 12px;
            height: 8px;
        }
        QComboBox#relationshipCombo QAbstractItemView {
            background-color: #252a32;
            color: #edf0f4;
            border: 1px solid #566179;
            selection-background-color: #394567;
        }
        QCheckBox#relationshipSameCheck {
            color: #cbd5e5;
            spacing: 10px;
            padding: 5px 0;
            font-size: 12px;
        }
        QCheckBox#relationshipSameCheck::indicator {
            width: 18px;
            height: 18px;
            background-color: #202327;
            border: 1px solid #687384;
            border-radius: 5px;
        }
        QCheckBox#relationshipSameCheck::indicator:unchecked:hover {
            background-color: #292f3a;
            border-color: #8799c7;
        }
        QCheckBox#relationshipSameCheck::indicator:checked {
            background-color: #4d6196;
            border-color: #8298d1;
            image: url(:/icons/checkmark.xpm);
        }
        QCheckBox#relationshipSameCheck::indicator:checked:hover {
            background-color: #5b71aa;
            border-color: #a0b3e0;
        }
        QLineEdit#relationshipEdit:disabled {
            background-color: #282c34;
            color: #8e99a9;
        }
        QPushButton {
            background-color: #252a32;
            color: #dce1e8;
            border: 1px solid #414751;
            border-radius: 7px;
            padding: 9px 17px;
        }
        QPushButton:hover { background-color: #303642; border-color: #65728e; }
        QPushButton#relationshipConfirm {
            background-color: #33416b;
            color: #e2e8ff;
            border-color: #566ba1;
            font-weight: bold;
        }
        QPushButton#relationshipConfirm:hover {
            background-color: #415284;
            border-color: #7a90c7;
        }
        QPushButton#relationshipConfirm:disabled {
            background-color: #2b2f38;
            color: #778092;
            border-color: #3b414c;
        }
    )QSS");
}
