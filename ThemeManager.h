#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>
#include <QColor>

class QApplication;

// 统一管理三套主题的 QSS 与主窗口背景色。
class ThemeManager
{
public:
    static QString styleSheet(const QString& theme);
    static QColor windowBackground(const QString& theme);
    static QColor foregroundColor(const QString& theme);
    static void apply(QApplication& app, const QString& theme);
};

#endif
