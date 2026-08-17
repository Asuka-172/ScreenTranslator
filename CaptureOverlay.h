#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include <QWidget>
#include <QRect>
#include <QPoint>

class CaptureOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit CaptureOverlay(QWidget* parent = nullptr);
    ~CaptureOverlay();

signals:
    void regionSelected(const QRect& rect);   // 选区完成信号

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    bool selecting = false;
    QPoint origin;
    QRect selectedRect;
};

#endif