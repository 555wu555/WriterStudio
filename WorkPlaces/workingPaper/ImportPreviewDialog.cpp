#include "ImportPreviewDialog.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

ImportPreviewDialog::ImportPreviewDialog(const ImportedBookHeader &header,
                                         const SplitNovelText &split, QWidget *parent)
    : QDialog(parent), split_(&split) {
    setObjectName("importPreviewDialog");
    setWindowTitle("预览 TXT 导入");
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(780, 560);
    setMinimumSize(560, 400);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel("确认章节划分", this);
    title->setObjectName("importPreviewTitle");
    root->addWidget(title);

    auto *details = new QLabel(
        QString("%1  ·  作者：%2  ·  识别到 %3 章")
            .arg(header.name,
                 header.authorName.isEmpty() ? QStringLiteral("未填写") : header.authorName)
            .arg(split.chapters.size()), this);
    details->setObjectName("importPreviewDetails");
    details->setTextFormat(Qt::PlainText);
    details->setWordWrap(true);
    root->addWidget(details);

    if (!header.synopsis.isEmpty()) {
        auto *synopsis = new QLabel("简介：" + header.synopsis.left(160), this);
        synopsis->setObjectName("importPreviewSynopsis");
        synopsis->setTextFormat(Qt::PlainText);
        synopsis->setWordWrap(true);
        root->addWidget(synopsis);
    }

    if (header.declaredChapterCount >= 0 &&
        header.declaredChapterCount != split.chapters.size()) {
        auto *warning = new QLabel(
            QString("文件头写有 %1 章，实际识别到 %2 章。请检查下方列表，再决定如何导入。")
                .arg(header.declaredChapterCount).arg(split.chapters.size()), this);
        warning->setObjectName("importPreviewWarning");
        warning->setWordWrap(true);
        root->addWidget(warning);
    }

    auto *hint = new QLabel(
        split.chapters.isEmpty()
            ? "没有识别到章节标题，可以将整个 TXT 作为一章导入。"
            : "按文件中的顺序导入；重复标题原样保留。选择章节可查看正文开头。", this);
    hint->setObjectName("importPreviewHint");
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(12);
    chapterList_ = new QListWidget(this);
    chapterList_->setObjectName("importChapterList");
    chapterList_->setUniformItemSizes(true);
    chapterList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    chapterList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chapterList_->verticalScrollBar()->setSingleStep(24);
    for (int i = 0; i < split.chapters.size(); ++i) {
        chapterList_->addItem(QString("%1. %2").arg(i + 1).arg(split.chapters.at(i).title));
    }
    contentRow->addWidget(chapterList_, 2);

    chapterPreview_ = new QPlainTextEdit(this);
    chapterPreview_->setObjectName("importChapterPreview");
    chapterPreview_->setReadOnly(true);
    chapterPreview_->setPlaceholderText("选择左侧章节查看正文开头");
    contentRow->addWidget(chapterPreview_, 3);
    root->addLayout(contentRow, 1);

    connect(chapterList_, &QListWidget::currentRowChanged, this,
            [this](int row) {
        if (row < 0 || row >= split_->chapters.size()) {
            chapterPreview_->clear();
            return;
        }
        const QString &body = split_->chapters.at(row).content;
        chapterPreview_->setPlainText(body.left(1200));
        chapterPreview_->moveCursor(QTextCursor::Start);
    });
    if (!split.chapters.isEmpty()) chapterList_->setCurrentRow(0);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    auto *singleButton = new QPushButton("作为一章导入", this);
    singleButton->setObjectName("importSingleButton");
    auto *cancelButton = new QPushButton("取消", this);
    cancelButton->setObjectName("importCancelButton");
    auto *chaptersButton = new QPushButton("按章节导入", this);
    chaptersButton->setObjectName("importChaptersButton");
    chaptersButton->setEnabled(!split.chapters.isEmpty());
    chaptersButton->setDefault(true);
    actions->addWidget(singleButton);
    actions->addWidget(cancelButton);
    actions->addWidget(chaptersButton);
    root->addLayout(actions);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(singleButton, &QPushButton::clicked, this, [this] {
        mode_ = Mode::SingleChapter;
        accept();
    });
    connect(chaptersButton, &QPushButton::clicked, this, [this] {
        mode_ = Mode::Chapters;
        accept();
    });

    setStyleSheet(R"QSS(
        QDialog#importPreviewDialog { background: #1c1e22; color: #edf0f4; font-family: "Microsoft YaHei UI"; }
        QLabel { color: #cbd1da; background: transparent; }
        QLabel#importPreviewTitle { color: #edf0f4; font-size: 20px; font-weight: bold; }
        QLabel#importPreviewDetails { color: #b9c5d6; font-size: 13px; }
        QLabel#importPreviewSynopsis, QLabel#importPreviewHint { color: #929dab; font-size: 12px; }
        QLabel#importPreviewWarning { color: #e0b769; font-size: 12px; }
        QListWidget#importChapterList, QPlainTextEdit#importChapterPreview {
            background: #202327; color: #dce1e8; border: 1px solid #373c43;
            border-radius: 8px; padding: 8px; font-size: 12px;
        }
        QListWidget#importChapterList { outline: none; }
        QListWidget#importChapterList::item { padding: 5px 6px; border-radius: 5px; }
        QListWidget#importChapterList::item:hover { background: #2c3139; }
        QListWidget#importChapterList::item:selected { background: #303958; color: #d3dbff; }
        QPushButton { background: #2b3039; color: #cbd4e3; border: 1px solid #46505f;
                      border-radius: 6px; padding: 8px 13px; min-width: 72px; }
        QPushButton:hover { background: #354051; border-color: #7384ba; }
        QPushButton:disabled { background: #252930; color: #697382; border-color: #363d47; }
        QPushButton#importChaptersButton { background: #303958; color: #d3dbff; border-color: #56669b; }
        QPushButton#importChaptersButton:hover { background: #3a476c; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 3px 1px; }
        QScrollBar::handle:vertical { background: #4b5360; border-radius: 4px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: #697588; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )QSS");
}
