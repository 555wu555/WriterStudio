#ifndef WRITE_MODELDEBUGDIALOG_H
#define WRITE_MODELDEBUGDIALOG_H

#include <QDialog>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QTabWidget;

class ModelDebugDialog : public QDialog {
public:
    explicit ModelDebugDialog(QWidget *parent = nullptr);

private:
    void refreshEntries();
    void showSelectedEntry();
    QListWidget *entryList_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QPlainTextEdit *promptView_ = nullptr;
    QPlainTextEdit *requestView_ = nullptr;
    QPlainTextEdit *responseView_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};

#endif // WRITE_MODELDEBUGDIALOG_H
