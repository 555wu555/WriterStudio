//
// Created by shijie on 2026/10/3.
//

#include "Writer.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QFrame>
#include <QDir>
#include "../../storage/ProjectStore.h"
#include "../../storage/StoragePaths.h"
#include "../../storage/Chapter/ChapterStore.h"
#include "../../storage/EntityStore/EntityStore.h"
#include "../../storage/SetUp/SetupStore.h"
#include "../../storage/GlobalWritingConventionStore.h"
#include <QUuid>
#include <QFile>
#include <QMessageBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QStyle>
#include <QVariant>
#include <QTimer>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QSaveFile>
#include <QShortcut>
#include <QFont>
#include <QWheelEvent>
#include <QFileInfo>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBlock>
#include <QScrollArea>
#include <QScrollBar>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QPainter>
#include <QTextLayout>
#include <QTextFrame>
#include <QHash>
#include <QDebug>
#include "../../storage/DocumentAbout/DocumentImporter.h"
#include "../ContextBuilder/ContextBuilder.h"
#include "../Generation/InlineSuggestionFilter.h"
#include "../Generation/InlinePrompt.h"
#include "../Generation/RoutedGenerationService.h"
void ChapterListWidget::startDrag(Qt::DropActions supportedActions) {
    QStringList before;
    before.reserve(count());
    for (int row = 0; row < count(); ++row)
        before.append(item(row)->data(Qt::UserRole).toString());

    // 拖动期间不触发章节切换，结束后再统一同步顺序与选中项。
    {
        const QSignalBlocker blocker(this);
        QListWidget::startDrag(supportedActions);
    }

    QStringList after;
    after.reserve(count());
    for (int row = 0; row < count(); ++row)
        after.append(item(row)->data(Qt::UserRole).toString());

    if (before != after) emit chaptersReordered();
}

Writer::Writer(QWidget *parent) :QWidget(parent){
    setUi();


}

void Writer::setInlineSuggestionEnabled(bool enabled) {
    if (inlineSuggestionEnabled_ == enabled) return;
    inlineSuggestionEnabled_ = enabled;
    // 重新开启时等下一次实际输入，不用旧正文立即触发请求。
    resetAutoSuggestion();
}

void Writer::setFocusMode(bool enabled) {
    if (focusMode_ == enabled) return;

    if (enabled) {
        headerSplitterState_ = headerBodySplitter_->saveState();
        chapterSplitterState_ = chapterEditorSplitter_->saveState();
        headerPanel_->hide();
        chapterPanel_->hide();
        referencePanel_->hide();
        titleRow_->hide();
        footerRow_->hide();
    } else {
        headerPanel_->show();
        chapterPanel_->show();
        referencePanel_->setVisible(referenceToggleButton_->isChecked());
        titleRow_->show();
        footerRow_->show();
        headerBodySplitter_->restoreState(headerSplitterState_);
        chapterEditorSplitter_->restoreState(chapterSplitterState_);
    }

    focusMode_ = enabled;
    if (editor->isEnabled()) editor->setFocus(Qt::ShortcutFocusReason);
}

bool Writer::confirmClose() {
    return confirmLeaveChapter();
}

void Writer::updateProjectInfo(const Project &project) {
    if (!hasProject_ || project.id != currentProject_.id) return;

    currentProject_.name = project.name;
    currentProject_.authorName = project.authorName;
    currentProject_.genre = project.genre;
    currentProject_.tags = project.tags;
    currentProject_.synopsis = project.synopsis;
    currentProject_.updatedAt = project.updatedAt;
    projectNameLabel->setText(project.name);
}

bool Writer::openProject(const QString &projectsPath, const QString &projectId, QString &error) {
    autoSaveTimer_->stop();
    resetAutoSuggestion();
    currentChapterId_.clear();
    const QString directory =QDir(projectsPath).filePath(projectId);

    const QString jsonPath = StoragePaths::projectFilePath(directory);

    Project loaded;

    if (!ProjectStore::loadProject(jsonPath, loaded, error)) {
        return false;
    }

    if (loaded.id != projectId) {
        error = "作品 ID 与文件夹不一致";
        return false;
    }
    currentProject_ = loaded;
    projectDirectory_ = directory;
    projectsPath_ = projectsPath;
    newChapterButton->setEnabled(true);
    deleteChapterButton->setEnabled(false);
    exportButton->setEnabled(false);
    bakButton->setEnabled(false);
    importButton->setEnabled(true);
    hasProject_ = true;

    projectNameLabel->setText(currentProject_.name);

    chapterTitleEdit->clear();
    editor->clear();
    wordCountLabel->setText("0 字");
    setSaveStatus(SaveStatus::Neutral,
    currentProject_.chapters.isEmpty()
        ? "尚无章节"
        : "尚未选择章节"
);

    renderChapterList();
    modified_ = false;
    bodyStack->setCurrentIndex(1);
    refreshReferences();
    return true;
}

bool Writer::resumeLastChapter() {
    if (!hasProject_ || currentProject_.chapters.isEmpty()) return true;

    QString chapterId = currentProject_.lastChapterId;
    bool found = false;
    for (const Chapter &chapter : currentProject_.chapters) {
        if (chapter.id == chapterId) {
            found = true;
            break;
        }
    }
    // 旧作品可能没有记录上次章节；此时打开章节列表中的最后一章。
    if (!found) chapterId = currentProject_.chapters.constLast().id;
    if (chapterId == currentChapterId_) return true;
    if (!confirmLeaveChapter()) return false;

    for (int row = 0; row < chapterList->count(); ++row) {
        if (chapterList->item(row)->data(Qt::UserRole).toString() != chapterId)
            continue;
        const QSignalBlocker blocker(chapterList);
        chapterList->setCurrentRow(row);
        break;
    }
    openChapter(chapterId);
    return currentChapterId_ == chapterId;
}

void Writer::clearReferences() {
    referenceData_ = WorldData{};
    if (!referenceList_) return;
    {
        const QSignalBlocker blocker(referenceSearch_);
        referenceSearch_->clear();
    }
    referenceList_->clear();
    referenceName_->setText(QStringLiteral("暂无资料"));
    referenceKind_->clear();
    referenceDetails_->setText(QStringLiteral("当前作品还没有人物或世界资料。"));
    insertReferenceButton_->setEnabled(false);
    mentionList_->hide();
}

void Writer::refreshReferences() {
    if (!hasProject_) {
        clearReferences();
        return;
    }

    WorldData loaded;
    QString entityError;
    const bool entitiesLoaded = EntityStore::load(projectDirectory_, loaded, entityError);
    const QString selectedId = referenceList_->currentItem()
        ? referenceList_->currentItem()->data(Qt::UserRole).toString() : QString();
    referenceData_ = entitiesLoaded ? loaded : WorldData{};
    renderReferences(selectedId);
    if (!entitiesLoaded) {
        referenceName_->setText(QStringLiteral("资料读取失败"));
        referenceDetails_->setText(entityError);
    }
    if (mentionList_->isVisible()) updateMentionSuggestions();
}

void Writer::renderReferences(const QString &preferredId) {
    const QString query = referenceSearch_->text().trimmed();
    referenceList_->clear();
    int selectedRow = -1;
    for (const Entity &entity : referenceData_.entities) {
        if (!entity.name.contains(query, Qt::CaseInsensitive)) continue;
        const QString name = entity.name.trimmed().isEmpty()
            ? QStringLiteral("未命名资料") : entity.name;
        auto *item = new QListWidgetItem(name, referenceList_);
        item->setData(Qt::UserRole, entity.id);
        if (entity.id == preferredId) selectedRow = referenceList_->count() - 1;
    }
    if (referenceList_->count() == 0) {
        showReference(nullptr);
        return;
    }
    referenceList_->setCurrentRow(selectedRow < 0 ? 0 : selectedRow);
}

void Writer::showReference(QListWidgetItem *item) {
    if (!item) {
        referenceName_->setText(QStringLiteral("暂无资料"));
        referenceKind_->clear();
        referenceDetails_->setText(referenceData_.entities.isEmpty()
            ? QStringLiteral("当前作品还没有人物或世界资料。")
            : QStringLiteral("没有匹配的资料。"));
        insertReferenceButton_->setEnabled(false);
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    for (const Entity &entity : referenceData_.entities) {
        if (entity.id != id) continue;
        referenceName_->setText(entity.name.trimmed().isEmpty()
            ? QStringLiteral("未命名资料") : entity.name);
        const QString kind = entity.kind == QStringLiteral("person") ? QStringLiteral("人物")
            : entity.kind == QStringLiteral("place") ? QStringLiteral("地点")
            : entity.kind == QStringLiteral("organization") ? QStringLiteral("组织")
            : entity.kind == QStringLiteral("item") ? QStringLiteral("物件")
            : entity.kind;
        referenceKind_->setText(kind);
        const auto value = [](const QString &text) {
            return text.trimmed().isEmpty() ? QStringLiteral("未填写") : text;
        };
        QStringList details;
        if (entity.kind == QStringLiteral("person"))
            details << QStringLiteral("角色定位\n%1").arg(value(entity.role));
        details << QStringLiteral("简介\n%1").arg(value(entity.summary));
        if (entity.kind == QStringLiteral("person"))
            details << QStringLiteral("人物背景\n%1").arg(value(entity.background));
        details << QStringLiteral("详细设定\n%1").arg(value(entity.notes));
        referenceDetails_->setText(details.join(QStringLiteral("\n\n")));
        insertReferenceButton_->setEnabled(hasActiveChapter());
        return;
    }
    showReference(nullptr);
}

QString Writer::mentionQuery(int *startPosition) const {
    if (startPosition) *startPosition = -1;
    if (!editor || !editor->isEnabled() || !hasActiveChapter()) return {};
    const QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) return {};
    const QTextBlock block = cursor.block();
    const QString before = block.text().left(cursor.position() - block.position());
    const qsizetype at = before.lastIndexOf(QLatin1Char('@'));
    if (at < 0) return {};
    const QString query = before.mid(at + 1);
    if (query.size() > 40) return {};
    for (const QChar ch : query) {
        if (ch.isSpace() || ch == QLatin1Char('@')) return {};
    }
    if (startPosition) *startPosition = block.position() + at;
    return query;
}

