#ifndef WRITE_CREATEENTITYDIALOG_H
#define WRITE_CREATEENTITYDIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QWidget;
struct Entity;

class CreateEntityDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateEntityDialog(QWidget *parent = nullptr);
    void loadEntity(const Entity &entity);

    QString kind() const;
    QString name() const;
    QString role() const;
    QString summary() const;
    QString background() const;
    QString notes() const;

private:
    void applyStyle();

    QComboBox *kindCombo_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QWidget *roleField_ = nullptr;
    QLineEdit *roleEdit_ = nullptr;
    QPlainTextEdit *summaryEdit_ = nullptr;
    QWidget *backgroundField_ = nullptr;
    QPlainTextEdit *backgroundEdit_ = nullptr;
    QPlainTextEdit *notesEdit_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QLabel *descriptionLabel_ = nullptr;
    QPushButton *confirmButton_ = nullptr;
};

#endif // WRITE_CREATEENTITYDIALOG_H
