#include "ScreenTranslator.h"
#include <QCoreApplication>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QDateTime>
#include <QClipboard>
#include <QApplication>
#include <future>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <opencv2/imgproc.hpp>
#include "DocxExporter.h"
#include "AppConfig.h"
#include "ThemeManager.h"
#include "SettingsDialog.h"
#include "TextToSpeech.h"
#include "GlobalHotkey.h"
#include "TranslationService.h"
#include "PluginManager.h"
#include "PluginTranslator.h"
#include "TranslationEngineManager.h"
#include "CustomOnlineTranslator.h"
#include "OfflineTranslator.h"
#include "ITranslator.h"

namespace {

// 全局热键动作 ID
enum HotkeyAction {
    ActCapture = 1,
    ActLanguage = 2,
    ActSettings = 3,
    ActToggleWindow = 4,
    ActSpeakLast = 5,
};

// 判断是否为 CJK 表意文字（中文等按字符连续书写，不应被空格拆分）
bool isCjkIdeograph(QChar ch)
{
    const ushort u = ch.unicode();
    return (u >= 0x4E00 && u <= 0x9FFF)     // CJK 统一表意文字
        || (u >= 0x3400 && u <= 0x4DBF)     // 扩展 A
        || (u >= 0xF900 && u <= 0xFAFF);    // 兼容表意文字
}

// 清理 OCR 输出：去除空行/多余空白，把被误分割的行合并回连续文本，
// 并移除 CJK 字符相邻的空格，避免中文被拆成单个字导致翻译出错。
QString cleanupOcrText(const QString& raw)
{
    QString text = raw;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    QStringList cleaned;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.isEmpty())
            cleaned << line;
    }

    QString result;
    for (int i = 0; i < cleaned.size(); ++i) {
        if (i > 0) {
            // 上行以连字符结尾视为英文换行断词：去掉连字符直接拼接
            if (cleaned[i - 1].endsWith(QLatin1Char('-'))) {
                result.chop(1);
                result += cleaned[i];
            } else {
                result += QLatin1Char(' ');
                result += cleaned[i];
            }
        } else {
            result += cleaned[i];
        }
    }

    // 移除与 CJK 字符相邻的空格（如“你 的 b 站” → “你的b站”）
    QString out;
    out.reserve(result.size());
    for (int i = 0; i < result.size(); ++i) {
        const QChar c = result.at(i);
        if (c == QLatin1Char(' ') || c == QChar('\t')) {
            const bool prevCjk = (i > 0) && isCjkIdeograph(result.at(i - 1));
            const bool nextCjk = (i + 1 < result.size()) && isCjkIdeograph(result.at(i + 1));
            if (prevCjk || nextCjk)
                continue;
        }
        out.append(c);
    }
    return out.trimmed();
}

} // namespace