void Writer::updateMentionSuggestions() {
    int start = -1;
    const QString query = mentionQuery(&start);
    if (start < 0 || !editor->hasFocus()) {
        mentionList_->hide();
        return;
    }
    mentionList_->clear();
    for (const Entity &entity : referenceData_.entities) {
        if (!entity.name.contains(query, Qt::CaseInsensitive)) continue;
        const QString name = entity.name.trimmed().isEmpty()
            ? QStringLiteral("未命名资料") : entity.name;
        const QString category = entity.kind == QStringLiteral("person") ? QStringLiteral("人物")
            : entity.kind == QStringLiteral("place") ? QStringLiteral("地点")
            : QStringLiteral("设定");
        auto *item = new QListWidgetItem(name + QStringLiteral(" · ") + category, mentionList_);
        item->setData(Qt::UserRole, name);
        if (mentionList_->count() >= 8) break;
    }
    if (mentionList_->count() == 0) {
        auto *empty = new QListWidgetItem(referenceData_.entities.isEmpty() ?
            QStringLiteral("当前作品暂无资料") : QStringLiteral("没有匹配的资料"), mentionList_);
        empty->setFlags(Qt::NoItemFlags);
    }
    if (mentionList_->count() > 0 && mentionList_->item(0)->flags().testFlag(Qt::ItemIsEnabled))
        mentionList_->setCurrentRow(0);
    const QRect cursorRect = editor->cursorRect();
    const int width = qMin(280, editor->viewport()->width());
    const int height = qMin(210, mentionList_->count() * 36 + 8);
    const int x = qBound(0, cursorRect.left(), editor->viewport()->width() - width);
    const int y = cursorRect.bottom() + height <= editor->viewport()->height()
        ? cursorRect.bottom() : qMax(0, cursorRect.top() - height);
    mentionList_->setGeometry(x, y, width, height);
    mentionList_->show();
    mentionList_->raise();
}

void Writer::insertReference(const QString &entityId, bool replaceMention) {
    if (!hasActiveChapter()) return;
    for (const Entity &entity : referenceData_.entities) {
        if (entity.id != entityId) continue;
        const QString name = entity.name.trimmed().isEmpty()
            ? QStringLiteral("未命名资料") : entity.name;
        insertReferenceText(name, replaceMention);
        return;
    }
}

void Writer::insertReferenceText(const QString &text, bool replaceMention) {
    if (!hasActiveChapter() || text.isEmpty()) return;
    QTextCursor cursor = editor->textCursor();
    if (replaceMention) {
        int start = -1;
        mentionQuery(&start);
        if (start < 0) return;
        cursor.setPosition(start);
        cursor.setPosition(editor->textCursor().position(), QTextCursor::KeepAnchor);
    }
    cursor.insertText(text);
    editor->setTextCursor(cursor);
    mentionList_->hide();
    editor->setFocus(Qt::OtherFocusReason);
}

bool Writer::eventFilter(QObject *watched, QEvent *event) {
    if (watched == editor && event->type() == QEvent::FocusOut) {
        resetAutoSuggestion();
        if (mentionList_) mentionList_->hide();
    }
    if (watched == editor && event->type() == QEvent::KeyPress &&
        mentionList_ && mentionList_->isVisible()) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            mentionList_->hide();
            return true;
        }
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
            const int offset = key->key() == Qt::Key_Down ? 1 : -1;
            const int row = qBound(0, mentionList_->currentRow() + offset,
                                   mentionList_->count() - 1);
            mentionList_->setCurrentRow(row);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter ||
            key->key() == Qt::Key_Tab) {
            if (auto *item = mentionList_->currentItem())
                insertReferenceText(item->data(Qt::UserRole).toString(), true);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Writer::markChapterModified() {
    if (!hasProject_|| currentChapterId_.isEmpty()) return;

    modified_ = true;
    saveButton->setEnabled(true);
    setSaveStatus(SaveStatus::Modified, "未保存",
                  "停止输入约 2 秒后自动保存，也可以点击保存。");
    autoSaveTimer_->start();
}

void Writer::invalidateSuggestion() {
    pendingSuggestion_.active = false;
    suggestionService_->cancel();
    editor->clearInlineSuggestion();
    if (modelStatusLabel_) modelStatusLabel_->hide();
}

void Writer::resetAutoSuggestion() {
    ++suggestionScheduleEpoch_;
    suggestionDelayTimer_->stop();
    invalidateSuggestion();
}

void Writer::requestShortSuggestion() {
    if (!inlineSuggestionEnabled_ || !hasActiveChapter() ||
        !editor->isEnabled() || !editor->hasFocus() ||
        editor->isComposing() || editor->hasInlineSuggestion() ||
        mentionList_->isVisible()) return;

    const QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection() ||
        cursor.position() != editor->document()->characterCount() - 1) return;

    const QString text = editor->toPlainText();
    if (text.trimmed().isEmpty()) return;

    const Chapter *activeChapter = nullptr;
    for (const Chapter &chapter : currentProject_.chapters) {
        if (chapter.id == currentChapterId_) {
            activeChapter = &chapter;
            break;
        }
    }
    if (!activeChapter) return;

    storySetup setup;
    QString error;
    if (!SetupStore::load(projectDirectory_, setup, error)) {
        qWarning().noquote() << "短语建议：读取作品设定失败：" << error;
        return;
    }

    WritingConvention globalConvention;
    if (setup.writingConvention().isEmpty() &&
        !GlobalWritingConventionStore::load(globalConvention, error)) {
        qWarning().noquote() << "短语建议：读取全局写作约定失败：" << error;
        return;
    }

    Chapter chapter = *activeChapter;
    const QString editedTitle = chapterTitleEdit->text().trimmed();
    if (!editedTitle.isEmpty()) chapter.title = editedTitle;

    GenerationRequest request;
    request.requestId = ++nextSuggestionRequestId_;
    request.instruction = InlinePrompt::instruction();
    request.context = ContextBuilder::buildInlineSuggestionContext(
        currentProject_, chapter, text, setup, globalConvention);

    pendingSuggestion_ = {request.requestId, currentProject_.id,
                          currentChapterId_, editor->document()->revision(),
                          cursor.position(), true};
    modelStatusLabel_->setText(QStringLiteral("正在生成短语建议…"));
    modelStatusLabel_->setToolTip(QString());
    modelStatusLabel_->show();
    suggestionService_->start(request);
}

bool Writer::confirmLeaveChapter() {
    if (!modified_) {
        return true;
    }

    QMessageBox box(this);
    box.setWindowTitle("未保存的修改");
    box.setText(QString("<%1>有未保存的修改。").arg(projectName()));
    box.setInformativeText("要保存吗？");

    auto * save =box.addButton("保存",QMessageBox::AcceptRole);
    auto *dicard = box.addButton("放弃修改",QMessageBox::DestructiveRole);
    box.addButton("取消",QMessageBox::RejectRole);
    autoSaveTimer_->stop();
    box.exec();

    if (box.clickedButton() == save) {
        return saveCurrentChapter();
    }
    if (box.clickedButton() == dicard) {
        return true;
    }
    if (modified_) autoSaveTimer_->start();
    return false;

}

void Writer::previewChapterBackup() {
    if (!hasProject_ || currentChapterId_.isEmpty()) return;

    // 模态弹窗也会处理定时器事件；预览期间先暂停自动保存。
    const bool timerWasActive = autoSaveTimer_->isActive();
    autoSaveTimer_->stop();

    QString backupText;
    QString error;
    if (!ChapterStore::loadChapterBackup(projectDirectory_, currentChapterId_,
                                        backupText, error)) {
        QMessageBox::warning(this, "无法查看上一版",
                             QString("无法读取该章节的上一版：\n%1").arg(error));
        if (timerWasActive && modified_) autoSaveTimer_->start();
        return;
    }

    QDialog dialog(this);
    dialog.setObjectName("chapterBackupDialog");
    dialog.setWindowTitle("查看上一版");
    dialog.resize(760, 560);
    dialog.setMinimumSize(520, 360);

    auto *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(22, 20, 22, 20);
    dialogLayout->setSpacing(12);

    auto *heading = new QLabel("上一版正文", &dialog);
    heading->setObjectName("chapterBackupHeading");
    auto *description = new QLabel(
        "这是该章节上次保存前的正文。关闭窗口不会修改当前内容。", &dialog);
    description->setObjectName("chapterBackupDescription");
    description->setWordWrap(true);

    auto *preview = new Editor(&dialog);
    preview->setObjectName("chapterBackupPreview");
    preview->setReadOnly(true);
    preview->setPlainText(backupText);

    auto *rebakButton = new QPushButton("恢复", &dialog);
    rebakButton->setObjectName("chapterBackupRebakButton");
    connect(rebakButton,&QPushButton::clicked,&dialog,&QDialog::accept);
    auto *closeButton = new QPushButton("关闭", &dialog);
    closeButton->setObjectName("chapterBackupCloseButton");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    actions->addWidget(closeButton);
    actions->addWidget(rebakButton);
    dialogLayout->addWidget(heading);
    dialogLayout->addWidget(description);
    dialogLayout->addWidget(preview, 1);
    dialogLayout->addLayout(actions);
    dialog.setStyleSheet(R"QSS(
        QDialog#chapterBackupDialog {
            background-color: #202327;
        }
        QLabel#chapterBackupHeading {
            color: #edf0f4;
            font: bold 18px "Microsoft YaHei UI";
        }
        QLabel#chapterBackupDescription {
            color: #aeb7c4;
            font: 13px "Microsoft YaHei UI";
        }
        QTextEdit#chapterBackupPreview {
            background-color: #191c20;
            color: #e2e7ee;
            border: 1px solid #3b424d;
            border-radius: 8px;
            padding: 14px;
            selection-background-color: #43577c;
        }
        QPushButton#chapterBackupCloseButton {
            min-width: 82px;
            padding: 8px 16px;
            color: #e2e7ee;
            background-color: #303958;
            border: 1px solid #55658d;
            border-radius: 7px;
        }
        QPushButton#chapterBackupCloseButton:hover {
            background-color: #394567;
        }
        QScrollBar:vertical {
            background: #202327;
            width: 10px;
            margin: 4px 2px;
        }
        QScrollBar::handle:vertical {
            background: #4b5668;
            border-radius: 4px;
            min-height: 28px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
    )QSS");

    if (dialog.exec() == QDialog::Accepted) {
        if (modified_) {
            const auto choice = QMessageBox::question(
                this, "覆盖未保存的修改",
                "当前未保存的正文将被上一版替换，章节标题保持当前值。\n确定恢复吗？",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (choice != QMessageBox::Yes) {
                if (timerWasActive && modified_) autoSaveTimer_->start();
                return;
            }
        }

        editor->setPlainText(backupText);
        markChapterModified();
        saveCurrentChapter();
    }
    if (timerWasActive && modified_) autoSaveTimer_->start();
}

