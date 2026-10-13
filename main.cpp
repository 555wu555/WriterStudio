#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>
#include <QTemporaryFile>
#include "MainWin.h"
#include "storage/DataRootStore.h"
#include "storage/StoragePaths.h"

namespace {
bool prepareLibrary(const QString &dataPath, QString &projectsPath, QString &error) {
    if (dataPath.isEmpty() || !QDir::isAbsolutePath(dataPath)) {
        error = QStringLiteral("作品库位置不是有效的绝对路径：%1").arg(dataPath);
        return false;
    }
    projectsPath = QDir(dataPath).filePath(StoragePaths::ProjectsDirName);
    if (!QDir().mkpath(projectsPath)) {
        error = QStringLiteral("无法创建作品目录：%1").arg(projectsPath);
        return false;
    }
    QTemporaryFile probe(QDir(projectsPath).filePath(
        QStringLiteral(".writerstudio-write-XXXXXX.tmp")));
    if (!probe.open()) {
        error = QStringLiteral("作品目录不可写：%1\n%2")
                    .arg(projectsPath, probe.errorString());
        return false;
    }
    return true;
}

bool chooseInitialLibrary(const QString &defaultPath,
                          QString &dataPath, QString &projectsPath) {
    while (true) {
        QMessageBox box;
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(QStringLiteral("选择作品库位置"));
        box.setText(QStringLiteral("首次使用，请选择小说和归档的存放位置。"));
        box.setInformativeText(QStringLiteral(
            "默认位置：%1\n\n如果已有作品库，请选择包含 projects 文件夹的目录。"
        ).arg(QDir::toNativeSeparators(defaultPath)));
        auto *defaultButton = box.addButton(
            QStringLiteral("使用默认位置"), QMessageBox::AcceptRole);
        auto *chooseButton = box.addButton(
            QStringLiteral("选择其他位置…"), QMessageBox::ActionRole);
        box.addButton(QStringLiteral("退出"), QMessageBox::RejectRole);
        box.exec();

        QString selected;
        if (box.clickedButton() == defaultButton) {
            selected = defaultPath;
        } else if (box.clickedButton() == chooseButton) {
            selected = QFileDialog::getExistingDirectory(
                nullptr, QStringLiteral("选择作品库目录"), QDir::homePath(),
                QFileDialog::ShowDirsOnly);
            if (selected.isEmpty()) continue;
        } else {
            return false;
        }

        QString error;
        if (!prepareLibrary(selected, projectsPath, error) ||
            !DataRootStore::save(selected, error)) {
            QMessageBox::warning(nullptr, QStringLiteral("无法使用所选位置"), error);
            continue;
        }
        dataPath = selected;
        return true;
    }
}
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("WriterStudio");
    a.setWindowIcon(QIcon(":/icons/logo.png"));

    QString dataPath;
    QString projectsPath;
    QString error;
    bool hasSavedPath = false;
    if (!DataRootStore::hasSavedPath(hasSavedPath, error) ||
        !DataRootStore::load(dataPath, error)) {
        QMessageBox::critical(nullptr, QStringLiteral("无法读取作品库位置"), error);
        return 1;
    }
    if (!hasSavedPath &&
        !chooseInitialLibrary(dataPath, dataPath, projectsPath))
        return 0;

    while (!DataRootStore::load(dataPath, error) ||
           !prepareLibrary(dataPath, projectsPath, error)) {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QStringLiteral("无法打开作品库"));
        box.setTextFormat(Qt::PlainText);
        box.setText(error);
        box.setInformativeText(QStringLiteral(
            "可以选择一个可写目录继续。若要找回已有作品，请选择包含 projects 文件夹的原作品库目录。"));
        auto *chooseButton = box.addButton(
            QStringLiteral("选择其他目录…"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("退出"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != chooseButton) return 1;

        const QString selected = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("选择作品库目录"), QDir::homePath(),
            QFileDialog::ShowDirsOnly);
        if (selected.isEmpty()) continue;
        if (!prepareLibrary(selected, projectsPath, error)) {
            QMessageBox::warning(nullptr, QStringLiteral("无法使用所选目录"), error);
            continue;
        }
        if (!DataRootStore::save(selected, error)) {
            QMessageBox::warning(nullptr, QStringLiteral("无法保存作品库位置"), error);
            continue;
        }
        dataPath = selected;
        break;
    }
    qDebug().noquote() << "作品保存目录：" << projectsPath;
    MainWin win(projectsPath);
    win.show();
    return QApplication::exec();
}
