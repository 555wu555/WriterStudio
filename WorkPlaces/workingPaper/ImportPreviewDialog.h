#ifndef WRITE_IMPORTPREVIEWDIALOG_H
#define WRITE_IMPORTPREVIEWDIALOG_H

#include <QDialog>
#include "../../storage/DocumentAbout/DocumentImporter.h"

class QListWidget;
class QPlainTextEdit;

class ImportPreviewDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { None, Chapters, SingleChapter };

    ImportPreviewDialog(const ImportedBookHeader &header,
                        const SplitNovelText &split, QWidget *parent = nullptr);
    Mode mode() const { return mode_; }

private:
    Mode mode_ = Mode::None;
    const SplitNovelText *split_ = nullptr;
    QListWidget *chapterList_ = nullptr;
    QPlainTextEdit *chapterPreview_ = nullptr;
};

#endif // WRITE_IMPORTPREVIEWDIALOG_H