bool Writer::addChapter(const QString &title, const QString &content) {

    if (!hasProject_) {
        return false;
    }
    const QString chapterTitle = title.trimmed();
    if (chapterTitle.isEmpty()) {
        QMessageBox::warning(this, "创建失败", "章节标题不能为空");
        return false;
    }
    if (!confirmLeaveChapter()) {
        return false;
    }

    Chapter chapter;
    chapter.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    chapter.title = chapterTitle;
    chapter.filePath = StoragePaths::chapterRelativePath(chapter.id);
    chapter.wordCount = 0;
    for (const auto codePoint : content.toUcs4()) {
        if (!QChar::isSpace(codePoint)) ++chapter.wordCount;
    }

    Project candiate = currentProject_;
    candiate.chapters.append(chapter);
    candiate.lastChapterId = chapter.id;
    candiate.updatedAt = QDateTime::currentDateTimeUtc();

    QString error;
    bool ok = ChapterStore::saveChapter(projectDirectory_, chapter.id, content, error);
    if (!ok) {
        setSaveStatus(SaveStatus::Error, "章节创建失败", error);
        QMessageBox::warning(this,"创建失败",error);
        return false;
    }
    ok = ProjectStore::saveProject(candiate,projectsPath_,error);
    if (!ok) {
        const QString newFile =
            QDir(projectDirectory_).filePath(chapter.filePath);
        if (!QFile::remove(newFile)) {
            error += QString("\n新建的章节文件未能清理：\n%1")
                         .arg(newFile);
        }
        setSaveStatus(SaveStatus::Error, "章节创建失败", error);
        QMessageBox::warning(this, "创建失败", error);
        return false;
    }
    currentProject_ = candiate;
    {
        const QSignalBlocker blocker(chapterList);
        renderChapterList();
    }

    modified_ = false;
    setSaveStatus(SaveStatus::Saved, "章节已创建");
    for (int row = 0; row < chapterList->count(); ++row) {
        auto *item = chapterList->item(row);

        if (item->data(Qt::UserRole).toString() == chapter.id) {
            chapterList->setCurrentItem(item);
            break;
        }
    }
    return true;
}

void Writer::setSaveStatus(SaveStatus status, const QString &text,
                           const QString &details) {
    QString state;
    switch (status) {
    case SaveStatus::Neutral:  state = "neutral"; break;
    case SaveStatus::Modified: state = "modified"; break;
    case SaveStatus::Saved:    state = "saved"; break;
    case SaveStatus::Warning:  state = "warning"; break;
    case SaveStatus::Error:    state = "error"; break;
    }

    saveStatusLabel->setText(text);
    const QString description = details.isEmpty()
        ? text : QString("%1\n%2").arg(text, details);
    saveStatusFrame->setToolTip(description);
    saveStatusFrame->setAccessibleDescription(description);

    if (saveStatusFrame->property("state").toString() == state) {
        return;
    }
    saveStatusFrame->setProperty("state", state);

    // 动态属性改变后，刷新外框及两个标签的 QSS，确保圆点和文字同步换色。
    for (QWidget *widget : QList<QWidget *>{saveStatusFrame, saveStatusDot,
                                          saveStatusLabel}) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
}

void Writer::createChapter() {
    addChapter(QStringLiteral("未命名"), QString());
}

