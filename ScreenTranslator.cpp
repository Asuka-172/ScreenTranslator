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
#include <opencv2/imgproc.hpp>
#include "DocxExporter.h"

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

    // 初始化翻译服务
    translator = new TranslationService(this);
    connect(translator, &TranslationService::translationFinished, this, &ScreenTranslator::onTranslationFinished);
    connect(translator, &TranslationService::translationError, this, &ScreenTranslator::onTranslationError);

    initUI();

    overlay = new CaptureOverlay();
    connect(overlay, &CaptureOverlay::regionSelected, this, &ScreenTranslator::onRegionSelected);
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
    setFixedSize(320, 700);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    closeBtn = new QPushButton("✕", this);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.7); border-radius: 12px; "
        "border: none; color: black; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background: rgba(255,0,0,180); color: white; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &ScreenTranslator::closeWindow);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addStretch();
    topLayout->addWidget(closeBtn);
    topLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(topLayout);

    QLabel* hint = new QLabel("按 F1 截图 | F2 语言设置 | 右键菜单更多", this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: rgba(255,255,255,180); font-size: 11px;");
    layout->addWidget(hint);

    historyTextEdit = new QTextEdit(this);
    historyTextEdit->setReadOnly(true);            
    historyTextEdit->setFrameShape(QFrame::NoFrame);
    historyTextEdit->setStyleSheet(
        "QTextEdit { color: white; font-size: 14px; background: rgba(0,0,0,120); "
        "border-radius: 5px; padding: 8px; }"
        "QScrollBar:vertical { background: transparent; width: 6px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,80); border-radius: 3px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; }"
    );
    layout->addWidget(historyTextEdit, 1);

    // ---- 底部导出按钮（右下角） ----
    exportBtn = new QPushButton("导出记录", this);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(
        "QPushButton { color: white; font-size: 14px; background: rgba(255,255,255,40); "
        "border: 1px solid rgba(255,255,255,60); border-radius: 5px; padding: 6px 16px; }"
        "QPushButton:hover { background: rgba(255,255,255,80); }"
        "QPushButton:pressed { background: rgba(255,255,255,30); }"
    );
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(30, 30, 30, 200));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);

    // 若未截图，显示提示文字（背景上）
    if (!hasCaptured && historyList.isEmpty()) {
        painter.setPen(QColor(255, 255, 255, 180));
        QFont font("Microsoft YaHei", 12);
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

    cv::Mat processed;
    preprocessImage(mat, processed);

    if (processed.empty() || cv::countNonZero(processed) == 0) {
        qWarning() << "Preprocessed image is invalid!";
        return;
    }

    // 不再显示图像预览，只标记已捕获
    hasCaptured = true;
    update();

    // 异步 OCR + 翻译
    ocrFuture = std::async(std::launch::async, [this, processed = processed.clone()]() {
        QString ocrResult = runOCR(processed);
        if (ocrResult.isEmpty()) ocrResult = "No text found";

        QMetaObject::invokeMethod(this, [this, ocrResult]() {
            QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
            QString pendingEntry = QString("[%1] %2\n    → 翻译中...").arg(timestamp, ocrResult);
            historyList.append(pendingEntry);
            updateHistoryDisplay();

            translator->translate(ocrResult, m_sourceLang, m_targetLang);
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

void ScreenTranslator::preprocessImage(const cv::Mat& src, cv::Mat& dst)
{
    cv::Mat gray;
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src.clone();

    // ------------- 关键改动：在增强之前判断并反转 -------------
    // 计算原始灰度图的平均亮度，若平均亮度 < 128 说明背景偏暗（深底浅字）
    cv::Scalar meanVal = cv::mean(gray);
    if (meanVal[0] < 128) {
        cv::bitwise_not(gray, gray);   // 反转灰度图，确保后续处理的是“白底黑字”
    }
    // -----------------------------------------------------------

    // 放大、锐化、CLAHE 等增强（和之前一样，但作用于可能已反转的图像）
    cv::resize(gray, gray, cv::Size(), 2.0, 2.0, cv::INTER_CUBIC);

    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_16S, 3);
    cv::convertScaleAbs(laplacian, laplacian);
    cv::addWeighted(gray, 1.5, laplacian, -0.5, 0, gray);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    clahe->apply(gray, gray);

    cv::medianBlur(gray, gray, 3);

    // 自适应阈值（此时文字一定是暗色，背景是亮色）
    cv::Mat binary;
    cv::adaptiveThreshold(gray, binary, 255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY, 11, 2);

    // 闭运算连接笔画
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(binary, dst, cv::MORPH_CLOSE, kernel);

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

QString ScreenTranslator::runOCR(const cv::Mat& image)
{
    std::lock_guard<std::mutex> lock(tessMutex);
    if (!tesseract || image.empty()) return "";
    tesseract->SetImage(image.data, image.cols, image.rows, 1, image.step);
    tesseract->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
    char* outText = tesseract->GetUTF8Text();
    QString result = QString::fromUtf8(outText);
    delete[] outText;
    return result.trimmed();
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