ScreenTranslator::ScreenTranslator(QWidget* parent)
    : QWidget(parent),
    m_sourceLang("auto"),
    m_targetLang("zh-CN")
{
    // 初始化 Tesseract
    tesseract = new tesseract::TessBaseAPI();
    QString tessdataPath = QCoreApplication::applicationDirPath() + "/tessdata";
    if (tesseract->Init(tessdataPath.toUtf8().constData(), "eng+chi_sim")) {
        qWarning() << "Could not initialize tesseract.";
    }

    // 初始化翻译引擎管理器，注册内置引擎（默认 Google）
    engineManager = new TranslationEngineManager(this);
    engineManager->registerEngine(QStringLiteral("google"), new TranslationService(this));
    engineManager->registerEngine(QStringLiteral("custom"), new CustomOnlineTranslator(this));
    engineManager->registerEngine(QStringLiteral("offline"), new OfflineTranslator(this));
    connect(engineManager, &TranslationEngineManager::finished, this, &ScreenTranslator::onTranslationFinished);
    connect(engineManager, &TranslationEngineManager::error, this, &ScreenTranslator::onTranslationError);

    // 初始化 TTS
    tts = new TextToSpeech(this);
    tts->setRate(AppConfig::instance().ttsRate());
    tts->setVolume(AppConfig::instance().ttsVolume());

    // 初始化插件管理器并加载插件
    pluginManager = new PluginManager(this);
    loadPlugins();

    initUI();

    overlay = new CaptureOverlay();
    connect(overlay, &CaptureOverlay::regionSelected, this, &ScreenTranslator::onRegionSelected);

    // 配置变化时应用主题/字体/透明度
    connect(&AppConfig::instance(), &AppConfig::changed, this, &ScreenTranslator::onConfigChanged);
    applyTheme();

    // 初始化全局热键
    hotkey = new GlobalHotkey(this);
    qApp->installNativeEventFilter(hotkey);
    connect(hotkey, &GlobalHotkey::activated, this, [this](int id) {
        switch (id) {
        case ActCapture:      startCapture(); break;
        case ActLanguage:     showLanguageDialog(); break;
        case ActSettings:     openSettings(); break;
        case ActToggleWindow: if (isVisible()) hide(); else show(); break;
        case ActSpeakLast:    if (tts && !m_lastTranslated.isEmpty()) tts->speak(m_lastTranslated); break;
        default: break;
        }
    });
    registerHotkeys();
}

ScreenTranslator::~ScreenTranslator()
{
    tesseract->End();
    delete tesseract;
}

void ScreenTranslator::initUI()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName("MainWindow");
    setFixedSize(320, 700);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    closeBtn = new QPushButton("✕", this);
    closeBtn->setObjectName("CloseButton");
    closeBtn->setAccessibleName("关闭");
    closeBtn->setFixedSize(24, 24);
    connect(closeBtn, &QPushButton::clicked, this, &ScreenTranslator::closeWindow);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addStretch();
    topLayout->addWidget(closeBtn);
    topLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(topLayout);

    QLabel* hint = new QLabel("按 F1 截图 | F2 语言 | F4 设置 | 右键菜单更多", this);
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    historyTextEdit = new QTextEdit(this);
    historyTextEdit->setReadOnly(true);
    historyTextEdit->setAccessibleName("翻译历史记录");
    historyTextEdit->setFrameShape(QFrame::NoFrame);
    layout->addWidget(historyTextEdit, 1);

    // ---- 底部导出按钮（右下角） ----
    exportBtn = new QPushButton("导出记录", this);
    exportBtn->setAccessibleName("导出记录");
    exportBtn->setCursor(Qt::PointingHandCursor);
    connect(exportBtn, &QPushButton::clicked, this, &ScreenTranslator::exportRecords);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    bottomLayout->addWidget(exportBtn);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bottomLayout);

    // ---- 右键菜单 ----
    contextMenu = new QMenu(this);
    QAction* copyLastAct = contextMenu->addAction("复制最新记录");
    connect(copyLastAct, &QAction::triggered, this, &ScreenTranslator::copyLastRecord);
    QAction* copyAllAct = contextMenu->addAction("复制全部记录");
    connect(copyAllAct, &QAction::triggered, this, &ScreenTranslator::copyAllRecords);
    contextMenu->addSeparator();
    QAction* settingsAct = contextMenu->addAction("设置");
    connect(settingsAct, &QAction::triggered, this, &ScreenTranslator::openSettings);
    QAction* clearAct = contextMenu->addAction("清空记录");
    connect(clearAct, &QAction::triggered, this, &ScreenTranslator::clearHistory);
    QAction* exitAct = contextMenu->addAction("退出");
    connect(exitAct, &QAction::triggered, this, &ScreenTranslator::closeWindow);

    adjustToRightEdge();
}

void ScreenTranslator::adjustToRightEdge()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect screenGeometry = screen->availableGeometry();
    int x = screenGeometry.right() - width() - 10;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void ScreenTranslator::closeWindow() { qApp->quit(); }