void Writer::deleteSelectedChapter() {
    if (!hasProject_ || !chapterList->currentItem()) return;

    const QString chapterId = chapterList->currentItem()->data(Qt::UserRole).toString();
    int chapterRow = -1;
    for (int row = 0; row < currentProject_.chapters.size(); ++row) {
        if (currentProject_.chapters.at(row).id == chapterId) {
            chapterRow = row;
            break;
        }
    }
    if (chapterRow < 0) {
        QMessageBox::warning(this, "删除失败", "未找到选中的章节");
        return;
    }

    const Chapter chapter = currentProject_.chapters.at(chapterRow);
    QMessageBox box(this);
    box.setWindowTitle("删除章节");
    box.setTextFormat(Qt::PlainText);
    box.setText(QString("确定删除「第%1章 · %2」吗？")
                    .arg(chapterRow + 1).arg(chapter.title));
    box.setInformativeText(
        currentChapterId_ == chapterId && modified_
            ? "该章节及正文将被永久删除，当前未保存的修改也会丢失。"
            : "该章节及正文将被永久删除。");
    auto *deleteButton = box.addButton("删除章节", QMessageBox::DestructiveRole);
    auto *cancelButton = box.addButton("取消", QMessageBox::RejectRole);
    deleteButton->setObjectName("deleteChapterConfirmButton");
    box.setDefaultButton(cancelButton);
    box.setEscapeButton(cancelButton);
    box.setStyleSheet(R"QSS(
        QMessageBox { background-color: #25282d; }
        QMessageBox QLabel { color: #edf0f4; font-family: "Microsoft YaHei UI"; }
        QMessageBox QPushButton {
            background-color: #30343c; color: #edf0f4;
            border: 1px solid #596170; border-radius: 6px;
            padding: 7px 16px; min-width: 72px;
        }
        QMessageBox QPushButton:hover { background-color: #3a404a; }
        QMessageBox QPushButton#deleteChapterConfirmButton {
            background-color: #54373b; color: #ffd1d1; border-color: #825057;
        }
        QMessageBox QPushButton#deleteChapterConfirmButton:hover {
            background-color: #684047; border-color: #a4616b;
        }
    )QSS");
    autoSaveTimer_->stop();
    box.exec();
    if (box.clickedButton() != deleteButton) {
        if (modified_) autoSaveTimer_->start();
        return;
    }

    // 删除的不是正在编辑的章节时，先处理当前章节尚未保存的修改。
    if (currentChapterId_ != chapterId && !confirmLeaveChapter()) return;

    Project candidate = currentProject_;
    candidate.chapters.removeAt(chapterRow);
    const int nextRow = candidate.chapters.isEmpty()
        ? -1 : qMin(chapterRow, candidate.chapters.size() - 1);
    const QString nextId = nextRow < 0 ? QString() : candidate.chapters.at(nextRow).id;
    candidate.lastChapterId = nextId;
    candidate.updatedAt = QDateTime::currentDateTimeUtc();

    QString error;
    if (!ProjectStore::saveProject(candidate, projectsPath_, error)) {
        setSaveStatus(SaveStatus::Error, "删除章节失败", error);
        QMessageBox::warning(this, "删除失败", error);
        // 删除其他章节时若已选择“放弃修改”，不要把那份修改重新自动保存。
        if (modified_ && currentChapterId_ == chapterId) autoSaveTimer_->start();
        return;
    }

    const QString chapterPath = QDir(projectDirectory_).filePath(chapter.filePath);
    QFile chapterFile(chapterPath);
    QString removalError;
    if (chapterFile.exists() && !chapterFile.remove()) {
        removalError = chapterFile.errorString();
    }

    currentProject_ = candidate;
    {
        const QSignalBlocker blocker(chapterList);
        renderChapterList();
        if (nextRow >= 0) chapterList->setCurrentRow(nextRow);
    }
    deleteChapterButton->setEnabled(nextRow >= 0);
    openChapter(nextId);

    if (!removalError.isEmpty()) {
        QMessageBox::warning(this, "章节已移出作品",
                             QString("章节已从作品中移除，但正文文件未能删除：\n%1\n%2")
                                 .arg(chapterPath, removalError));
    }
}

void Writer::exportCurrentChapterTxt() {
    if (currentChapterId_.isEmpty()) {
        QMessageBox::critical(this,"无章节","请先创建章节");
        return;
    }
    if (modified_) {
        QMessageBox::critical(this,"未保存修改","请先保存");
        return;
    }

    QString error;
    QString content;
    QString title;
    int chapterNumber = 0;
    for (int row = 0; row < currentProject_.chapters.size(); ++row) {
        const auto &chapter = currentProject_.chapters.at(row);
        if (chapter.id != currentChapterId_) continue;

        chapterNumber = row + 1;
        title = chapter.title;
        if (!ChapterStore::loadChapter(projectDirectory_, chapter.id, content, error)) {
            QMessageBox::critical(this, "读取失败", error);
            return;
        }
        break;
    }
    if (chapterNumber == 0) {
        QMessageBox::warning(this, "导出失败", "未找到当前章节");
        return;
    }

    const QString text = QString("第%1章 · %2\n\n%3")
                             .arg(chapterNumber).arg(title).arg(content);
    QString exportPath = QFileDialog::getSaveFileName(this,"导出章节",QDir::home().filePath(QString("第%1章_%2.txt").arg(chapterNumber).arg(title)),"文本文件 (*.txt)");
      if (exportPath.isEmpty()) return;

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

void Writer::renderChapterList() {
    chapterList->clear();
    chapterSizeLabel->setText(QString("共 %1 章").arg(currentProject_.chapters.size()));
    for (int row = 0; row < currentProject_.chapters.size(); ++row) {
        const auto &chapter = currentProject_.chapters.at(row);

        const QString displayTitle =
            QString("第%1章 · %2")
                .arg(row + 1)
                .arg(chapter.title);

        auto *item = new QListWidgetItem(displayTitle, chapterList);
        item->setData(Qt::UserRole, chapter.id);
    }

}

void Writer::saveChapterOrder() {
    auto restoreList = [this] {
        const QSignalBlocker blocker(chapterList);
        renderChapterList();
        for (int row = 0; row < chapterList->count(); ++row) {
            if (chapterList->item(row)->data(Qt::UserRole).toString()
                == currentChapterId_) {
                chapterList->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
                break;
            }
        }
        deleteChapterButton->setEnabled(chapterList->currentItem() != nullptr);
    };

    if (!hasProject_ || chapterList->count() != currentProject_.chapters.size()) {
        restoreList();
        return;
    }

    QHash<QString, Chapter> chaptersById;
    chaptersById.reserve(currentProject_.chapters.size());
    for (const Chapter &chapter : currentProject_.chapters)
        chaptersById.insert(chapter.id, chapter);

    QList<Chapter> reordered;
    reordered.reserve(chapterList->count());
    for (int row = 0; row < chapterList->count(); ++row) {
        const QString id = chapterList->item(row)->data(Qt::UserRole).toString();
        if (!chaptersById.contains(id)) {
            restoreList();
            return;
        }
        reordered.append(chaptersById.take(id));
    }
    if (!chaptersById.isEmpty()) {
        restoreList();
        return;
    }

    Project candidate = currentProject_;
    candidate.chapters = reordered;
    candidate.updatedAt = QDateTime::currentDateTimeUtc();

    QString error;
    if (!ProjectStore::saveProject(candidate, projectsPath_, error)) {
        restoreList();
        QMessageBox::warning(this, "调整章节顺序失败", error);
        return;
    }

    currentProject_ = candidate;
    const QSignalBlocker blocker(chapterList);
    for (int row = 0; row < chapterList->count(); ++row) {
        auto *listItem = chapterList->item(row);
        const Chapter &chapter = currentProject_.chapters.at(row);
        listItem->setText(QString("第%1章 · %2").arg(row + 1).arg(chapter.title));
        if (chapter.id == currentChapterId_) {
            chapterList->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
            chapterNumberLabel->setText(QString("第%1章 ·").arg(row + 1));
        }
    }
}

void Writer::openChapter(const QString &chapterId) {
    autoSaveTimer_->stop();
    resetAutoSuggestion();
    const QSignalBlocker titleBlocker(chapterTitleEdit);
    const QSignalBlocker editorBlocker(editor);

    modified_ = false;

    currentChapterId_.clear();
    chapterTitleEdit->clear();
    editor->clear();

    chapterTitleEdit->setEnabled(false);
    exportButton->setEnabled(false);
    editor->setEnabled(false);
    insertReferenceButton_->setEnabled(false);
    mentionList_->hide();
    saveButton->setEnabled(false);
    bakButton->setEnabled(false);
    chapterNumberLabel->setText("章节");
    wordCountLabel->setText("0 字");
    setSaveStatus(SaveStatus::Neutral,
    currentProject_.chapters.isEmpty()? "尚无章节"
    : "尚未选择章节"
    );
    if (!hasProject_|| chapterId.isEmpty()) return;

    for (int row = 0; row < currentProject_.chapters.size(); ++row) {
        const auto &chapter = currentProject_.chapters.at(row);
        if (chapter.id != chapterId) continue;

        QString content;
        QString error;
        if (!ChapterStore::loadChapter(projectDirectory_,chapter.id,content,error)) {
            setSaveStatus(SaveStatus::Warning, "章节读取失败", error);
            QMessageBox::warning(this,"打开章节失败",error);
            return;
        }
            currentChapterId_ = chapter.id;
            chapterTitleEdit->setText(chapter.title);
            editor->setPlainText(content);

            chapterTitleEdit->setEnabled(true);
            editor->setEnabled(true);
            insertReferenceButton_->setEnabled(referenceList_->currentItem() != nullptr);
            exportButton->setEnabled(true);
            saveButton->setEnabled(false);
            bakButton->setEnabled(true);
            wordCountLabel->setText(QString("%1 字").arg(chapter.wordCount));
            chapterNumberLabel->setText(QString("第%1章 ·").arg(row + 1));
            setSaveStatus(SaveStatus::Saved, "章节已加载",
                          "已从磁盘读取章节，当前没有未保存的修改。");
            return;


    }
    setSaveStatus(SaveStatus::Warning, "未找到该章节");

}

bool Writer::saveCurrentChapter(bool showErrorDialog) {
    if (!hasProject_ || currentChapterId_.isEmpty()) return false;

    const QString title = chapterTitleEdit->text().trimmed();
    if (title.isEmpty()) {
        setSaveStatus(SaveStatus::Warning, "标题不能为空",
                      "请填写章节标题后保存。");
        if (showErrorDialog)
        QMessageBox::critical(this,"保存失败","章节标题不能为空");
        return false;
    }
    const QString content = editor->toPlainText();
    qint64 count = 0;
    for (const auto codePoint :content.toUcs4() ) {
        if (!QChar::isSpace(codePoint)) count++;
    }

    Project candidate = currentProject_;
    bool found = false;
    for (auto &chapter : candidate.chapters) {
        if (chapter.id==currentChapterId_) {
            chapter.title = title;
            chapter.wordCount = count;
            found = true;
        }
    }
    if (!found) {
        setSaveStatus(SaveStatus::Warning, "未找到当前章节");
        if (showErrorDialog)
        QMessageBox::warning(this,"保存失败","没有找到");
        return false;
    }

    candidate.updatedAt = QDateTime::currentDateTimeUtc();
    candidate.lastChapterId = currentChapterId_;

     QString error;
    bool ok = false;
    ok = ChapterStore::saveChapter(
    projectDirectory_, currentChapterId_, content, error
    );
    if (!ok) {
        setSaveStatus(SaveStatus::Error, "正文保存失败", error);
        if (showErrorDialog)
        QMessageBox::warning(this,"保存失败",error);
        return false;
    }
    ok = ProjectStore::saveProject(candidate, projectsPath_, error);
    if (!ok) {
        setSaveStatus(SaveStatus::Warning, "正文已保存 · 信息未保存",
                      QString("作品信息保存失败，请重试：\n%1").arg(error));
        if (showErrorDialog)
        QMessageBox::warning(
            this,
            "保存未完成",
            QString("正文已写入，但作品信息保存失败：\n%1")
                .arg(error)
        );
        return false;
    }
    currentProject_ = candidate;
    wordCountLabel->setText(QString("%1 字").arg(count));
    setSaveStatus(SaveStatus::Saved, "已保存",
                  "正文和作品信息已全部保存。");

    for (int row = 0; row < chapterList->count(); ++row) {
        auto *item = chapterList->item(row);

        if (item->data(Qt::UserRole).toString() == currentChapterId_) {
            item->setText(
                QString("第%1章 · %2")
                    .arg(row + 1)
                    .arg(title)
            );
            break;
        }
    }
    modified_ = false;
    saveButton->setEnabled(false);
    autoSaveTimer_->stop();
    return true;

}

void Writer::importChapterTxt() {

    auto openPath = QFileDialog::getOpenFileName(this,"导入章节",QDir().homePath(),"文本文件 (*.txt)",nullptr);

    if (openPath.isEmpty()) return;
    QString title = QFileInfo(openPath).completeBaseName();
    QString error;
    QString content;
    if (!DocumentImporter::readTxt(openPath, content,error)) {
        QMessageBox::warning(this,"导入失败",error);
        return;
    }

    // 单章导出的格式是「章节标题\n\n正文」。只检查开头两行，避免误拆正文。
    const qsizetype firstLineEnd = content.indexOf('\n');
    if (firstLineEnd >= 0) {
        const qsizetype secondLineEnd = content.indexOf('\n', firstLineEnd + 1);
        if (secondLineEnd >= 0) {
            QString firstLine = content.left(firstLineEnd);
            if (firstLine.startsWith(QChar::ByteOrderMark)) firstLine.remove(0, 1);

            const QString secondLine = content.mid(
                firstLineEnd + 1, secondLineEnd - firstLineEnd - 1);
            QString parsedTitle;
            if (secondLine.trimmed().isEmpty() &&
                DocumentImporter::parseChapterHeading(firstLine, true, parsedTitle)) {
                title = parsedTitle;
                content.remove(0, secondLineEnd + 1);
            }
        }
    }

    if (!addChapter(title, content)) {
        return;
    }
    QMessageBox::information(this,"导入成功","导入成功!");

}

void Writer::setUi() {
    auto *layout = new QVBoxLayout(this);
    autoSaveTimer_ = new QTimer(this);
    autoSaveTimer_->setSingleShot(true);
    autoSaveTimer_->setInterval(2000);
    suggestionDelayTimer_ = new QTimer(this);
    suggestionDelayTimer_->setSingleShot(true);
    suggestionDelayTimer_->setInterval(1000);
    suggestionService_ = new RoutedGenerationService(this);
    connect(suggestionService_, &GenerationService::availabilityChanged,
            this, &Writer::modelAvailabilityChanged);
    // 纵向分割：顶部工具栏可以收起，写作区保留。
    headerBodySplitter_ = new QSplitter(Qt::Vertical, this);
    headerPanel_ = new QWidget;
    headerPanel_->setObjectName("writerHeaderPanel");
    headerPanel_->setMinimumHeight(0);
    headerPanel_->setMaximumHeight(54);
    auto *headerLayout = new QHBoxLayout(headerPanel_);

    projectNameLabel = new QLabel("写作");
    headerLayout->addWidget(projectNameLabel);
    headerLayout->addStretch();
    importButton = new QPushButton("导入章节");
    exportButton = new QPushButton("导出章节");
    bakButton = new QPushButton("查看上一版");
    saveButton = new QPushButton("保存");
    referenceToggleButton_ = new QPushButton("资料", headerPanel_);
    referenceToggleButton_->setObjectName("writerReferenceToggle");
    referenceToggleButton_->setCheckable(true);
    referenceToggleButton_->setChecked(false);

    headerLayout->addWidget(referenceToggleButton_);
    headerLayout->addWidget(importButton);
    headerLayout->addWidget(exportButton);
    headerLayout->addWidget(bakButton);
    headerLayout->addWidget(saveButton);

    headerBodySplitter_->addWidget(headerPanel_);
    // 写作页内部切换：0 是未打开作品的空页，1 是编辑区。
    bodyStack = new QStackedWidget;
    bodyStack->setObjectName("writerBodyStack");
    bodyStack->setMinimumHeight(460);
    headerBodySplitter_->addWidget(bodyStack);
    layout->addWidget(headerBodySplitter_, 1);

    auto *emptyPage = new QFrame(bodyStack);
    emptyPage->setObjectName("writerEmptyPage");
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(24, 32, 24, 32);
    emptyLayout->setSpacing(16);

    auto *emptyTitle = new QLabel("尚未打开作品", emptyPage);
    emptyTitle->setObjectName("writerEmptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);

    auto *emptyDescription = new QLabel(
        "到作品库选择或新建一部作品，然后点击“继续写作”。", emptyPage);
    emptyDescription->setObjectName("writerEmptyDescription");
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyDescription->setWordWrap(true);

    auto *libraryButton = new QPushButton("前往作品库", emptyPage);
    libraryButton->setObjectName("writerLibraryButton");
    libraryButton->setCursor(Qt::PointingHandCursor);

    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyTitle);
    // 控件铺满可用宽度，文字在控件内居中，换行高度由布局计算。
    emptyLayout->addWidget(emptyDescription);
    emptyLayout->addSpacing(8);
    emptyLayout->addWidget(libraryButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    bodyStack->addWidget(emptyPage);

    connect(libraryButton, &QPushButton::clicked,
            this, &Writer::libraryRequested);

    // 编辑区
    auto *splitter = new QSplitter(Qt::Horizontal, bodyStack);
    chapterEditorSplitter_ = splitter;
    // 左侧列表
    chapterPanel_ = new QFrame(splitter);
    chapterPanel_->setMinimumWidth(0);
    auto * leftLayout = new QVBoxLayout(chapterPanel_);

    auto * leftHeadLayout = new QHBoxLayout();
    chapterSizeLabel = new QLabel("章节");
    chapterSizeLabel->setMinimumWidth(0);
    leftHeadLayout->addWidget(chapterSizeLabel);
    leftHeadLayout->addStretch();
    newChapterButton = new QPushButton("＋新建");
    newChapterButton->setMinimumWidth(0);
    deleteChapterButton = new QPushButton("删除");
    deleteChapterButton->setMinimumWidth(0);
    connect(newChapterButton,&QPushButton::clicked,this,&Writer::createChapter);
    connect(deleteChapterButton, &QPushButton::clicked,
            this, &Writer::deleteSelectedChapter);
    connect(exportButton,&QPushButton::clicked,this,&Writer::exportCurrentChapterTxt);
    chapterList = new ChapterListWidget(chapterPanel_);

    leftHeadLayout->addWidget(newChapterButton);
    leftHeadLayout->addWidget(deleteChapterButton);
    leftLayout->addLayout(leftHeadLayout);
    leftLayout->addWidget(chapterList, 1);

    splitter->addWidget(chapterPanel_);
    // 右侧写作区
    auto *editPanel = new QFrame(splitter);
    auto *editorLayout = new QVBoxLayout(editPanel);
    editPanel->setMinimumWidth(400);

    titleRow_ = new QWidget(editPanel);
    auto * editHeadLayout = new QHBoxLayout(titleRow_);
    chapterNumberLabel = new QLabel("章节");
    chapterTitleEdit = new QLineEdit();
    editHeadLayout->addWidget(chapterNumberLabel);
    editHeadLayout->addWidget(chapterTitleEdit);

    editorLayout->addWidget(titleRow_);

    editor = new Editor(editPanel);
    editor->setMinimumSize(420, 320);
    editor->installEventFilter(this);
    editorLayout->addWidget(editor, 1);
    mentionList_ = new QListWidget(editor->viewport());
    mentionList_->setObjectName("writerMentionList");
    mentionList_->setFocusPolicy(Qt::NoFocus);
    mentionList_->hide();
    chapterTitleEdit->setReadOnly(false);
    editor->setReadOnly(false);
    footerRow_ = new QWidget(editPanel);
    auto *editfootLayout = new QHBoxLayout(footerRow_);
    wordCountLabel = new QLabel("0 字");
    modelStatusLabel_ = new QLabel(footerRow_);
    modelStatusLabel_->setObjectName("writerModelStatus");
    modelStatusLabel_->setAccessibleName("短语建议状态");
    modelStatusLabel_->hide();
    saveStatusFrame = new QFrame(editPanel);
    saveStatusFrame->setObjectName("writerSaveStatusBadge");
    saveStatusFrame->setAccessibleName("章节保存状态");
    saveStatusFrame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    saveStatusFrame->setFixedHeight(30);
    auto *statusLayout = new QHBoxLayout(saveStatusFrame);
    statusLayout->setContentsMargins(10, 4, 10, 4);
    statusLayout->setSpacing(7);

    saveStatusDot = new QLabel(saveStatusFrame);
    saveStatusDot->setObjectName("writerSaveStatusDot");
    saveStatusDot->setFixedSize(10, 10);
    saveStatusDot->setAccessibleName("保存状态指示灯");
    saveStatusLabel = new QLabel(saveStatusFrame);
    statusLayout->addWidget(saveStatusDot, 0, Qt::AlignVCenter);
    statusLayout->addWidget(saveStatusLabel, 0, Qt::AlignVCenter);

    editfootLayout->addWidget(wordCountLabel);
    editfootLayout->addWidget(modelStatusLabel_, 0, Qt::AlignVCenter);
    editfootLayout->addWidget(saveStatusFrame, 0, Qt::AlignVCenter);

    editorLayout->addWidget(footerRow_);
    splitter->addWidget(editPanel);

    referencePanel_ = new QFrame(splitter);
    referencePanel_->setObjectName("writerReferencePanel");
    referencePanel_->setMinimumWidth(0);
    referencePanel_->setMaximumWidth(380);
    auto *referenceLayout = new QVBoxLayout(referencePanel_);
    referenceLayout->setContentsMargins(14, 14, 14, 14);
    referenceLayout->setSpacing(10);
    auto *referenceHeader = new QHBoxLayout;
    auto *referenceTitle = new QLabel("作品资料", referencePanel_);
    referenceTitle->setObjectName("writerReferenceTitle");
    auto *referenceClose = new QPushButton("×", referencePanel_);
    referenceClose->setObjectName("writerReferenceClose");
    referenceClose->setToolTip("收起资料面板");
    referenceHeader->addWidget(referenceTitle);
    referenceHeader->addStretch();
    referenceHeader->addWidget(referenceClose);
    referenceLayout->addLayout(referenceHeader);
    referenceSearch_ = new QLineEdit(referencePanel_);
    referenceSearch_->setObjectName("writerReferenceSearch");
    referenceSearch_->setPlaceholderText("按名称搜索资料…");
    referenceLayout->addWidget(referenceSearch_);
    referenceList_ = new QListWidget(referencePanel_);
    referenceList_->setObjectName("writerReferenceList");
    referenceList_->setMinimumHeight(100);
    referenceList_->setMaximumHeight(190);
    referenceLayout->addWidget(referenceList_);
    auto *referenceScroll = new QScrollArea(referencePanel_);
    referenceScroll->setObjectName("writerReferenceScroll");
    referenceScroll->setWidgetResizable(true);
    referenceScroll->setFrameShape(QFrame::NoFrame);
    auto *referenceContent = new QWidget;
    auto *referenceContentLayout = new QVBoxLayout(referenceContent);
    referenceContentLayout->setContentsMargins(2, 4, 8, 8);
    referenceContentLayout->setSpacing(10);
    referenceName_ = new QLabel("暂无资料", referenceContent);
    referenceName_->setObjectName("writerReferenceName");
    referenceName_->setWordWrap(true);
    referenceKind_ = new QLabel(referenceContent);
    referenceKind_->setObjectName("writerReferenceKind");
    referenceDetails_ = new QLabel("当前作品还没有人物或世界资料。", referenceContent);
    referenceDetails_->setObjectName("writerReferenceDetails");
    referenceDetails_->setTextFormat(Qt::PlainText);
    referenceDetails_->setWordWrap(true);
    referenceDetails_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    referenceContentLayout->addWidget(referenceName_);
    referenceContentLayout->addWidget(referenceKind_);
    referenceContentLayout->addWidget(referenceDetails_);
    referenceContentLayout->addStretch();
    referenceScroll->setWidget(referenceContent);
    referenceLayout->addWidget(referenceScroll, 1);
    insertReferenceButton_ = new QPushButton("插入资料名", referencePanel_);
    insertReferenceButton_->setObjectName("writerInsertReference");
    insertReferenceButton_->setEnabled(false);
    referenceLayout->addWidget(insertReferenceButton_);
    splitter->addWidget(referencePanel_);
    splitter->setSizes({190, 570, 260});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setCollapsible(0, true);
    splitter->setCollapsible(2, true);
    referencePanel_->hide();
    bodyStack->addWidget(splitter);

    connect(referenceToggleButton_, &QPushButton::toggled, this, [this](bool shown) {
        if (focusMode_) return;
        referencePanel_->setVisible(shown);
        if (shown) chapterEditorSplitter_->setSizes({190, 570, 260});
    });
    connect(splitter, &QSplitter::splitterMoved, this,
            [this, splitter](int, int index) {
                if (index != 2 || focusMode_) return;
                const bool shown = splitter->sizes().value(2) > 0;
                if (referenceToggleButton_->isChecked() == shown) return;
                const QSignalBlocker blocker(referenceToggleButton_);
                referenceToggleButton_->setChecked(shown);
            });
    connect(referenceClose, &QPushButton::clicked, referenceToggleButton_,
            [this] { referenceToggleButton_->setChecked(false); });
    connect(referenceSearch_, &QLineEdit::textChanged, this,
            [this] { renderReferences(); });
    connect(referenceList_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item, QListWidgetItem *) { showReference(item); });
    connect(insertReferenceButton_, &QPushButton::clicked, this, [this] {
        if (auto *item = referenceList_->currentItem())
            insertReference(item->data(Qt::UserRole).toString(), false);
    });
    connect(mentionList_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
                insertReferenceText(item->data(Qt::UserRole).toString(), true);
            });
    connect(editor, &QTextEdit::textChanged, this, &Writer::updateMentionSuggestions);
    connect(editor, &QTextEdit::cursorPositionChanged, this,
            &Writer::updateMentionSuggestions);

    // 读取作品成功后再显示编辑区、启用对应操作。
    bodyStack->setCurrentIndex(0);
    newChapterButton->setEnabled(false);
    deleteChapterButton->setEnabled(false);
    saveButton->setEnabled(false);
    bakButton->setEnabled(false);
    exportButton->setEnabled(false);
    importButton->setEnabled(false);
    chapterTitleEdit->setEnabled(false);
    editor->setEnabled(false);

    setObjectName("writerPage");
    setAttribute(Qt::WA_StyledBackground, true);

    projectNameLabel->setObjectName("writerProjectName");
    exportButton->setObjectName("writerExportButton");
    saveButton->setObjectName("writerSaveButton");

    chapterPanel_->setObjectName("writerChapterPanel");
    chapterSizeLabel->setObjectName("writerChapterCaption");
    newChapterButton->setObjectName("writerNewChapterButton");
    deleteChapterButton->setObjectName("writerDeleteChapterButton");
    chapterList->setObjectName("writerChapterList");

    editPanel->setObjectName("writerEditorPanel");
    chapterNumberLabel->setObjectName("writerTitleCaption");
    chapterTitleEdit->setObjectName("writerChapterTitle");
    editor->setObjectName("writerEditor");
    wordCountLabel->setObjectName("writerWordCount");
    saveStatusLabel->setObjectName("writerSaveStatus");
    splitter->setObjectName("writerSplitter");
    headerBodySplitter_->setObjectName("writerVerticalSplitter");
    headerBodySplitter_->setHandleWidth(8);
    headerBodySplitter_->setStretchFactor(0, 0);
    headerBodySplitter_->setStretchFactor(1, 1);
    headerBodySplitter_->setCollapsible(0, true);
    headerBodySplitter_->setCollapsible(1, false);
    headerBodySplitter_->setSizes({54, 720});
    connect(headerBodySplitter_, &QSplitter::splitterMoved, this,
            [this](int, int index) {
                if (index != 1 || headerBodySplitter_->sizes().value(0) <= 54) return;
                const QSignalBlocker blocker(headerBodySplitter_);
                headerBodySplitter_->setSizes({54, headerBodySplitter_->height()});
            });

    // 页面与面板的留白
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(0);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    leftLayout->setContentsMargins(12, 14, 12, 12);
    leftLayout->setSpacing(12);
    leftHeadLayout->setSpacing(8);

    editorLayout->setContentsMargins(16, 16, 16, 12);
    editorLayout->setSpacing(12);
    editHeadLayout->setContentsMargins(0, 0, 0, 0);
    editHeadLayout->setSpacing(10);
    editfootLayout->setContentsMargins(0, 0, 0, 0);

    // 把保存状态推到右边
    editfootLayout->insertStretch(1, 1);

    // 列表与编辑器
    chapterList->setSpacing(5);
    chapterList->setMouseTracking(true);
    chapterList->setWordWrap(false);
    chapterList->setTextElideMode(Qt::ElideRight);
    chapterList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chapterList->setToolTip("拖动章节可调整顺序");
    chapterList->setSelectionMode(QAbstractItemView::SingleSelection);
    chapterList->setDragDropMode(QAbstractItemView::InternalMove);
    chapterList->setDefaultDropAction(Qt::MoveAction);
    chapterList->setDropIndicatorShown(true);
    connect(chapterList, &ChapterListWidget::chaptersReordered,
            this, &Writer::saveChapterOrder);
    connect(chapterList,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *current,QListWidgetItem *) {
        deleteChapterButton->setEnabled(current != nullptr);
        const QString nextId = current? current->data(Qt::UserRole).toString(): QString();
        if (nextId == currentChapterId_)return;

        if (!confirmLeaveChapter()) {
            const QSignalBlocker blocker(chapterList);
            const QString restoreId = currentChapterId_;

                QTimer::singleShot(0, chapterList, [this, restoreId] {
                    const QSignalBlocker blocker(chapterList);

                    for (int row = 0; row < chapterList->count(); ++row) {
                        if (chapterList->item(row)->data(Qt::UserRole).toString() == restoreId) {
                            chapterList->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
                            break;
                        }
                    }
                });
                return;
        }

        openChapter(nextId);
    });

    chapterTitleEdit->setPlaceholderText("请输入章节标题");
    editor->setPlaceholderText("在这里开始写作……");
    editor->setCursorWidth(2);
    splitter->setHandleWidth(8);

    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveShortcut, &QShortcut::activated, this, [this] {
        if (!currentChapterId_.isEmpty() && modified_)
            saveCurrentChapter();
    });
    connect(bakButton, &QPushButton::clicked,this, &Writer::previewChapterBackup);
    connect(importButton, &QPushButton::clicked, this, &Writer::importChapterTxt);
    uiStyle();
    setSaveStatus(SaveStatus::Neutral, "未打开作品");
    connect(saveButton, &QPushButton::clicked,
        this, [this]() {
        saveCurrentChapter();
    });
    connect(chapterTitleEdit, &QLineEdit::textChanged,
        this, &Writer::markChapterModified);

    connect(editor, &QTextEdit::textChanged,
            this, &Writer::markChapterModified);

    // 文本变化先使旧建议失效，再等光标完成本次移动后开始停顿计时。
    connect(editor, &QTextEdit::textChanged, this, [this] {
        suggestionDelayTimer_->stop();
        invalidateSuggestion();
        if (!inlineSuggestionEnabled_ || !hasActiveChapter() ||
            !editor->hasFocus() || editor->isComposing())
            return;
        const quint64 epoch = ++suggestionScheduleEpoch_;
        const QString chapterId = currentChapterId_;
        const int revision = editor->document()->revision();
        QTimer::singleShot(0, this, [this, epoch, chapterId, revision] {
            if (epoch != suggestionScheduleEpoch_ ||
                chapterId != currentChapterId_ ||
                revision != editor->document()->revision() ||
                !editor->hasFocus() || editor->isComposing()) return;
            suggestionDelayTimer_->start();
        });
    });
    connect(editor, &QTextEdit::cursorPositionChanged, this, [this] {
        suggestionDelayTimer_->stop();
        invalidateSuggestion();
    });
    auto cancelOnScroll = [this] {
        suggestionDelayTimer_->stop();
        invalidateSuggestion();
    };
    // 滚动条数值也会因排版、自动保存状态或调整灰字位置而变化。
    // 只有用户主动滚动时才取消正在等待的模型回复。
    connect(editor, &Editor::userScrolled, this, cancelOnScroll);
    connect(editor->verticalScrollBar(), &QScrollBar::actionTriggered,
            this, cancelOnScroll);
    connect(editor->horizontalScrollBar(), &QScrollBar::actionTriggered,
            this, cancelOnScroll);
    connect(editor, &Editor::suggestionAccepted,
            this, &Writer::resetAutoSuggestion);
    connect(editor, &Editor::suggestionNoLongerVisible, this, [this] {
        if (!hasActiveChapter() || !inlineSuggestionEnabled_) return;
        modelStatusLabel_->setText(QStringLiteral("建议未能显示在当前位置"));
        modelStatusLabel_->setToolTip(
            QStringLiteral("编辑区布局变化后，建议无法放在光标附近。请在正文末尾继续输入以重新请求。"));
        modelStatusLabel_->show();
    });
    connect(editor, &Editor::compositionFinished, this, [this] {
        invalidateSuggestion();
        if (!inlineSuggestionEnabled_ || !hasActiveChapter() ||
            !editor->hasFocus()) return;
        const quint64 epoch = ++suggestionScheduleEpoch_;
        QTimer::singleShot(0, this, [this, epoch] {
            if (epoch == suggestionScheduleEpoch_ && !editor->isComposing() &&
                editor->hasFocus()) suggestionDelayTimer_->start();
        });
    });
    connect(suggestionDelayTimer_, &QTimer::timeout,
            this, &Writer::requestShortSuggestion);
    connect(suggestionService_, &GenerationService::generated,
            this, [this](quint64 requestId, const QString &text) {
        const PendingSuggestion snapshot = pendingSuggestion_;
        if (!snapshot.active || snapshot.requestId != requestId) return;
        pendingSuggestion_.active = false;

        const QTextCursor cursor = editor->textCursor();
        if (!inlineSuggestionEnabled_ || !hasActiveChapter() ||
            !editor->hasFocus() || editor->isComposing() ||
            mentionList_->isVisible() ||
            snapshot.projectId != currentProject_.id ||
            snapshot.chapterId != currentChapterId_ ||
            snapshot.documentRevision != editor->document()->revision() ||
            snapshot.cursorPosition != cursor.position() || cursor.hasSelection())
            return;
        const QString suggestion = InlineSuggestionFilter::clean(text);
        if (suggestion.isEmpty()) {
            modelStatusLabel_->setText(text.isEmpty()
                ? QStringLiteral("模型没有给出合适短语")
                : QStringLiteral("模型回复不符合短语格式"));
            modelStatusLabel_->setToolTip(text.isEmpty()
                ? QStringLiteral("模型返回了空建议；也可能在生成预算内没有写出回复。")
                : QStringLiteral("建议需为单行、20 字以内；最多允许一处句中逗号，不允许其他句中标点。模型原文前 120 字：\n%1")
                      .arg(text.left(120)));
            modelStatusLabel_->show();
            return;
        }
        // 先让底部状态退出布局，再计算灰字位置；否则布局调整可能
        // 触发编辑器重排并立即清掉刚显示的建议。
        modelStatusLabel_->hide();
        editor->showInlineSuggestion(suggestion);
        if (!editor->hasInlineSuggestion()) {
            modelStatusLabel_->setText(QStringLiteral("建议未能显示在当前位置"));
            modelStatusLabel_->setToolTip(QStringLiteral("请确认光标位于正文末尾，并让编辑区底部留出显示空间。"));
            modelStatusLabel_->show();
        }
    });

    connect(suggestionService_, &GenerationService::failed,
            this, [this](quint64 requestId, const QString &error) {
        if (!pendingSuggestion_.active || pendingSuggestion_.requestId != requestId)
            return;
        pendingSuggestion_.active = false;
        if (!inlineSuggestionEnabled_ || !hasActiveChapter() ||
            pendingSuggestion_.projectId != currentProject_.id ||
            pendingSuggestion_.chapterId != currentChapterId_)
            return;
        modelStatusLabel_->setText(QStringLiteral("短语建议失败 · 悬停查看原因"));
        modelStatusLabel_->setToolTip(error);
        modelStatusLabel_->show();
    });
    connect(autoSaveTimer_, &QTimer::timeout,this,[this]() {
        if (!currentChapterId_.isEmpty() && modified_) {
            saveCurrentChapter(false);
        }


    });
}

