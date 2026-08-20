#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QTabWidget;
class QComboBox;
class QListWidget;
class QStackedWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void setEngines(const QStringList& engineIds, const QStringList& engineLabels, const QString& current);
    void setPlugins(const QStringList& descriptions);

signals:
    void testSpeechRequested();
    void testTranslationRequested(const QString& engineId);

private:
    QWidget* createGeneralPage();
    QWidget* createAppearancePage();
    QWidget* createHotkeyPage();
    QWidget* createTranslationPage();
    QWidget* createPluginPage();
    QWidget* createAccessibilityPage();
    void syncEngineStack();

    QTabWidget* m_tabs;
    QComboBox* m_engineCombo = nullptr;
    QStackedWidget* m_engineStack = nullptr;
    QListWidget* m_pluginList = nullptr;
};

#endif