void ScreenTranslator::openSettings()
{
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::testSpeechRequested, this, [this]() {
        if (tts) tts->speak(QStringLiteral("你好，这是翻译朗读测试。"));
    });

    QStringList engineIds = engineManager->engineIds();
    QStringList engineLabels;
    for (const QString& id : engineIds) {
        ITranslator* eng = engineManager->engine(id);
        engineLabels << (eng ? eng->name() : id);
    }
    dlg.setEngines(engineIds, engineLabels, AppConfig::instance().translatorEngine());

    connect(&dlg, &SettingsDialog::testTranslationRequested, this, [this](const QString& id) {
        engineManager->testTranslate(id, QStringLiteral("Hello"), QStringLiteral("en"), QStringLiteral("zh-CN"));
    });
    connect(engineManager, &TranslationEngineManager::testResult, &dlg,
        [](const QString&, bool ok, const QString& message) {
            if (ok)
                QMessageBox::information(nullptr, QStringLiteral("测试翻译"), QStringLiteral("翻译成功：\n%1").arg(message));
            else
                QMessageBox::warning(nullptr, QStringLiteral("测试翻译"), QStringLiteral("翻译失败：\n%1").arg(message));
        });

    QStringList pluginDescs;
    for (IPlugin* p : pluginManager->plugins())
        pluginDescs << QString("%1  v%2").arg(p->name(), p->version());
    dlg.setPlugins(pluginDescs);

    dlg.exec();
}

void ScreenTranslator::applyTheme()
{
    AppConfig& cfg = AppConfig::instance();

    if (qApp)
        ThemeManager::apply(*qApp, cfg.theme());

    QFont f(cfg.fontFamily(), cfg.fontSize());
    if (qApp)
        qApp->setFont(f);

    setWindowOpacity(cfg.windowOpacity() / 100.0);
    update();
}

void ScreenTranslator::onConfigChanged(const QString& key)
{
    if (key == "theme" || key == "fontFamily" || key == "fontSize" || key == "windowOpacity") {
        applyTheme();
    }
    else if (key == "ttsRate" && tts) {
        tts->setRate(AppConfig::instance().ttsRate());
    }
    else if (key == "ttsVolume" && tts) {
        tts->setVolume(AppConfig::instance().ttsVolume());
    }
    else if (key.startsWith("hotkey_")) {
        registerHotkeys();
    }
    else if (key == "translatorEngine") {
        engineManager->setCurrentEngine(AppConfig::instance().translatorEngine());
    }
}

void ScreenTranslator::registerHotkeys()
{
    if (!hotkey)
        return;
    hotkey->unregisterAll();
    registerOneHotkey(ActCapture, "capture", "F1");
    registerOneHotkey(ActLanguage, "language", "F2");
    registerOneHotkey(ActSettings, "settings", "F4");
    registerOneHotkey(ActToggleWindow, "toggleWindow", "");
    registerOneHotkey(ActSpeakLast, "speakLast", "");
}

void ScreenTranslator::registerOneHotkey(int id, const QString& action, const QString& def)
{
    QString seqStr = AppConfig::instance().hotkey(action);
    if (seqStr.isEmpty())
        seqStr = def;
    if (!seqStr.isEmpty())
        hotkey->registerHotkey(id, QKeySequence(seqStr));
}

void ScreenTranslator::loadPlugins()
{
    if (!pluginManager)
        return;

    const QString exeDir = QCoreApplication::applicationDirPath();
    QString pluginDir = exeDir + "/plugins";
    if (!QDir(pluginDir).exists())
        pluginDir = exeDir;
    pluginManager->loadPlugins(pluginDir);

    for (ITranslatorPlugin* p : pluginManager->translatorPlugins())
        engineManager->registerEngine(p->name(), new PluginTranslator(p, this));

    engineManager->setCurrentEngine(AppConfig::instance().translatorEngine());
}