void Writer::uiStyle()
{
    setStyleSheet(R"QSS(
        /* 页面背景 */
        QWidget#writerPage {
            background-color: #1c1e22;
        }

        QStackedWidget#writerBodyStack {
            background-color: transparent;
            border: none;
        }

        QFrame#writerEmptyPage {
            background-color: #202327;
            border: 1px dashed #3d434d;
            border-radius: 8px;
        }

        /* 基础字体 */
        QLabel, QPushButton, QListWidget,
        QLineEdit {
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
            color: #edf0f4;
        }

        QLabel {
            background-color: transparent;
            border: none;
            color: #a1a9b4;
        }

        /* 顶部作品名 */
        QLabel#writerProjectName {
            color: #edf0f4;
            font-size: 20px;
            font-weight: bold;
            padding: 4px 0;
        }

        QLabel#writerEmptyTitle {
            color: #dce1e8;
            font-size: 20px;
            font-weight: bold;
        }

        QLabel#writerEmptyDescription {
            color: #929dab;
            font-size: 13px;
        }

        QLabel#writerChapterCaption {
            color: #dce1e8;
            font-size: 13px;
            font-weight: bold;
        }

        QPushButton#writerReferenceToggle, QPushButton#writerReferenceClose,
        QPushButton#writerInsertReference {
            background-color: #292f3c;
            color: #d2dcf3;
            border: 1px solid #46526d;
            border-radius: 6px;
            padding: 6px 10px;
        }
        QPushButton#writerReferenceToggle:checked {
            background-color: #35436a;
            border-color: #6679a7;
        }
        QPushButton#writerReferenceClose { padding: 2px 8px; }
        QPushButton#writerReferenceToggle:hover, QPushButton#writerReferenceClose:hover,
        QPushButton#writerInsertReference:hover { background-color: #35405b; }
        QFrame#writerReferencePanel {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 8px;
        }
        QLabel#writerReferenceTitle, QLabel#writerReferenceName {
            color: #edf0f4;
            font-weight: bold;
            font-size: 15px;
        }
        QLabel#writerReferenceKind { color: #aab9e6; }
        QLabel#writerReferenceDetails {
            color: #c9d1dc;
            line-height: 1.5;
        }
        QLineEdit#writerReferenceSearch {
            background-color: #1b1f25;
            color: #edf0f4;
            border: 1px solid #414b5a;
            border-radius: 6px;
            padding: 7px 10px;
        }
        QLineEdit#writerReferenceSearch:focus { border-color: #7083b3; }
        QListWidget#writerReferenceList, QListWidget#writerMentionList {
            background-color: #1b1f25;
            color: #dce4f0;
            border: 1px solid #3d4655;
            border-radius: 6px;
            outline: none;
        }
        QListWidget#writerReferenceList::item, QListWidget#writerMentionList::item {
            padding: 7px 9px;
        }
        QListWidget#writerReferenceList::item:selected,
        QListWidget#writerMentionList::item:selected {
            background-color: #394567;
            color: #edf0f4;
        }
        QScrollArea#writerReferenceScroll, QScrollArea#writerReferenceScroll QWidget {
            background: transparent;
            border: none;
        }

        QLabel#writerTitleCaption {
            color: #929dab;
            font-size: 12px;
        }

        QLabel#writerWordCount {
            color: #929dab;
            font-size: 12px;
            padding: 3px 0;
        }

        QLabel#writerModelStatus {
            color: #e2c58a;
            font-size: 12px;
        }

        /* 保存状态：圆点、文字和浅色背景一起表达状态。 */
        QFrame#writerSaveStatusBadge {
            background-color: #24282f;
            border: 1px solid #3b424d;
            border-radius: 7px;
        }

        QLabel#writerSaveStatus {
            color: #a5aebb;
            font-size: 12px;
            font-weight: 600;
            padding: 0;
        }

        QLabel#writerSaveStatusDot {
            background-color: #7e8897;
            border: none;
            border-radius: 5px;
            padding: 0;
        }

        QFrame#writerSaveStatusBadge[state="modified"],
        QFrame#writerSaveStatusBadge[state="error"] {
            background-color: #38292f;
            border-color: #70424e;
        }

        QFrame#writerSaveStatusBadge[state="modified"] QLabel#writerSaveStatus,
        QFrame#writerSaveStatusBadge[state="error"] QLabel#writerSaveStatus {
            color: #f2a0aa;
        }

        QFrame#writerSaveStatusBadge[state="modified"] QLabel#writerSaveStatusDot,
        QFrame#writerSaveStatusBadge[state="error"] QLabel#writerSaveStatusDot {
            background-color: #f07889;
        }

        QFrame#writerSaveStatusBadge[state="saved"] {
            background-color: #24342d;
            border-color: #3c614c;
        }

        QFrame#writerSaveStatusBadge[state="saved"] QLabel#writerSaveStatus {
            color: #a0d6b4;
        }

        QFrame#writerSaveStatusBadge[state="saved"] QLabel#writerSaveStatusDot {
            background-color: #79c99c;
        }

        QFrame#writerSaveStatusBadge[state="warning"] {
            background-color: #373127;
            border-color: #6a5939;
        }

        QFrame#writerSaveStatusBadge[state="warning"] QLabel#writerSaveStatus {
            color: #e2c58a;
        }

        QFrame#writerSaveStatusBadge[state="warning"] QLabel#writerSaveStatusDot {
            background-color: #dfb968;
        }

        /* 左右面板 */
        QFrame#writerChapterPanel,
        QFrame#writerEditorPanel {
            background-color: #25282d;
            border: 1px solid #373c43;
            border-radius: 8px;
        }

        /* 普通按钮：导出等 */
        QPushButton {
            background-color: #25282d;
            color: #dce1e8;
            border: 1px solid #424852;
            border-radius: 6px;
            padding: 7px 15px;
            min-height: 20px;
        }

        QPushButton:hover {
            background-color: #30343c;
            border-color: #596170;
        }

        QPushButton:pressed {
            background-color: #202327;
            border-color: #424852;
        }

        /* 保存按钮与空页入口 */
        QPushButton#writerSaveButton,
        QPushButton#writerLibraryButton {
            background-color: #303958;
            color: #c2cbff;
            border-color: #4b5887;
            font-weight: bold;
        }

        QPushButton#writerSaveButton:hover,
        QPushButton#writerLibraryButton:hover {
            background-color: #394567;
            border-color: #6575aa;
        }

        QPushButton#writerSaveButton:pressed,
        QPushButton#writerLibraryButton:pressed {
            background-color: #29324b;
            border-color: #46537c;
        }

        /* 章节栏中的小按钮 */
        QPushButton#writerNewChapterButton {
            background-color: #202327;
            color: #b8c3ed;
            border-color: #3e4655;
            font-size: 12px;
            padding: 4px 9px;
            min-height: 18px;
        }

        QPushButton#writerNewChapterButton:hover {
            background-color: #303958;
            color: #d1d8ff;
            border-color: #53618c;
        }

        QPushButton#writerNewChapterButton:pressed {
            background-color: #29324b;
            border-color: #46537c;
        }

        QPushButton#writerDeleteChapterButton {
            background-color: #202327;
            color: #d7a5a8;
            border-color: #574348;
            font-size: 12px;
            padding: 4px 9px;
            min-height: 18px;
        }

        QPushButton#writerDeleteChapterButton:hover {
            background-color: #3b2b30;
            color: #ffd1d1;
            border-color: #825057;
        }

        QPushButton#writerDeleteChapterButton:pressed {
            background-color: #302328;
            border-color: #684047;
        }

        /* 用柔和边框表示键盘焦点 */
        QPushButton:focus,
        QPushButton#writerSaveButton:focus,
        QPushButton#writerLibraryButton:focus,
        QPushButton#writerNewChapterButton:focus,
        QPushButton#writerDeleteChapterButton:focus {
            border-color: #8a96c0;
        }

        QPushButton:disabled,
        QPushButton#writerSaveButton:disabled,
        QPushButton#writerNewChapterButton:disabled,
        QPushButton#writerDeleteChapterButton:disabled {
            background-color: #23262c;
            color: #68717d;
            border-color: #343941;
        }

        /* 章节列表 */
        QListWidget#writerChapterList {
            background-color: #25282d;
            color: #d4dae4;
            border: none;
            padding: 2px;
            outline: none;
            selection-background-color: #303958;
            selection-color: #c2cbff;
        }

        QListWidget#writerChapterList::item {
            background-color: #202327;
            color: #d4dae4;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 10px 12px;
            min-height: 22px;
        }

        QListWidget#writerChapterList::item:hover {
            background-color: #2d323a;
            border-color: #414956;
        }

        QListWidget#writerChapterList::item:selected {
            background-color: #303958;
            color: #c2cbff;
            border-color: #4b5887;
        }

        QListWidget#writerChapterList::item:selected:hover {
            background-color: #374263;
            border-color: #5c6a99;
        }

        /* 标题和正文的共同设置 */
        QLineEdit#writerChapterTitle,
        QTextEdit#writerEditor {
            background-color: #202327;
            color: #dce1e8;
            border: 1px solid #373c43;
            border-radius: 6px;
            selection-background-color: #3e4968;
            selection-color: #edf0f4;
            placeholder-text-color: #747e8b;
        }

        /* 章节标题 */
        QLineEdit#writerChapterTitle {
            font-size: 17px;
            padding: 9px 12px;
        }

        /* 正文：更大的字号与留白 */
        QTextEdit#writerEditor {
            padding: 16px;
        }

        QLineEdit#writerChapterTitle:hover,
        QTextEdit#writerEditor:hover {
            border-color: #4b5260;
        }

        QLineEdit#writerChapterTitle:focus,
        QTextEdit#writerEditor:focus {
            border-color: #65718e;
        }

        QLineEdit#writerChapterTitle:disabled,
        QTextEdit#writerEditor:disabled {
            background-color: #202327;
            color: #747e8b;
            border-color: #30353d;
            placeholder-text-color: #68717d;
        }

        /* 分割器：细线，鼠标经过时高亮 */
        QSplitter#writerSplitter {
            background-color: transparent;
            border: none;
        }

        QSplitter#writerSplitter::handle:horizontal {
            background-color: #373e49;
            margin: 12px 2px;
            border-radius: 2px;
        }

        QSplitter#writerSplitter::handle:horizontal:hover {
            background-color: #65718e;
        }

        QSplitter#writerSplitter::handle:horizontal:pressed {
            background-color: #8a96c0;
        }

        QSplitter#writerVerticalSplitter {
            background-color: transparent;
            border: none;
        }

        QSplitter#writerVerticalSplitter::handle:vertical {
            background-color: #373e49;
            margin: 2px 18px;
            border-radius: 2px;
        }

        QSplitter#writerVerticalSplitter::handle:vertical:hover {
            background-color: #65718e;
        }

        QSplitter#writerVerticalSplitter::handle:vertical:pressed {
            background-color: #8a96c0;
        }

        /* 滚动条：透明轨道、圆角滑块 */
        QScrollBar {
            background-color: transparent;
            border: none;
        }

        QScrollBar:vertical {
            width: 8px;
            margin: 2px 0;
        }

        QScrollBar:horizontal {
            height: 8px;
            margin: 0 2px;
        }

        QScrollBar::handle {
            background-color: #4b5260;
            border-radius: 4px;
        }

        QScrollBar::handle:vertical {
            min-height: 28px;
        }

        QScrollBar::handle:horizontal {
            min-width: 28px;
        }

        QScrollBar::handle:hover {
            background-color: #657085;
        }

        QScrollBar::handle:pressed {
            background-color: #7d89aa;
        }

        QScrollBar::add-line,
        QScrollBar::sub-line {
            background-color: transparent;
            border: none;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }

        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0;
        }

        QScrollBar::up-arrow,
        QScrollBar::down-arrow,
        QScrollBar::left-arrow,
        QScrollBar::right-arrow {
            width: 0;
            height: 0;
        }

        QScrollBar::add-page,
        QScrollBar::sub-page {
            background-color: transparent;
        }
    )QSS");
}

