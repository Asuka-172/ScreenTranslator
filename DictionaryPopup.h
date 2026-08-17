#ifndef DICTIONARYPOPUP_H
#define DICTIONARYPOPUP_H

#include <QWidget>

class QLabel;
class QTextEdit;
class QPushButton;

// 划词查询结果浮窗（点击外部自动关闭）。
class DictionaryPopup : public QWidget
{
    Q_OBJECT
public:
    explicit DictionaryPopup(QWidget* parent = nullptr);

    void showResult(const QString& word, const QString& formatted);
    void showError(const QString& message);

signals:
    void speakRequested(const QString& word);

private:
    QString m_word;
    QLabel* m_wordLabel;
    QTextEdit* m_text;
    QPushButton* m_speakBtn;
};

#endif
