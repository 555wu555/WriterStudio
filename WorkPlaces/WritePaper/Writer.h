//
// Created by shijie on 2026/10/3.
//

#ifndef WRITE_WRITER_H
#define WRITE_WRITER_H
#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QStackedWidget>
#include <QSplitter>
#include <QTimer>
#include <QByteArray>
#include "../../models/Project.h"
#include "../../models/WorldData.h"
#include "../Generation/GenerationService.h"
class QScrollArea;
class QEvent;
class QInputMethodEvent;
class QPaintEvent;
class QResizeEvent;
class QTextLayout;

class ChapterListWidget : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;
signals:
    void chaptersReordered();
protected:
    void startDrag(Qt::DropActions supportedActions) override;
};

class Editor : public QTextEdit {
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override = default;
    void setPlainText(const QString &text);
    void showInlineSuggestion(const QString &text);
    void clearInlineSuggestion();
    bool hasInlineSuggestion() const;
    bool isComposing() const { return composing_; }

signals:
    void suggestionAccepted();
    void compositionFinished();
    void userScrolled();
    void suggestionNoLongerVisible();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
private:
    bool layoutInlineSuggestion(QTextLayout &layout, const QRect &caret) const;
    QString suggestionText_;
    bool composing_ = false;
};


class Writer : public QWidget{
    Q_OBJECT
public:
    explicit Writer(QWidget *parent = nullptr);
    ~Writer() override = default;
    QString projectName() const {return currentProject_.name;}
    QString projectId() const {return currentProject_.id;}
    bool hasActiveChapter() const { return hasProject_ && !currentChapterId_.isEmpty(); }
    void setInlineSuggestionEnabled(bool enabled);
    bool confirmClose();
    void setFocusMode(bool enabled);
    void updateProjectInfo(const Project &project);
    bool openProject(const QString &projectsPath,const QString &projectId,QString &error);
    bool resumeLastChapter();
    void refreshReferences();
    void clearReferences();
signals:
    void libraryRequested();
    void modelAvailabilityChanged(const QString &modelKey, const QString &modelName,
                                  const QString &state, const QString &details);
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    enum class SaveStatus {
        Neutral,
        Modified,
        Saved,
        Warning,
        Error
    };

    QStackedWidget *bodyStack = nullptr;
    QSplitter *headerBodySplitter_ = nullptr;
    QSplitter *chapterEditorSplitter_ = nullptr;
    QWidget *headerPanel_ = nullptr;
    QFrame *chapterPanel_ = nullptr;
    QFrame *referencePanel_ = nullptr;
    QPushButton *referenceToggleButton_ = nullptr;
    QLineEdit *referenceSearch_ = nullptr;
    QListWidget *referenceList_ = nullptr;
    QListWidget *mentionList_ = nullptr;
    QLabel *referenceName_ = nullptr;
    QLabel *referenceKind_ = nullptr;
    QLabel *referenceDetails_ = nullptr;
    QPushButton *insertReferenceButton_ = nullptr;
    WorldData referenceData_;
    QWidget *titleRow_ = nullptr;
    QWidget *footerRow_ = nullptr;
    QByteArray headerSplitterState_;
    QByteArray chapterSplitterState_;
    bool focusMode_ = false;
    bool inlineSuggestionEnabled_ = true;
    QLabel *projectNameLabel = nullptr;
    QPushButton *saveButton = nullptr; //保存按钮
    QPushButton *importButton = nullptr;
    QTimer *autoSaveTimer_ = nullptr; // 定时保存 2s
    QTimer *suggestionDelayTimer_ = nullptr;
    GenerationService *suggestionService_ = nullptr;
    struct PendingSuggestion {
        quint64 requestId = 0;
        QString projectId;
        QString chapterId;
        int documentRevision = 0;
        int cursorPosition = 0;
        bool active = false;
    } pendingSuggestion_;
    quint64 nextSuggestionRequestId_ = 0;
    quint64 suggestionScheduleEpoch_ = 0;
    QPushButton *exportButton = nullptr; // 导出按钮
    QPushButton *bakButton = nullptr;
    QPushButton *newChapterButton = nullptr; //新建按钮
    QPushButton *deleteChapterButton = nullptr; //删除选中的章节
    ChapterListWidget *chapterList = nullptr; //
    QLabel *chapterNumberLabel = nullptr;
    QLineEdit *chapterTitleEdit  = nullptr;
    Editor *editor = nullptr;
    QLabel *chapterSizeLabel = nullptr;
    QLabel *wordCountLabel = nullptr;
    QLabel *modelStatusLabel_ = nullptr;
    QFrame *saveStatusFrame = nullptr;
    QLabel *saveStatusDot = nullptr;
    QLabel *saveStatusLabel = nullptr;
    Project currentProject_;    // 当前的作品
    QString projectsPath_;      // 作品库根目录的绝对路径
    QString projectDirectory_;  // 当前作品目录的绝对路径
    QString currentChapterId_;  // 正在显示的章节 ID
    bool hasProject_ = false;
    bool modified_ = false;  // 是否存在尚未保存的修改

    bool confirmLeaveChapter();
    void previewChapterBackup();
    bool addChapter(const QString &title, const QString &content);
    void setSaveStatus(SaveStatus status, const QString &text,const QString &details = QString());
    void markChapterModified();
    void invalidateSuggestion();
    void resetAutoSuggestion();
    void requestShortSuggestion();
    void createChapter();
    void deleteSelectedChapter();
    void exportCurrentChapterTxt();
    void renderChapterList();
    void renderReferences(const QString &preferredId = QString());
    void showReference(QListWidgetItem *item);
    void insertReference(const QString &entityId, bool replaceMention);
    void insertReferenceText(const QString &text, bool replaceMention);
    void updateMentionSuggestions();
    QString mentionQuery(int *startPosition = nullptr) const;
    void saveChapterOrder();
    void openChapter(const QString &chapterId);
    bool saveCurrentChapter(bool showErrorDialog = true);
    void importChapterTxt();
    void setUi();
    void uiStyle();
};


#endif //WRITE_WRITER_H
