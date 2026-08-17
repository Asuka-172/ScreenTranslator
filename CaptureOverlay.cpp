#include "CaptureOverlay.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QShowEvent>            // 记得包含
#include <QGuiApplication>
#include <QScreen>

CaptureOverlay::CaptureOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        setGeometry(screen->geometry());
    }
    setCursor(Qt::CrossCursor);
}

CaptureOverlay::~CaptureOverlay() {}

void CaptureOverlay::showEvent(QShowEvent* event)
{
    // 每次显示遮罩时，清空旧选区，重置状态
    selecting = false;
    selectedRect = QRect();
    QWidget::showEvent(event);
}

void CaptureOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    // 半透明黑色遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 80));

    if (selecting || !selectedRect.isNull()) {
        QRect r = selectedRect.normalized();
        // 选区镂空
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(r, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(Qt::red, 2));
        painter.drawRect(r);
    }
}

void CaptureOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        selecting = true;
        origin = event->pos();
        selectedRect = QRect(origin, QSize());
        update();
    }
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (selecting) {
        selectedRect = QRect(origin, event->pos());
        update();
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        selecting = false;
        if (selectedRect.width() > 10 && selectedRect.height() > 10) {
            hide();
            emit regionSelected(selectedRect);
        }
        else {
            selectedRect = QRect();
            update();
        }
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        selectedRect = QRect();
        hide();
        emit regionSelected(QRect());
    }
    QWidget::keyPressEvent(event);
}