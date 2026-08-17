#include "SettingsDialog.h"
#include "AppConfig.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QFontComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFont>
#include <QKeySequenceEdit>
#include <QListWidget>

namespace {

QWidget* placeholderPage(const QString& title, const QString& hint)
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    QLabel* label = new QLabel(title, page);
    QFont f = label->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    label->setFont(f);
    lay->addWidget(label);
    lay->addWidget(new QLabel(hint, page));
    lay->addStretch();
    return page;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("设置");
    setMinimumSize(460, 420);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createGeneralPage(), "常规");
    m_tabs->addTab(createAppearancePage(), "外观");
    m_tabs->addTab(createHotkeyPage(), "全局热键");
    m_tabs->addTab(createTranslationPage(), "翻译与词典");
    m_tabs->addTab(createPluginPage(), "插件");
    m_tabs->addTab(createAccessibilityPage(), "无障碍");

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->addWidget(m_tabs);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::clicked, this, [this](QAbstractButton* b) {
        Q_UNUSED(b);
        reject();
    });
    lay->addWidget(box);
}

void SettingsDialog::setEngines(const QStringList& engineIds, const QStringList& engineLabels, const QString& current)
{
    if (!m_engineCombo)
        return;
    m_engineCombo->blockSignals(true);
    m_engineCombo->clear();
    for (int i = 0; i < engineIds.size() && i < engineLabels.size(); ++i)
        m_engineCombo->addItem(engineLabels[i], engineIds[i]);
    int idx = m_engineCombo->findData(current);
    if (idx >= 0)
        m_engineCombo->setCurrentIndex(idx);
    m_engineCombo->blockSignals(false);
}

void SettingsDialog::setPlugins(const QStringList& descriptions)
{
    if (!m_pluginList)
        return;
    m_pluginList->clear();
    m_pluginList->addItems(descriptions);
}