Editor::Editor(QWidget *parent) : QTextEdit(parent) {
    QFont bodyFont = font();
    bodyFont.setFamily("Microsoft YaHei UI");
    bodyFont.setPointSize(16);
    setFont(bodyFont);
    setAcceptRichText(false);
    setAutoFormatting(QTextEdit::AutoNone);
    setPlainText(QString());

    connect(this, &QTextEdit::textChanged,
            this, &Editor::clearInlineSuggestion);
    connect(this, &QTextEdit::cursorPositionChanged,
            this, &Editor::clearInlineSuggestion);
    connect(verticalScrollBar(), &QScrollBar::actionTriggered,
            this, &Editor::clearInlineSuggestion);
    connect(horizontalScrollBar(), &QScrollBar::actionTriggered,
            this, &Editor::clearInlineSuggestion);
}

void Editor::setPlainText(const QString &text) {
    QTextEdit::setPlainText(text);

    // setPlainText 会重建文档；行距只影响显示，不应成为一次可撤销的编辑。
    const bool undoEnabled = document()->isUndoRedoEnabled();
    document()->setUndoRedoEnabled(false);
    QTextCursor all(document());
    all.select(QTextCursor::Document);
    QTextBlockFormat format;
    format.setLineHeight(140, QTextBlockFormat::ProportionalHeight);
    all.mergeBlockFormat(format);
    // 正文末尾保留几行滚动空间，行内建议换行时也能完整显示。
    QTextFrameFormat frameFormat = document()->rootFrame()->frameFormat();
    frameFormat.setBottomMargin(qMax<qreal>(80, fontMetrics().height() * 3));
    document()->rootFrame()->setFrameFormat(frameFormat);
    // 导入的章节可能含有退格等不可见控制字符。仅对这些字符关闭
    // 字体回退，避免 Qt/DirectWrite 尝试 Fixedsys 等旧位图字体。
    // 字符仍留在文档中，保存时不会被悄悄删改。
    QTextCharFormat controlFormat;
    controlFormat.setFontStyleStrategy(QFont::NoFontMerging);
    const QString displayedText = document()->toPlainText();
    QTextCursor controlCursor(document());
    for (int index = 0; index < displayedText.size(); ++index) {
        const ushort codepoint = displayedText.at(index).unicode();
        const bool legacyControl = codepoint < 0x20 && codepoint != '\n' &&
                                   codepoint != '\t';
        if (!legacyControl && !(codepoint >= 0x7f && codepoint <= 0x9f))
            continue;
        controlCursor.setPosition(index);
        controlCursor.setPosition(index + 1, QTextCursor::KeepAnchor);
        controlCursor.mergeCharFormat(controlFormat);
    }
    document()->setUndoRedoEnabled(undoEnabled);
}

