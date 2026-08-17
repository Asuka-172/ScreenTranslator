#include "DictionaryPopup.h"
#include "AppConfig.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFont>
#include <QFrame>

DictionaryPopup::DictionaryPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup)
{
    setObjectName("DictionaryPopup");
    setAttribute(Qt::WA_StyledBackground);

    const QString theme = AppConfig::instance().theme();
    const QString bg = theme == "light" ? "#FFFFFF"
        : (theme == "highcontrast" ? "#000000" : "#2B2B2B");
    const QString border = theme == "highcontrast" ? "#FFFF00" : "#888888";
    setStyleSheet(QString("QWidget#DictionaryPopup { background: %1; border: 1px solid %2; border-radius: 6px; }")
                      .arg(bg, border));

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 10, 12, 10);

    QHBoxLayout* top = new QHBoxLayout();
    m_wordLabel = new QLabel(this);
    QFont f = m_wordLabel->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    m_wordLabel->setFont(f);
    top->addWidget(m_wordLabel);
    top->addStretch();

    m_speakBtn = new QPushButton("朗读", this);
    m_speakBtn->setFixedWidth(56);
    connect(m_speakBtn, &QPushButton::clicked, this, [this]() {
        emit speakRequested(m_word);
    });
    top->addWidget(m_speakBtn);
    lay->addLayout(top);

    m_text = new QTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setFixedHeight(200);
    lay->addWidget(m_text);

    setFixedWidth(400);
}

void DictionaryPopup::showResult(const QString& word, const QString& formatted)
{
    m_word = word;
    m_wordLabel->setText(word);
    m_text->setPlainText(formatted);
}

void DictionaryPopup::showError(const QString& message)
{
    m_word.clear();
    m_wordLabel->setText(QString());
    m_text->setPlainText(message);
}
