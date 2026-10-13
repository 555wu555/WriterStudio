//
// Created by shijie on 2026/10/3.
//

#ifndef WRITE_SETTER_H
#define WRITE_SETTER_H
#include <QWidget>
#include <QString>
#include <QImage>
#include <QPixmap>
#include "../../models/StorySetup.h"
#include "../../storage/ModelSettingsStore.h"
#include "../../storage/CloudProfileStore.h"
#include "../../storage/ActiveModelServiceStore.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QComboBox;
class QPlainTextEdit;
class QCheckBox;
class QNetworkAccessManager;
class QStackedWidget;
class ModelDebugDialog;

class Setter : public QWidget{
    Q_OBJECT
public:
    explicit Setter(QWidget *parent = nullptr);
    ~Setter() override = default;
    QString defaultAuthorName() const;
    QPixmap avatarPixmap(int size) const;
    void showAuthorSection();
    bool inlineSuggestionEnabled() const { return inlineSuggestionEnabled_; }

signals:
    void defaultAuthorNameChanged(const QString &name);
    void authorAvatarChanged();
    void globalWritingConventionChanged();
    void dataRootChangeRequested(const QString &targetRoot);
    void inlineSuggestionEnabledChanged(bool enabled);
    void activeModelChanged(const QString &modelKey, const QString &modelName);
    void modelAvailabilityChanged(const QString &modelKey, const QString &modelName,
                                  const QString &state, const QString &details);

private:
    QLineEdit *authorNameEdit_ = nullptr;
    QListWidget *categoryList_ = nullptr;
    QLabel *avatarPreview_ = nullptr;
    QLabel *authorPreviewName_ = nullptr;
    QLabel *saveStatus_ = nullptr;
    QPushButton *resetAvatarButton_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QString savedAuthorName_;
    QString avatarPath_;
    QImage savedAvatarImage_;
    WritingConvention savedConvention_;
    QComboBox *globalViewpointCombo_ = nullptr;
    QComboBox *globalTenseCombo_ = nullptr;
    QPlainTextEdit *globalStyleEdit_ = nullptr;
    QPlainTextEdit *globalRhythmEdit_ = nullptr;
    QPlainTextEdit *globalConstraintsEdit_ = nullptr;
    QLabel *globalSaveStatus_ = nullptr;
    QPushButton *globalSaveButton_ = nullptr;
    QCheckBox *inlineSuggestionCheck_ = nullptr;
    bool inlineSuggestionEnabled_ = true;
    ModelSettings savedModelSettings_;
    QLineEdit *modelUrlEdit_ = nullptr;
    QComboBox *modelCombo_ = nullptr;
    QLineEdit *modelNameInput_ = nullptr;
    QPushButton *modelSaveButton_ = nullptr;
    QPushButton *modelRefreshButton_ = nullptr;
    QPushButton *modelTestButton_ = nullptr;
    QPushButton *modelRemoveButton_ = nullptr;
    QLabel *modelSaveStatus_ = nullptr;
    QLabel *modelConnectionStatus_ = nullptr;
    QNetworkAccessManager *modelNetwork_ = nullptr;
    ModelDebugDialog *modelDebugDialog_ = nullptr;
    ActiveModelService activeModelService_ = ActiveModelService::Ollama;
    QVector<CloudProfile> cloudProfiles_;
    QString activeCloudProfileId_;
    QString editingCloudProfileId_;
    QLabel *activeServiceStatus_ = nullptr;
    QStackedWidget *modelSourceStack_ = nullptr;
    QPushButton *cloudSourceButton_ = nullptr;
    QPushButton *ollamaSourceButton_ = nullptr;
    QPushButton *useCloudButton_ = nullptr;
    QComboBox *cloudPresetCombo_ = nullptr;
    QComboBox *cloudProfileCombo_ = nullptr;
    QLineEdit *cloudNameEdit_ = nullptr;
    QLineEdit *cloudUrlEdit_ = nullptr;
    QLineEdit *cloudModelEdit_ = nullptr;
    QLineEdit *cloudKeyEdit_ = nullptr;
    QLabel *cloudKeyStatus_ = nullptr;
    QLabel *cloudStatus_ = nullptr;
    QPushButton *cloudSaveButton_ = nullptr;
    QPushButton *cloudDeleteButton_ = nullptr;
    QPushButton *cloudTestButton_ = nullptr;
    QString savedSystemRules_;
    QPlainTextEdit *systemPromptEdit_ = nullptr;
    QPushButton *systemPromptSaveButton_ = nullptr;
    QPushButton *systemPromptRestoreButton_ = nullptr;
    QLabel *systemPromptStatus_ = nullptr;

    void updateAuthorPreview(const QString &name);
    void updateAvatarPreview();
    void chooseAvatar();
    void resetAvatar();
    void setSaveStatus(const QString &state, const QString &text);
    void saveAuthorName();
    WritingConvention collectGlobalConvention() const;
    void updateGlobalSaveState();
    void saveGlobalConvention();
    ModelSettings collectModelSettings() const;
    void updateModelSaveState();
    void saveModelSettings();
    void updateSystemPromptSaveState();
    void saveSystemPromptRules();
    void restoreDefaultSystemPrompt();
    void addKnownModel();
    void removeSelectedModel();
    void fetchInstalledModels();
    void testSelectedModel();
    void setModelConnectionStatus(const QString &state, const QString &message);
    void setCloudStatus(const QString &state, const QString &message);
    void refreshActiveServiceStatus();
    void showModelSource(ActiveModelService source);
    void refreshCloudProfileCombo();
    void loadCloudProfileForm(const QString &id);
    CloudProfile collectCloudProfile() const;
    bool cloudFormHasChanges() const;
    bool confirmDiscardCloudEdits();
    void updateCloudSaveState();
    void createCloudProfile();
    void saveCloudProfile();
    void deleteCloudProfile();
    void testCloudProfile();
    void activateCloudProfile();
    void applyStyle();
};


#endif //WRITE_SETTER_H