void Editor::showInlineSuggestion(const QString &text) {
    const QTextCursor cursor = textCursor();
    if (text.isEmpty() || composing_ || cursor.hasSelection() ||
        cursor.position() != document()->characterCount() - 1) {
        clearInlineSuggestion();
        return;
    }

    QFont suggestionFont = currentFont();
    if (suggestionFont.pointSizeF() <= 0) suggestionFont = font();
    QTextLayout layout(text, suggestionFont);
    if (!layoutInlineSuggestion(layout, cursorRect(cursor))) {
        // 光标已在正文末尾；先滚到底部给建议腾出显示空间。
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        QTextLayout retryLayout(text, suggestionFont);
        if (!layoutInlineSuggestion(retryLayout, cursorRect(cursor))) {
            clearInlineSuggestion();
            return;
        }
    }
    suggestionText_ = text;
    viewport()->update();
}

void Editor::clearInlineSuggestion() {
    if (suggestionText_.isEmpty()) return;
    suggestionText_.clear();
    viewport()->update();
}

bool Editor::hasInlineSuggestion() const {
    return !suggestionText_.isEmpty();
}

bool Editor::layoutInlineSuggestion(QTextLayout &layout, const QRect &caret) const {
    if (caret.bottom() < 0 || caret.top() >= viewport()->height()) return false;

    const QFontMetricsF metrics(layout.font());
    const qreal rightEdge = viewport()->width() - document()->documentMargin();
    const qreal firstX = caret.right() + 1;
    QTextCursor blockStart = textCursor();
    blockStart.movePosition(QTextCursor::StartOfBlock);
    const qreal nextLineX = cursorRect(blockStart).left();
    if (nextLineX < 0 || nextLineX >= rightEdge) return false;

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    int row = 0;
    qreal top = caret.top();
    qreal lastBottom = top;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        qreal x = row == 0 ? firstX : nextLineX;
        if (row == 0 && rightEdge - x <
            metrics.horizontalAdvance(layout.text().left(1))) {
            x = nextLineX;
            top += caret.height();
        } else if (row > 0) {
            top += caret.height();
        }
        line.setLineWidth(qMax<qreal>(1, rightEdge - x));
        const qreal lineTop = top + (caret.height() - line.height()) / 2;
        line.setPosition(QPointF(x, lineTop));
        lastBottom = lineTop + line.height();
        ++row;
    }
    layout.endLayout();
    return row > 0 && caret.top() >= 0 && lastBottom <= viewport()->height();
}