void ScreenTranslator::clearHistory()
{
    historyList.clear();
    updateHistoryDisplay();
}

void ScreenTranslator::copyLastRecord()
{
    if (!historyList.isEmpty()) {
        QApplication::clipboard()->setText(historyList.last());
    }
}

void ScreenTranslator::copyAllRecords()
{
    QApplication::clipboard()->setText(historyList.join("\n"));
}

void ScreenTranslator::exportRecords()
{
    if (historyList.isEmpty()) {
        QMessageBox::information(this, "导出记录", "当前没有可导出的记录。");
        return;
    }

    QString defaultName = "translations_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filter = "文本文件 (*.txt);;Word 文档 (*.docx)";
    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(this, "导出记录", defaultName, filter, &selectedFilter);
    if (filePath.isEmpty())
        return;

    const bool wantDocx = selectedFilter.contains("docx");
    if (!QFileInfo(filePath).suffix().isEmpty()) {
        // 已有扩展名，尊重用户输入
    } else if (wantDocx) {
        filePath += ".docx";
    } else {
        filePath += ".txt";
    }

    bool ok = false;
    if (filePath.endsWith(".docx", Qt::CaseInsensitive)) {
        ok = DocxExporter::write(filePath, historyList);
    } else {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            ok = file.write(historyList.join("\n\n").toUtf8()) >= 0;
            file.close();
        }
    }

    if (ok)
        QMessageBox::information(this, "导出记录", QString("已导出 %1 条记录到：\n%2").arg(historyList.size()).arg(filePath));
    else
        QMessageBox::warning(this, "导出记录", "导出失败，请检查路径或权限。");
}

void ScreenTranslator::contextMenuEvent(QContextMenuEvent* event)
{
    contextMenu->exec(event->globalPos());
}

void ScreenTranslator::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void ScreenTranslator::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void ScreenTranslator::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}

void ScreenTranslator::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const QString theme = AppConfig::instance().theme();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(ThemeManager::windowBackground(theme));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);

    // 若未截图，显示提示文字（背景上）
    if (!hasCaptured && historyList.isEmpty()) {
        painter.setPen(ThemeManager::foregroundColor(theme));
        QFont font(AppConfig::instance().fontFamily(), AppConfig::instance().fontSize());
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, "屏幕翻译器\n按 F1 截图");
    }
}

void ScreenTranslator::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F1) {
        startCapture();
    }
    else if (event->key() == Qt::Key_F2) {
        showLanguageDialog();
    }
    else if (event->key() == Qt::Key_F4) {
        openSettings();
    }
    QWidget::keyPressEvent(event);
}

void ScreenTranslator::startCapture()
{
    hide();
    overlay->show();
    overlay->raise();
    overlay->activateWindow();
}

void ScreenTranslator::onRegionSelected(const QRect& rect)
{
    if (rect.isNull()) {
        show();
        return;
    }
    QScreen* screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height());
    processCapturedPixmap(pixmap);
    show();
}

void ScreenTranslator::processCapturedPixmap(const QPixmap& pixmap)
{
    QImage image = pixmap.toImage();
    cv::Mat mat = cv::Mat(image.height(), image.width(), CV_8UC4,
        const_cast<uchar*>(image.constBits()),
        image.bytesPerLine()).clone();
    cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGR);

    std::vector<cv::Mat> candidates;
    preprocessCandidates(mat, candidates);

    if (candidates.empty()) {
        qWarning() << "Preprocessed image is invalid!";
        return;
    }

    // 不再显示图像预览，只标记已捕获
    hasCaptured = true;
    update();

    // 异步 OCR + 翻译
    ocrFuture = std::async(std::launch::async, [this, candidates = std::move(candidates)]() {
        QString ocrResult = runOCR(candidates);
        if (ocrResult.isEmpty()) ocrResult = "No text found";

        QMetaObject::invokeMethod(this, [this, ocrResult]() {
            QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
            QString pendingEntry = QString("[%1] %2\n    → 翻译中...").arg(timestamp, ocrResult);
            historyList.append(pendingEntry);
            updateHistoryDisplay();

            engineManager->translate(ocrResult, m_sourceLang, m_targetLang);
            }, Qt::QueuedConnection);
        });
}

