#include "ModelDebugDialog.h"
#include "../Generation/ModelRequestLog.h"
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
QString readableJson(const QByteArray &raw) {
    if (raw.isEmpty()) return QStringLiteral("（尚未收到响应）");
    const QJsonDocument document = QJsonDocument::fromJson(raw);
    if (document.isNull()) return QString::fromUtf8(raw);
    return QString::fromUtf8(document.toJson(QJsonDocument::Indented));
}

QPlainTextEdit *readOnlyView(QWidget *parent) {
    auto *view = new QPlainTextEdit(parent);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return view;
}
}

ModelDebugDialog::ModelDebugDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("模型请求记录");
    setObjectName("modelDebugDialog");
    setMinimumSize(760, 500);
    resize(1050, 690);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);
    auto *title = new QLabel("模型请求记录", this);
    title->setObjectName("debugTitle");
    auto *note = new QLabel(
        "保留本次运行最近 30 次请求（含重试）。提示词和完整 JSON 只在内存中，关闭程序后清空。",
        this);
    note->setObjectName("debugNote");
    note->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(note);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    entryList_ = new QListWidget(splitter);
    entryList_->setObjectName("debugList");
    entryList_->setMinimumWidth(230);
    tabs_ = new QTabWidget(splitter);
    tabs_->setObjectName("debugTabs");
    promptView_ = readOnlyView(tabs_);
    requestView_ = readOnlyView(tabs_);
    responseView_ = readOnlyView(tabs_);
    tabs_->addTab(promptView_, "提示词");
    tabs_->addTab(requestView_, "请求 JSON");
    tabs_->addTab(responseView_, "返回 JSON");
    splitter->addWidget(entryList_);
    splitter->addWidget(tabs_);
    splitter->setSizes({260, 750});
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    auto *actions = new QHBoxLayout;
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("debugStatus");
    auto *clearButton = new QPushButton("清空记录", this);
    auto *copyButton = new QPushButton("复制当前页", this);
    auto *closeButton = new QPushButton("关闭", this);
    actions->addWidget(statusLabel_, 1);
    actions->addWidget(clearButton);
    actions->addWidget(copyButton);
    actions->addWidget(closeButton);
    root->addLayout(actions);

    connect(&ModelRequestLog::instance(), &ModelRequestLog::changed,
            this, &ModelDebugDialog::refreshEntries);
    connect(entryList_, &QListWidget::currentItemChanged,
            this, [this] { showSelectedEntry(); });
    connect(clearButton, &QPushButton::clicked, this,
            [] { ModelRequestLog::instance().clear(); });
    connect(copyButton, &QPushButton::clicked, this, [this] {
        auto *view = qobject_cast<QPlainTextEdit *>(tabs_->currentWidget());
        if (view) QApplication::clipboard()->setText(view->toPlainText());
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    refreshEntries();

    setStyleSheet(R"QSS(
        QDialog#modelDebugDialog { background: #1c1e22; color: #edf0f4;
            font-family: "Microsoft YaHei UI"; }
        QLabel#debugTitle { color: #edf0f4; font-size: 20px; font-weight: bold; }
        QLabel#debugNote, QLabel#debugStatus { color: #9aa5b4; font-size: 11px; }
        QListWidget#debugList, QPlainTextEdit {
            background: #202327; color: #e2e7ef; border: 1px solid #3b424d;
            border-radius: 7px; padding: 8px; selection-background-color: #354365;
        }
        QListWidget#debugList::item { padding: 9px 7px; border-radius: 5px; }
        QListWidget#debugList::item:selected { background: #354365; color: #eef1ff; }
        QTabWidget::pane { border: 1px solid #3b424d; border-radius: 6px; }
        QTabBar::tab { background: #252a31; color: #aeb9c8; padding: 8px 16px;
            border: 1px solid #3b424d; }
        QTabBar::tab:selected { background: #354365; color: #edf0f4; }
        QPushButton { background: #2b3039; color: #dce3ee; border: 1px solid #46505f;
            border-radius: 6px; padding: 8px 13px; }
        QPushButton:hover { background: #354051; border-color: #7384ba; }
        QSplitter::handle { background: #343b45; width: 5px; }
        QScrollBar:vertical { background: #202327; width: 9px; margin: 2px; }
        QScrollBar:horizontal { background: #202327; height: 9px; margin: 2px; }
        QScrollBar::handle { background: #505a68; border-radius: 4px; min-height: 26px;
            min-width: 26px; }
        QScrollBar::handle:hover { background: #718097; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: none; }
    )QSS");
}

void ModelDebugDialog::refreshEntries() {
    const quint64 selectedId = entryList_->currentItem()
        ? entryList_->currentItem()->data(Qt::UserRole).toULongLong() : 0;
    {
        const QSignalBlocker blocker(entryList_);
        entryList_->clear();
        for (const ModelRequestEntry &entry : ModelRequestLog::instance().entries()) {
            const QString text = QStringLiteral("%1  %2\n%3")
                .arg(entry.startedAt.toString("HH:mm:ss"),
                     entry.modelName, entry.status);
            auto *item = new QListWidgetItem(text, entryList_);
            item->setData(Qt::UserRole, QVariant::fromValue(entry.id));
        }
        int row = 0;
        if (selectedId != 0) {
            for (int i = 0; i < entryList_->count(); ++i) {
                if (entryList_->item(i)->data(Qt::UserRole).toULongLong() == selectedId) {
                    row = i;
                    break;
                }
            }
        }
        if (entryList_->count() > 0) entryList_->setCurrentRow(row);
    }
    statusLabel_->setText(QStringLiteral("%1 条记录")
        .arg(ModelRequestLog::instance().entries().size()));
    showSelectedEntry();
}

void ModelDebugDialog::showSelectedEntry() {
    const quint64 selectedId = entryList_->currentItem()
        ? entryList_->currentItem()->data(Qt::UserRole).toULongLong() : 0;
    for (const ModelRequestEntry &entry : ModelRequestLog::instance().entries()) {
        if (entry.id != selectedId) continue;
        promptView_->setPlainText(entry.prompt);
        requestView_->setPlainText(readableJson(entry.requestJson));
        responseView_->setPlainText(readableJson(entry.responseJson));
        return;
    }
    promptView_->clear();
    requestView_->clear();
    responseView_->clear();
}
