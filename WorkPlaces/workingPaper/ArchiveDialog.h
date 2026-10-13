#ifndef WRITE_ARCHIVEDIALOG_H
#define WRITE_ARCHIVEDIALOG_H

#include <QDialog>
#include <QString>

class QListWidget;
class QPushButton;

class ArchiveDialog : public QDialog {
public:
    explicit ArchiveDialog(const QString &archivePath, QWidget *parent = nullptr);
    QString selectedProjectId() const;
private:
    QListWidget *list_ = nullptr;
    QPushButton *restoreButton_ = nullptr;

    void uiStyle();
};

#endif // WRITE_ARCHIVEDIALOG_H