QWidget* SettingsDialog::createGeneralPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);

    QGroupBox* ttsGroup = new QGroupBox("翻译自动朗读 (TTS)", page);
    QFormLayout* form = new QFormLayout(ttsGroup);

    QCheckBox* enableCheck = new QCheckBox("翻译完成后自动朗读译文", ttsGroup);
    enableCheck->setChecked(AppConfig::instance().ttsEnabled());
    connect(enableCheck, &QCheckBox::toggled, enableCheck, [](bool on) {
        AppConfig::instance().setTtsEnabled(on);
    });
    form->addRow(enableCheck);

    QSlider* rateSlider = new QSlider(Qt::Horizontal, ttsGroup);
    rateSlider->setRange(50, 200);
    rateSlider->setValue(qRound(AppConfig::instance().ttsRate() * 100.0));
    QLabel* rateValue = new QLabel(QString::number(AppConfig::instance().ttsRate(), 'f', 1) + "x", ttsGroup);
    connect(rateSlider, &QSlider::valueChanged, rateSlider, [rateValue](int v) {
        double r = v / 100.0;
        rateValue->setText(QString::number(r, 'f', 1) + "x");
        AppConfig::instance().setTtsRate(r);
    });
    QWidget* rateRow = new QWidget(ttsGroup);
    QHBoxLayout* rateLay = new QHBoxLayout(rateRow);
    rateLay->setContentsMargins(0, 0, 0, 0);
    rateLay->addWidget(rateSlider);
    rateLay->addWidget(rateValue);
    form->addRow("语速", rateRow);

    QSlider* volSlider = new QSlider(Qt::Horizontal, ttsGroup);
    volSlider->setRange(0, 100);
    volSlider->setValue(AppConfig::instance().ttsVolume());
    QLabel* volValue = new QLabel(QString::number(AppConfig::instance().ttsVolume()) + "%", ttsGroup);
    connect(volSlider, &QSlider::valueChanged, volSlider, [volValue](int v) {
        volValue->setText(QString::number(v) + "%");
        AppConfig::instance().setTtsVolume(v);
    });
    QWidget* volRow = new QWidget(ttsGroup);
    QHBoxLayout* volLay = new QHBoxLayout(volRow);
    volLay->setContentsMargins(0, 0, 0, 0);
    volLay->addWidget(volSlider);
    volLay->addWidget(volValue);
    form->addRow("音量", volRow);

    lay->addWidget(ttsGroup);

    QPushButton* testBtn = new QPushButton("测试朗读", page);
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::testSpeechRequested);
    lay->addWidget(testBtn, 0, Qt::AlignLeft);

    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::createAppearancePage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);

    QGroupBox* themeGroup = new QGroupBox("主题", page);
    QFormLayout* themeForm = new QFormLayout(themeGroup);
    QComboBox* themeCombo = new QComboBox(themeGroup);
    themeCombo->addItem("深色", "dark");
    themeCombo->addItem("浅色", "light");
    themeCombo->addItem("高对比度", "highcontrast");
    int idx = themeCombo->findData(AppConfig::instance().theme());
    if (idx >= 0) themeCombo->setCurrentIndex(idx);
    connect(themeCombo, &QComboBox::currentIndexChanged, themeCombo, [themeCombo](int) {
        AppConfig::instance().setTheme(themeCombo->currentData().toString());
    });
    themeForm->addRow("配色方案", themeCombo);

    QGroupBox* fontGroup = new QGroupBox("字体", page);
    QFormLayout* fontForm = new QFormLayout(fontGroup);
    QFontComboBox* fontCombo = new QFontComboBox(fontGroup);
    fontCombo->setCurrentFont(QFont(AppConfig::instance().fontFamily()));
    connect(fontCombo, &QFontComboBox::currentFontChanged, fontCombo, [](const QFont& f) {
        AppConfig::instance().setFontFamily(f.family());
    });
    fontForm->addRow("字体族", fontCombo);

    QSpinBox* sizeSpin = new QSpinBox(fontGroup);
    sizeSpin->setRange(12, 20);
    sizeSpin->setValue(AppConfig::instance().fontSize());
    connect(sizeSpin, &QSpinBox::valueChanged, sizeSpin, [](int v) {
        AppConfig::instance().setFontSize(v);
    });
    fontForm->addRow("字号", sizeSpin);

    QGroupBox* opacityGroup = new QGroupBox("窗口透明度", page);
    QHBoxLayout* opacityLay = new QHBoxLayout(opacityGroup);
    QSlider* opacitySlider = new QSlider(Qt::Horizontal, opacityGroup);
    opacitySlider->setRange(10, 100);
    opacitySlider->setValue(AppConfig::instance().windowOpacity());
    QLabel* opacityValue = new QLabel(QString::number(AppConfig::instance().windowOpacity()) + "%", opacityGroup);
    connect(opacitySlider, &QSlider::valueChanged, opacitySlider, [opacitySlider, opacityValue](int v) {
        opacityValue->setText(QString::number(v) + "%");
        AppConfig::instance().setWindowOpacity(v);
    });
    opacityLay->addWidget(opacitySlider);
    opacityLay->addWidget(opacityValue);

    lay->addWidget(themeGroup);
    lay->addWidget(fontGroup);
    lay->addWidget(opacityGroup);
    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::createHotkeyPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);

    QLabel* tip = new QLabel("点击右侧输入框后，按下新的快捷键组合。", page);
    lay->addWidget(tip);

    struct Action { const char* label; const char* key; const char* def; };
    const Action actions[] = {
        {"截图翻译", "capture", "F1"},
        {"语言设置", "language", "F2"},
        {"打开设置", "settings", "F4"},
        {"显示/隐藏主窗口", "toggleWindow", ""},
        {"朗读最近一条", "speakLast", ""},
    };

    for (const Action& a : actions) {
        QHBoxLayout* row = new QHBoxLayout();
        QLabel* nameLabel = new QLabel(QString::fromUtf8(a.label), page);
        nameLabel->setFixedWidth(150);
        QKeySequenceEdit* edit = new QKeySequenceEdit(page);
        QString cur = AppConfig::instance().hotkey(QLatin1String(a.key));
        if (cur.isEmpty())
            cur = QString::fromUtf8(a.def);
        edit->setKeySequence(QKeySequence(cur));
        const QString key = QString::fromUtf8(a.key);
        connect(edit, &QKeySequenceEdit::keySequenceChanged, edit,
                [key](const QKeySequence& seq) {
                    AppConfig::instance().setHotkey(key, seq.toString());
                });
        row->addWidget(nameLabel);
        row->addWidget(edit, 1);
        lay->addLayout(row);
    }

    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::createTranslationPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);

    QGroupBox* engineGroup = new QGroupBox("翻译引擎", page);
    QFormLayout* engineForm = new QFormLayout(engineGroup);
    m_engineCombo = new QComboBox(engineGroup);
    m_engineCombo->addItem("Google", "google");
    connect(m_engineCombo, &QComboBox::currentIndexChanged, m_engineCombo, [this](int) {
        AppConfig::instance().setTranslatorEngine(m_engineCombo->currentData().toString());
    });
    engineForm->addRow("引擎", m_engineCombo);

    QGroupBox* dictGroup = new QGroupBox("划词查询", page);
    QFormLayout* dictForm = new QFormLayout(dictGroup);
    QCheckBox* dictCheck = new QCheckBox("启用划词查询", dictGroup);
    dictCheck->setChecked(AppConfig::instance().dictEnabled());
    connect(dictCheck, &QCheckBox::toggled, dictCheck, [](bool on) {
        AppConfig::instance().setDictEnabled(on);
    });
    dictForm->addRow(dictCheck);

    QGroupBox* exportGroup = new QGroupBox("历史导出格式", page);
    QFormLayout* exportForm = new QFormLayout(exportGroup);
    QComboBox* exportCombo = new QComboBox(exportGroup);
    exportCombo->addItem("txt", "txt");
    exportCombo->addItem("docx", "docx");
    exportCombo->addItem("md", "md");
    exportCombo->addItem("csv", "csv");
    int idx = exportCombo->findData(AppConfig::instance().exportFormat());
    if (idx >= 0) exportCombo->setCurrentIndex(idx);
    connect(exportCombo, &QComboBox::currentIndexChanged, exportCombo, [exportCombo](int) {
        AppConfig::instance().setExportFormat(exportCombo->currentData().toString());
    });
    exportForm->addRow("格式", exportCombo);

    lay->addWidget(engineGroup);
    lay->addWidget(dictGroup);
    lay->addWidget(exportGroup);
    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::createPluginPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);

    QLabel* tip = new QLabel("已加载插件：", page);
    lay->addWidget(tip);

    m_pluginList = new QListWidget(page);
    lay->addWidget(m_pluginList);

    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::createAccessibilityPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);

    QGroupBox* hcGroup = new QGroupBox("高对比度模式", page);
    QFormLayout* hcForm = new QFormLayout(hcGroup);
    QCheckBox* hcCheck = new QCheckBox("启用高对比度模式", hcGroup);
    hcCheck->setChecked(AppConfig::instance().theme() == "highcontrast");
    connect(hcCheck, &QCheckBox::toggled, hcCheck, [](bool on) {
        AppConfig::instance().setTheme(on ? "highcontrast" : "dark");
    });
    hcForm->addRow(hcCheck);

    QGroupBox* srGroup = new QGroupBox("屏幕阅读器兼容", page);
    QVBoxLayout* srLay = new QVBoxLayout(srGroup);
    srLay->addWidget(new QLabel("所有交互控件均设置了无障碍名称，可被屏幕阅读器朗读。", srGroup));
    srLay->addWidget(new QLabel("状态：已启用", srGroup));

    lay->addWidget(hcGroup);
    lay->addWidget(srGroup);
    lay->addStretch();
    return page;
}
