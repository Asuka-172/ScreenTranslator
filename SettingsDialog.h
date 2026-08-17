#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QTabWidget;
class QComboBox;
class QListWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void setEngines(const QStringList& engineIds, const QStringList& engineLabels, const QString& current);
    void setPlugins(const QStringList& descriptions);

signals:
    void testSpeechRequested();

private:
    QWidget* createGeneralPage();
    QWidget* createAppearancePage();
    QWidget* createHotkeyPage();
    QWidget* createTranslationPage();
    QWidget* createPluginPage();
    QWidget* createAccessibilityPage();

    QTabWidget* m_tabs;
    QComboBox* m_engineCombo = nullptr;
    QListWidget* m_pluginList = nullptr;
};

#endif