void ScreenTranslator::updateHistoryDisplay()
{
    historyTextEdit->setPlainText(historyList.join("\n"));
    QScrollBar* vbar = historyTextEdit->verticalScrollBar();
    if (vbar) vbar->setValue(vbar->maximum());
}

void ScreenTranslator::onTranslationFinished(const QString& translatedText, const QString& detectedLang)
{
    if (!historyList.isEmpty()) {
        QString lastEntry = historyList.last();
        lastEntry.replace("→ 翻译中...", QString("→ [%1] %2").arg(detectedLang, translatedText));
        historyList.last() = lastEntry;
        updateHistoryDisplay();
    }

    if (AppConfig::instance().ttsEnabled() && tts && tts->isAvailable())
        tts->speak(translatedText);

    m_lastTranslated = translatedText;
}

void ScreenTranslator::onTranslationError(const QString& errorMessage)
{
    if (!historyList.isEmpty()) {
        QString lastEntry = historyList.last();
        lastEntry.replace("→ 翻译中...", QString("→ 翻译失败: %1").arg(errorMessage));
        historyList.last() = lastEntry;
        updateHistoryDisplay();
    }
}

void ScreenTranslator::preprocessCandidates(const cv::Mat& src, std::vector<cv::Mat>& out)
{
    out.clear();

    cv::Mat gray;
    if (src.channels() == 3) {
        // 复杂彩色背景：按对比度（标准差）选择 R/G/B 中最能区分文字与背景的通道，
        // 避免亮度转灰度在文字与背景亮度相近、但色相不同时丢失对比度。
        cv::Mat chans[3];
        cv::split(src, chans);
        int best = 0;
        double bestStd = -1.0;
        for (int c = 0; c < 3; ++c) {
            cv::Scalar m, s;
            cv::meanStdDev(chans[c], m, s);
            if (s[0] > bestStd) {
                bestStd = s[0];
                best = c;
            }
        }
        gray = chans[best];
    } else {
        gray = src.clone();
    }

    // 用 Otsu 自动判断文字极性（深字浅底 or 浅字深底）。文字通常是像素较少的那一类：
    // 若亮色类别像素更少（浅字深底/灰底浅字），则反转，使后续统一按“深字浅底”处理。
    // 这比固定用平均亮度 128 判断更稳，能正确识别灰色背景下的浅色/深色文字。
    cv::Mat otsuBin;
    const double otsu = cv::threshold(gray, otsuBin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    const int darkCount = cv::countNonZero(gray <= otsu);
    const int brightCount = static_cast<int>(gray.total()) - darkCount;
    if (brightCount < darkCount)
        cv::bitwise_not(gray, gray);   // 反转灰度图，确保后续处理的是“白底黑字”

    // 放大、锐化、CLAHE、去噪等共同增强
    cv::resize(gray, gray, cv::Size(), 2.0, 2.0, cv::INTER_CUBIC);

    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_16S, 3);
    cv::convertScaleAbs(laplacian, laplacian);
    cv::addWeighted(gray, 1.5, laplacian, -0.5, 0, gray);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    clahe->apply(gray, gray);

    cv::medianBlur(gray, gray, 3);

    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));

    // 多策略二值化，产生多个候选，交由 runOCR 用置信度择优：
    // 1) 全局 Otsu —— 适合纯色/均匀背景（如纯灰底黑字）
    cv::Mat b1;
    cv::threshold(gray, b1, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::morphologyEx(b1, b1, cv::MORPH_CLOSE, kernel);
    out.push_back(b1);

    // 2) 自适应高斯（小窗）—— 适合光照不均/复杂背景
    cv::Mat b2;
    cv::adaptiveThreshold(gray, b2, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 11, 2);
    cv::morphologyEx(b2, b2, cv::MORPH_CLOSE, kernel);
    out.push_back(b2);

    // 3) 自适应均值
    cv::Mat b3;
    cv::adaptiveThreshold(gray, b3, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 11, 2);
    cv::morphologyEx(b3, b3, cv::MORPH_CLOSE, kernel);
    out.push_back(b3);

    // 4) 自适应高斯（大窗）—— 适合较大字号/更不均匀背景
    cv::Mat b4;
    cv::adaptiveThreshold(gray, b4, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 25, 5);
    cv::morphologyEx(b4, b4, cv::MORPH_CLOSE, kernel);
    out.push_back(b4);
}