void Editor::paintEvent(QPaintEvent *event) {
    QTextEdit::paintEvent(event);
    if (suggestionText_.isEmpty()) return;

    const QTextCursor cursor = textCursor();
    const QRect caret = cursorRect(cursor);

    QFont suggestionFont = currentFont();
    if (suggestionFont.pointSizeF() <= 0) suggestionFont = font();
    QTextLayout suggestionLayout(suggestionText_, suggestionFont);
    if (!layoutInlineSuggestion(suggestionLayout, caret)) return;

    QPainter painter(viewport());
    painter.setPen(QColor("#93a0b4"));
    suggestionLayout.draw(&painter, QPointF());
}

void Editor::resizeEvent(QResizeEvent *event) {
    QTextEdit::resizeEvent(event);
    if (suggestionText_.isEmpty()) return;
    // 自动保存状态、底栏文字或窗口尺寸变化也可能调整编辑区。
    // 等 Qt 完成排版后重算灰字位置，不直接丢掉已经收到的建议。
    QTimer::singleShot(0, this, [this] {
        if (suggestionText_.isEmpty()) return;
        const QTextCursor cursor = textCursor();
        QTextLayout layout(suggestionText_, currentFont());
        if (!layoutInlineSuggestion(layout, cursorRect(cursor))) {
            verticalScrollBar()->setValue(verticalScrollBar()->maximum());
            QTextLayout retry(suggestionText_, currentFont());
            if (!layoutInlineSuggestion(retry, cursorRect(cursor))) {
                clearInlineSuggestion();
                emit suggestionNoLongerVisible();
                return;
            }
        }
        viewport()->update();
    });
}

void Editor::inputMethodEvent(QInputMethodEvent *event) {
    const bool wasComposing = composing_;
    if (!event->preeditString().isEmpty()) composing_ = true;
    clearInlineSuggestion();
    QTextEdit::inputMethodEvent(event);
    composing_ = !event->preeditString().isEmpty();
    if (wasComposing && !composing_ && !event->commitString().isEmpty())
        emit compositionFinished();
}

void Editor::wheelEvent(QWheelEvent *event) {
    emit userScrolled();
    clearInlineSuggestion();
    const int delta = event->angleDelta().y();
    if (event->modifiers().testAnyFlag(Qt::ControlModifier)&&delta!=0) {
        const int size = font().pointSize();
        if (delta>0 && size<28) zoomIn();
        else if (delta<0 && size>10) zoomOut();

        event->accept();
        return;
    }

    QTextEdit::wheelEvent(event);
}

void Editor::keyPressEvent(QKeyEvent *event) {
    if (!suggestionText_.isEmpty()) {
        if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
            const QString text = suggestionText_;
            clearInlineSuggestion();
            QTextCursor cursor = textCursor();
            cursor.insertText(text);
            setTextCursor(cursor);
            emit suggestionAccepted();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            clearInlineSuggestion();
            event->accept();
            return;
        }
        clearInlineSuggestion();
    }
    QTextEdit::keyPressEvent(event);
}
