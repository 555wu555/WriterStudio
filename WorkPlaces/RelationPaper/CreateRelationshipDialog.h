#ifndef WRITE_CREATERELATIONSHIPDIALOG_H
#define WRITE_CREATERELATIONSHIPDIALOG_H

#include <QDialog>
#include <QVector>
#include "../../models/Entity.h"
#include "../../models/Relationship.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class CreateRelationshipDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateRelationshipDialog(const QVector<Entity> &entities,
                                      QWidget *parent = nullptr);

    bool setRelationshipForEditing(const Relationship &relationship);

    QString fromId() const;
    QString toId() const;
    QString label() const;
    QString reverseLabel() const;
    QString notes() const;

private:
    void updateForm();
    void applyStyle();

    QComboBox *fromCombo_ = nullptr;
    QComboBox *toCombo_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QLabel *hintLabel_ = nullptr;
    QLabel *forwardLabel_ = nullptr;
    QLabel *reverseLabel_ = nullptr;
    QLabel *forwardHelpLabel_ = nullptr;
    QLabel *reverseHelpLabel_ = nullptr;
    QLineEdit *forwardEdit_ = nullptr;
    QLineEdit *reverseEdit_ = nullptr;
    QCheckBox *sameLabelCheck_ = nullptr;
    QPlainTextEdit *notesEdit_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QPushButton *confirmButton_ = nullptr;
};

#endif // WRITE_CREATERELATIONSHIPDIALOG_H
