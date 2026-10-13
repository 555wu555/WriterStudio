//
// Created by shijie on 2026/10/4.
//

#ifndef WRITE_CREATEPROJECTDIALOG_H
#define WRITE_CREATEPROJECTDIALOG_H
#include <QDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QStringList>
#include <QPushButton>
#include "../Project.h"

class CreateProjectDialog :public QDialog{
    Q_OBJECT
public:
    explicit CreateProjectDialog(
    const QString &projectsPath,
    QWidget *parent = nullptr
);
    ~CreateProjectDialog() override = default;
    QString projectName() const;
    QString projectGenre() const;
    QStringList projectTags() const;
    QString projectSynopsis() const;
    QString projectAuthor() const;
    void loadProject(const Project& project);
    void setDefaultAuthorName(const QString &name);
private:

    const QStringList tags = {"都市","玄幻","奇幻","仙侠","科幻","历史","悬疑","游戏","武侠","军事","现实","轻小说","种田","青春","纯爱","古言","现言","幻言","年代","其他","待设定"} ;
    QLabel *titleLabel = nullptr;
    QLabel *descriptionLabel = nullptr;
    QLineEdit *nameEdit = nullptr;
    QComboBox *genreCombo = nullptr;
    QLineEdit *tagsEdit = nullptr;
    QPlainTextEdit *synopsisEdit = nullptr;
    QLineEdit *authorEdit = nullptr;
    QLabel *errorLabel = nullptr;
    QPushButton *confirmButton = nullptr;
    void uiStyle();
};


#endif //WRITE_CREATEPROJECTDIALOG_H
