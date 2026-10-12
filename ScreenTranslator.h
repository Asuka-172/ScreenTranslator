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
#include <QPointer>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <opencv2/opencv.hpp>
#include "CaptureOverlay.h"

class PluginManager;
class TextToSpeech;
class GlobalHotkey;
class TranslationEngineManager;
class DocumentTranslator;
class QProgressDialog;

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
    void importDocument();
    void onTranslationFinished(const QString& translatedText, const QString& detectedLang, quint64 requestId);
    void onTranslationError(const QString& errorMessage, quint64 requestId);
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
    void syncBaiduEnv();
    void preprocessCandidates(const cv::Mat& src, std::vector<cv::Mat>& out);
    QImage matToQImage(const cv::Mat& mat);
    QString runOCR(const std::vector<cv::Mat>& images);
    void processCapturedPixmap(const QPixmap& pixmap);
    void updateHistoryDisplay();
    void closeDocProgress();

    // UI 组件
    QPushButton* closeBtn;
    QPushButton* exportBtn;
    QPushButton* importBtn;
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

    // 文档翻译编排器（逐段串行翻译）
    DocumentTranslator* docTranslator;

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

    // 文档导入翻译状态（QPointer：对象被销毁时自动置空，避免悬垂指针崩溃）
    QPointer<QProgressDialog> m_docProgress;
    QString m_docOutputPath;
    bool m_docTranslating = false;

    // 翻译 API 计时起点（毫秒，0 表示未开始）
    qint64 m_translateStartMs = 0;
};

#endif