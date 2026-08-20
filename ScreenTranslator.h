#ifndef SCREENTRANSLATOR_H
#define SCREENTRANSLATOR_H

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QPoint>
#include <QTextEdit>
#include <QStringList>
#include <future>
#include <mutex>
#include <vector>
#include <QScrollBar>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <opencv2/opencv.hpp>
#include "CaptureOverlay.h"

class PluginManager;
class TextToSpeech;
class GlobalHotkey;
class TranslationEngineManager;

class ScreenTranslator : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenTranslator(QWidget* parent = nullptr);
    ~ScreenTranslator();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void startCapture();
    void onRegionSelected(const QRect& rect);
    void closeWindow();
    void clearHistory();
    void copyLastRecord();
    void copyAllRecords();
    void exportRecords();
    void onTranslationFinished(const QString& translatedText, const QString& detectedLang);
    void onTranslationError(const QString& errorMessage);
    void showLanguageDialog();
    void openSettings();

private:
    void initUI();
    void adjustToRightEdge();
    void applyTheme();
    void onConfigChanged(const QString& key);
    void registerHotkeys();
    void registerOneHotkey(int id, const QString& action, const QString& def);
    void loadPlugins();
    void preprocessCandidates(const cv::Mat& src, std::vector<cv::Mat>& out);
    QImage matToQImage(const cv::Mat& mat);
    QString runOCR(const std::vector<cv::Mat>& images);
    void processCapturedPixmap(const QPixmap& pixmap);
    void updateHistoryDisplay();

    // UI 组件
    QPushButton* closeBtn;
    QPushButton* exportBtn;
    QTextEdit* historyTextEdit;   // 替换原来的 QLabel + QScrollArea
    QMenu* contextMenu;

    // 拖拽
    bool dragging = false;
    QPoint dragPosition;

    // 截图遮罩
    CaptureOverlay* overlay;

    // 图像状态
    cv::Mat currentMat;
    bool hasCaptured = false;

    // Tesseract
    tesseract::TessBaseAPI* tesseract;
    std::mutex tessMutex;

    // 异步 OCR 任务
    std::future<void> ocrFuture;

    // 历史记录列表
    QStringList historyList;

    // 翻译引擎管理器（注册/切换/回退/缓存）
    TranslationEngineManager* engineManager;

    // 插件管理
    PluginManager* pluginManager;

    // TTS
    TextToSpeech* tts;

    // 全局热键
    GlobalHotkey* hotkey;

    // 语言
    QString m_sourceLang;
    QString m_targetLang;

    // 最近一条译文（用于"朗读最近一条"）
    QString m_lastTranslated;
};

#endif