QImage ScreenTranslator::matToQImage(const cv::Mat& mat)
{
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    }
    else if (mat.type() == CV_8UC3) {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888);
        return img.rgbSwapped().copy();
    }
    return QImage();
}

QString ScreenTranslator::runOCR(const std::vector<cv::Mat>& images)
{
    std::lock_guard<std::mutex> lock(tessMutex);
    if (!tesseract) return "";

    QString bestText;
    int bestConf = -1;

    for (const cv::Mat& image : images) {
        if (image.empty())
            continue;

        tesseract->SetImage(image.data, image.cols, image.rows, 1, image.step);
        tesseract->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
        char* outText = tesseract->GetUTF8Text();
        const QString text = cleanupOcrText(QString::fromUtf8(outText));
        delete[] outText;
        if (text.isEmpty())
            continue;

        // 取置信度最高的候选结果
        const int conf = tesseract->MeanTextConf();
        if (conf > bestConf) {
            bestConf = conf;
            bestText = text;
        }
    }
    return bestText;
}

void ScreenTranslator::showLanguageDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("翻译语言设置");
    dlg.setFixedSize(300, 200);

    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    // 源语言
    QLabel* srcLabel = new QLabel("源语言：");
    QComboBox* srcCombo = new QComboBox();
    srcCombo->addItem("自动检测", "auto");
    srcCombo->addItem("英语", "en");
    srcCombo->addItem("中文（简体）", "zh-CN");
    srcCombo->addItem("日语", "ja");
    srcCombo->addItem("韩语", "ko");
    srcCombo->addItem("法语", "fr");
    srcCombo->addItem("德语", "de");
    srcCombo->addItem("西班牙语", "es");

    int srcIndex = srcCombo->findData(m_sourceLang);
    if (srcIndex >= 0) srcCombo->setCurrentIndex(srcIndex);

    layout->addWidget(srcLabel);
    layout->addWidget(srcCombo);

    // 目标语言
    QLabel* tgtLabel = new QLabel("目标语言：");
    QComboBox* tgtCombo = new QComboBox();
    tgtCombo->addItem("中文（简体）", "zh-CN");
    tgtCombo->addItem("英语", "en");
    tgtCombo->addItem("日语", "ja");
    tgtCombo->addItem("韩语", "ko");
    tgtCombo->addItem("法语", "fr");
    tgtCombo->addItem("德语", "de");
    tgtCombo->addItem("西班牙语", "es");
    tgtCombo->addItem("中文（繁体）", "zh-TW");

    int tgtIndex = tgtCombo->findData(m_targetLang);
    if (tgtIndex >= 0) tgtCombo->setCurrentIndex(tgtIndex);

    layout->addWidget(tgtLabel);
    layout->addWidget(tgtCombo);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dlg.exec() == QDialog::Accepted) {
        m_sourceLang = srcCombo->currentData().toString();
        m_targetLang = tgtCombo->currentData().toString();
    }
}