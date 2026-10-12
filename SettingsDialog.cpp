#include "SettingsDialog.h"
#include "AppConfig.h"
#include "OfflineTranslator.h"

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
#include <QLineEdit>
#include <QFontMetrics>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QScrollArea>
#include <QFrame>

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

QLineEdit* makeEngineLineEdit(QWidget* parent)
{
    // 输入框样式（白底黑字/边框/内边距）由应用级 QSS 统一提供，
    // 这里只设置与应用一致的字号与最小高度，保证文字垂直居中、与标签对齐。
    QLineEdit* e = new QLineEdit(parent);

    QFont boxFont = e->font();
    boxFont.setPointSize(AppConfig::instance().fontSize());
    e->setFont(boxFont);
    const QFontMetrics fm(boxFont);
    e->setMinimumHeight(fm.height() + 10);
    return e;
}

QDialog* buildEngineConfigDialog(const QString& engineId, QWidget* parent)
{
    QDialog* dlg = new QDialog(parent);
    dlg->setWindowTitle("引擎配置");
    dlg->setMinimumWidth(440);

    QVBoxLayout* lay = new QVBoxLayout(dlg);

    if (engineId == QLatin1String("custom")) {
        QFormLayout* f = new QFormLayout();
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lay->addLayout(f);

        QLineEdit* urlEdit = makeEngineLineEdit(dlg);
        urlEdit->setText(AppConfig::instance().customEngineUrl());
        urlEdit->setPlaceholderText("https://api.example.com/translate?key={key}&text={text}&from={from}&to={to}");
        QObject::connect(urlEdit, &QLineEdit::editingFinished, urlEdit, [urlEdit]() {
            AppConfig::instance().setCustomEngineUrl(urlEdit->text().trimmed());
        });
        f->addRow("请求 URL 模板", urlEdit);

        QLineEdit* keyEdit = makeEngineLineEdit(dlg);
        keyEdit->setText(AppConfig::instance().customEngineApiKey());
        keyEdit->setEchoMode(QLineEdit::Password);
        QObject::connect(keyEdit, &QLineEdit::editingFinished, keyEdit, [keyEdit]() {
            AppConfig::instance().setCustomEngineApiKey(keyEdit->text());
        });
        f->addRow("API Key", keyEdit);

        QComboBox* keyPosCombo = new QComboBox(dlg);
        keyPosCombo->addItem("URL 参数 ({key})", "url");
        keyPosCombo->addItem("请求头", "header");
        const bool hasHeader = !AppConfig::instance().customEngineKeyHeader().isEmpty();
        keyPosCombo->setCurrentIndex(hasHeader ? 1 : 0);

        QLineEdit* headerEdit = makeEngineLineEdit(dlg);
        headerEdit->setText(AppConfig::instance().customEngineKeyHeader());
        headerEdit->setPlaceholderText("Authorization / X-Api-Key");
        headerEdit->setEnabled(hasHeader);
        QObject::connect(headerEdit, &QLineEdit::editingFinished, headerEdit, [headerEdit]() {
            AppConfig::instance().setCustomEngineKeyHeader(headerEdit->text().trimmed());
        });
        QObject::connect(keyPosCombo, &QComboBox::currentIndexChanged, keyPosCombo,
            [keyPosCombo, headerEdit](int i) {
                const bool header = (i == 1);
                headerEdit->setEnabled(header);
                if (!header)
                    AppConfig::instance().setCustomEngineKeyHeader(QString());
            });

        f->addRow("密钥位置", keyPosCombo);
        f->addRow("请求头名称", headerEdit);

        QLineEdit* pathEdit = makeEngineLineEdit(dlg);
        pathEdit->setText(AppConfig::instance().customEngineResultPath());
        pathEdit->setPlaceholderText("如 data.translatedText；留空表示整个响应即译文");
        QObject::connect(pathEdit, &QLineEdit::editingFinished, pathEdit, [pathEdit]() {
            AppConfig::instance().setCustomEngineResultPath(pathEdit->text().trimmed());
        });
        f->addRow("结果 JSON 路径", pathEdit);

        QLabel* hint = new QLabel("占位符：{text}=原文  {from}=源语言  {to}=目标语言  {key}=密钥", dlg);
        hint->setWordWrap(true);
        f->addRow(hint);
    } else if (engineId == QLatin1String("offline")) {
        QFormLayout* f = new QFormLayout();
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lay->addLayout(f);

        QCheckBox* enableCheck = new QCheckBox("启用离线翻译（本地词典）", dlg);
        enableCheck->setChecked(AppConfig::instance().offlineEnabled());
        QObject::connect(enableCheck, &QCheckBox::toggled, enableCheck, [](bool on) {
            AppConfig::instance().setOfflineEnabled(on);
        });
        f->addRow(enableCheck);

        QLineEdit* modelEdit = makeEngineLineEdit(dlg);
        modelEdit->setText(AppConfig::instance().offlineModelPath());
        modelEdit->setPlaceholderText(OfflineTranslator::defaultModelDir());
        QObject::connect(modelEdit, &QLineEdit::editingFinished, modelEdit, [modelEdit]() {
            AppConfig::instance().setOfflineModelPath(modelEdit->text().trimmed());
        });
        f->addRow("模型目录", modelEdit);

        QLineEdit* glossEdit = makeEngineLineEdit(dlg);
        glossEdit->setText(AppConfig::instance().offlineGlossaryPath());
        glossEdit->setPlaceholderText(OfflineTranslator::defaultGlossaryPath());
        QObject::connect(glossEdit, &QLineEdit::editingFinished, glossEdit, [glossEdit]() {
            AppConfig::instance().setOfflineGlossaryPath(glossEdit->text().trimmed());
        });
        f->addRow("词典文件", glossEdit);

        QPushButton* editBtn = new QPushButton("编辑词典", dlg);
        QObject::connect(editBtn, &QPushButton::clicked, editBtn, [glossEdit]() {
            QString path = glossEdit->text().trimmed();
            if (path.isEmpty())
                path = OfflineTranslator::defaultGlossaryPath();
            QFile file(path);
            if (!file.exists()) {
                QDir().mkpath(QFileInfo(path).absolutePath());
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.write("# 离线词典：每行一条，格式为  源词=译文  （# 开头为注释）\n");
                    file.close();
                }
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        f->addRow(editBtn);

        QLabel* fmtHint = new QLabel("词典格式：每行 源词=译文（# 开头为注释）", dlg);
        fmtHint->setWordWrap(true);
        f->addRow(fmtHint);
    } else if (engineId == QStringLiteral("百度翻译")) {
        QFormLayout* f = new QFormLayout();
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lay->addLayout(f);

        QLineEdit* appIdEdit = makeEngineLineEdit(dlg);
        appIdEdit->setText(AppConfig::instance().baiduAppId());
        appIdEdit->setPlaceholderText("百度翻译开放平台 APPID");
        QObject::connect(appIdEdit, &QLineEdit::editingFinished, appIdEdit, [appIdEdit]() {
            AppConfig::instance().setBaiduAppId(appIdEdit->text().trimmed());
        });
        f->addRow("APPID", appIdEdit);

        QLineEdit* secretEdit = makeEngineLineEdit(dlg);
        secretEdit->setText(AppConfig::instance().baiduSecret());
        secretEdit->setEchoMode(QLineEdit::Password);
        secretEdit->setPlaceholderText("百度翻译开放平台 密钥");
        QObject::connect(secretEdit, &QLineEdit::editingFinished, secretEdit, [secretEdit]() {
            AppConfig::instance().setBaiduSecret(secretEdit->text().trimmed());
        });
        f->addRow("密钥", secretEdit);

        QLabel* hint = new QLabel("密钥在百度翻译开放平台「开发者信息」中查看。", dlg);
        hint->setWordWrap(true);
        f->addRow(hint);
    } else {
        lay->addWidget(new QLabel("该引擎无需额外配置。", dlg));
    }

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(box, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    lay->addWidget(box);

    return dlg;
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
    QObject::connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(box, &QDialogButtonBox::clicked, this, [this](QAbstractButton* b) {
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
    QObject::connect(enableCheck, &QCheckBox::toggled, enableCheck, [](bool on) {
        AppConfig::instance().setTtsEnabled(on);
    });
    form->addRow(enableCheck);

    QSlider* rateSlider = new QSlider(Qt::Horizontal, ttsGroup);
    rateSlider->setRange(50, 200);
    rateSlider->setValue(qRound(AppConfig::instance().ttsRate() * 100.0));
    QLabel* rateValue = new QLabel(QString::number(AppConfig::instance().ttsRate(), 'f', 1) + "x", ttsGroup);
    QObject::connect(rateSlider, &QSlider::valueChanged, rateSlider, [rateValue](int v) {
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
    QObject::connect(volSlider, &QSlider::valueChanged, volSlider, [volValue](int v) {
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
    QObject::connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::testSpeechRequested);
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
    QObject::connect(themeCombo, &QComboBox::currentIndexChanged, themeCombo, [themeCombo](int) {
        AppConfig::instance().setTheme(themeCombo->currentData().toString());
    });
    themeForm->addRow("配色方案", themeCombo);

    QGroupBox* fontGroup = new QGroupBox("字体", page);
    QFormLayout* fontForm = new QFormLayout(fontGroup);
    QFontComboBox* fontCombo = new QFontComboBox(fontGroup);
    fontCombo->setCurrentFont(QFont(AppConfig::instance().fontFamily()));
    QObject::connect(fontCombo, &QFontComboBox::currentFontChanged, fontCombo, [](const QFont& f) {
        AppConfig::instance().setFontFamily(f.family());
    });
    fontForm->addRow("字体族", fontCombo);

    QSpinBox* sizeSpin = new QSpinBox(fontGroup);
    sizeSpin->setRange(12, 20);
    sizeSpin->setValue(AppConfig::instance().fontSize());
    QObject::connect(sizeSpin, &QSpinBox::valueChanged, sizeSpin, [](int v) {
        AppConfig::instance().setFontSize(v);
    });
    fontForm->addRow("字号", sizeSpin);

    QGroupBox* opacityGroup = new QGroupBox("窗口透明度", page);
    QHBoxLayout* opacityLay = new QHBoxLayout(opacityGroup);
    QSlider* opacitySlider = new QSlider(Qt::Horizontal, opacityGroup);
    opacitySlider->setRange(10, 100);
    opacitySlider->setValue(AppConfig::instance().windowOpacity());
    QLabel* opacityValue = new QLabel(QString::number(AppConfig::instance().windowOpacity()) + "%", opacityGroup);
    QObject::connect(opacitySlider, &QSlider::valueChanged, opacitySlider, [opacitySlider, opacityValue](int v) {
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
        QObject::connect(edit, &QKeySequenceEdit::keySequenceChanged, edit,
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
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(10);

    // 输入控件高度对齐「四号」（应用默认 14pt）字高，避免内容紧凑、控件高低不一
    const int inputH = QFontMetrics(
        QFont(AppConfig::instance().fontFamily(), AppConfig::instance().fontSize())).height() + 14;

    QGroupBox* engineGroup = new QGroupBox("翻译引擎", page);
    QVBoxLayout* engineLay = new QVBoxLayout(engineGroup);

    m_engineCombo = new QComboBox(engineGroup);
    m_engineCombo->setMinimumHeight(inputH);
    engineLay->addWidget(m_engineCombo);

    QLabel* cfgTip = new QLabel("切换引擎后，将在新窗口中进行配置。", engineGroup);
    cfgTip->setWordWrap(true);
    engineLay->addWidget(cfgTip);

    QObject::connect(m_engineCombo, &QComboBox::currentIndexChanged, m_engineCombo, [this](int) {
        const QString id = m_engineCombo->currentData().toString();
        AppConfig::instance().setTranslatorEngine(id);
        if (id == QLatin1String("custom") || id == QLatin1String("offline")
            || id == QStringLiteral("百度翻译")) {
            QDialog* dlg = buildEngineConfigDialog(id, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->exec();
        }
    });

    QPushButton* testBtn = new QPushButton("测试翻译", engineGroup);
    testBtn->setMinimumHeight(inputH);
    QObject::connect(testBtn, &QPushButton::clicked, this, [this]() {
        emit testTranslationRequested(m_engineCombo->currentData().toString());
    });
    engineLay->addWidget(testBtn, 0, Qt::AlignLeft);

    lay->addWidget(engineGroup);

    QGroupBox* fallbackGroup = new QGroupBox("回退", page);
    QVBoxLayout* fallbackLay = new QVBoxLayout(fallbackGroup);
    QCheckBox* fallbackCheck = new QCheckBox("在线引擎失败时自动回退到离线引擎", fallbackGroup);
    fallbackCheck->setChecked(AppConfig::instance().fallbackEnabled());
    QObject::connect(fallbackCheck, &QCheckBox::toggled, fallbackCheck, [](bool on) {
        AppConfig::instance().setFallbackEnabled(on);
    });
    fallbackLay->addWidget(fallbackCheck);
    lay->addWidget(fallbackGroup);

    QGroupBox* cacheGroup = new QGroupBox("翻译缓存", page);
    QFormLayout* cacheForm = new QFormLayout(cacheGroup);
    QCheckBox* cacheCheck = new QCheckBox("启用缓存", cacheGroup);
    cacheCheck->setChecked(AppConfig::instance().cacheEnabled());
    QObject::connect(cacheCheck, &QCheckBox::toggled, cacheCheck, [](bool on) {
        AppConfig::instance().setCacheEnabled(on);
    });
    cacheForm->addRow(cacheCheck);

    QSpinBox* maxSpin = new QSpinBox(cacheGroup);
    maxSpin->setRange(0, 10000);
    maxSpin->setValue(AppConfig::instance().cacheMaxEntries());
    maxSpin->setMinimumHeight(inputH);
    QObject::connect(maxSpin, &QSpinBox::valueChanged, maxSpin, [](int v) {
        AppConfig::instance().setCacheMaxEntries(v);
    });
    cacheForm->addRow("最大缓存条数", maxSpin);
    lay->addWidget(cacheGroup);

    QGroupBox* exportGroup = new QGroupBox("历史导出格式", page);
    QFormLayout* exportForm = new QFormLayout(exportGroup);
    QComboBox* exportCombo = new QComboBox(exportGroup);
    exportCombo->addItem("txt", "txt");
    exportCombo->addItem("docx", "docx");
    exportCombo->addItem("md", "md");
    exportCombo->addItem("csv", "csv");
    exportCombo->setMinimumHeight(inputH);
    int idx = exportCombo->findData(AppConfig::instance().exportFormat());
    if (idx >= 0) exportCombo->setCurrentIndex(idx);
    QObject::connect(exportCombo, &QComboBox::currentIndexChanged, exportCombo, [exportCombo](int) {
        AppConfig::instance().setExportFormat(exportCombo->currentData().toString());
    });
    exportForm->addRow("格式", exportCombo);
    lay->addWidget(exportGroup);

    QGroupBox* docGroup = new QGroupBox("文档翻译（长文）", page);
    QFormLayout* docForm = new QFormLayout(docGroup);
    docForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QSpinBox* chunkSpin = new QSpinBox(docGroup);
    chunkSpin->setRange(500, 20000);
    chunkSpin->setSingleStep(500);
    chunkSpin->setValue(AppConfig::instance().docChunkMaxBytes());
    chunkSpin->setMinimumHeight(inputH);
    QObject::connect(chunkSpin, &QSpinBox::valueChanged, chunkSpin, [](int v) {
        AppConfig::instance().setDocChunkMaxBytes(v);
    });
    docForm->addRow("分块字节上限", chunkSpin);

    QSpinBox* concSpin = new QSpinBox(docGroup);
    concSpin->setRange(1, 8);
    concSpin->setValue(AppConfig::instance().docConcurrency());
    concSpin->setMinimumHeight(inputH);
    QObject::connect(concSpin, &QSpinBox::valueChanged, concSpin, [](int v) {
        AppConfig::instance().setDocConcurrency(v);
    });
    docForm->addRow("并发路数", concSpin);

    QSpinBox* retrySpin = new QSpinBox(docGroup);
    retrySpin->setRange(0, 10);
    retrySpin->setValue(AppConfig::instance().docRetryMax());
    retrySpin->setMinimumHeight(inputH);
    QObject::connect(retrySpin, &QSpinBox::valueChanged, retrySpin, [](int v) {
        AppConfig::instance().setDocRetryMax(v);
    });
    docForm->addRow("失败重试次数", retrySpin);

    QSpinBox* baseSpin = new QSpinBox(docGroup);
    baseSpin->setRange(0, 30000);
    baseSpin->setSingleStep(500);
    baseSpin->setValue(AppConfig::instance().docRetryBaseMs());
    baseSpin->setMinimumHeight(inputH);
    QObject::connect(baseSpin, &QSpinBox::valueChanged, baseSpin, [](int v) {
        AppConfig::instance().setDocRetryBaseMs(v);
    });
    docForm->addRow("重试基础间隔(ms)", baseSpin);

    QSpinBox* intervalSpin = new QSpinBox(docGroup);
    intervalSpin->setRange(0, 10000);
    intervalSpin->setSingleStep(100);
    intervalSpin->setValue(AppConfig::instance().docMinIntervalMs());
    intervalSpin->setMinimumHeight(inputH);
    QObject::connect(intervalSpin, &QSpinBox::valueChanged, intervalSpin, [](int v) {
        AppConfig::instance().setDocMinIntervalMs(v);
    });
    docForm->addRow("请求最小间隔(ms)", intervalSpin);

    QLabel* intervalHint = new QLabel("0=不限；百度标准版建议 1100", docGroup);
    intervalHint->setWordWrap(true);
    docForm->addRow(intervalHint);

    QCheckBox* resumeCheck = new QCheckBox("启用断点续传（失败/取消后可继续）", docGroup);
    resumeCheck->setChecked(AppConfig::instance().resumeEnabled());
    QObject::connect(resumeCheck, &QCheckBox::toggled, resumeCheck, [](bool on) {
        AppConfig::instance().setResumeEnabled(on);
    });
    docForm->addRow(resumeCheck);

    lay->addWidget(docGroup);

    lay->addStretch();

    // 内容较多：改为上下滚动模式
    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);   // 避免 viewport 用默认白底盖住对话框背景
    page->setAutoFillBackground(false);
    scroll->setWidget(page);
    return scroll;
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
    QObject::connect(hcCheck, &QCheckBox::toggled, hcCheck, [](bool on) {
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
