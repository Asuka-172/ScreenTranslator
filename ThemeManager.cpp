#include "ThemeManager.h"
#include <QApplication>

namespace {

QString darkQss()
{
    return R"(
QWidget { color: #E0E0E0; font-size: 14px; }
QWidget#MainWindow { background: transparent; }
QLabel { background: transparent; color: #E0E0E0; }
QTextEdit { color: #FFFFFF; background: rgba(0,0,0,120); border-radius: 5px; padding: 8px; border: none; }
QPushButton { color: #FFFFFF; background: rgba(255,255,255,40); border: 1px solid rgba(255,255,255,60); border-radius: 5px; padding: 6px 16px; }
QPushButton:hover { background: rgba(255,255,255,80); }
QPushButton:pressed { background: rgba(255,255,255,30); }
QPushButton#CloseButton { background: rgba(255,255,255,180); color: black; border: none; border-radius: 12px; font-weight: bold; }
QPushButton#CloseButton:hover { background: rgba(255,0,0,180); color: white; }
QMenu { background: #2B2B2B; color: #E0E0E0; border: 1px solid #444; }
QMenu::item:selected { background: #3A3A3A; }
QComboBox { background: #2B2B2B; color: #E0E0E0; border: 1px solid #555; border-radius: 4px; padding: 4px 8px; }
QComboBox QAbstractItemView { background: #2B2B2B; color: #E0E0E0; selection-background-color: #3A3A3A; }
QCheckBox { background: transparent; color: #E0E0E0; }
QSlider::groove:horizontal { height: 4px; background: #444; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #FFFFFF; border-radius: 7px; }
QTabWidget::pane { border: 1px solid #444; }
QTabBar::tab { background: #2B2B2B; color: #E0E0E0; padding: 6px 14px; border: 1px solid #444; border-bottom: none; }
QTabBar::tab:selected { background: #3A3A3A; color: #FFFFFF; }
QScrollBar:vertical { background: transparent; width: 8px; }
QScrollBar::handle:vertical { background: rgba(255,255,255,80); border-radius: 4px; min-height: 20px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0px; }
QScrollBar:horizontal { background: transparent; height: 8px; }
QScrollBar::handle:horizontal { background: rgba(255,255,255,80); border-radius: 4px; min-width: 20px; }
QDialog { background: #2B2B2B; }
)";
}

QString lightQss()
{
    return R"(
QWidget { color: #202020; font-size: 14px; }
QWidget#MainWindow { background: transparent; }
QLabel { background: transparent; color: #202020; }
QTextEdit { color: #101010; background: rgba(255,255,255,200); border-radius: 5px; padding: 8px; border: none; }
QPushButton { color: #101010; background: rgba(0,0,0,15); border: 1px solid rgba(0,0,0,40); border-radius: 5px; padding: 6px 16px; }
QPushButton:hover { background: rgba(0,0,0,40); }
QPushButton:pressed { background: rgba(0,0,0,60); }
QPushButton#CloseButton { background: rgba(0,0,0,180); color: white; border: none; border-radius: 12px; font-weight: bold; }
QPushButton#CloseButton:hover { background: rgba(255,0,0,180); color: white; }
QMenu { background: #FFFFFF; color: #202020; border: 1px solid #CCC; }
QMenu::item:selected { background: #E8E8E8; }
QComboBox { background: #FFFFFF; color: #202020; border: 1px solid #CCC; border-radius: 4px; padding: 4px 8px; }
QComboBox QAbstractItemView { background: #FFFFFF; color: #202020; selection-background-color: #E8E8E8; }
QCheckBox { background: transparent; color: #202020; }
QSlider::groove:horizontal { height: 4px; background: #CCC; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #606060; border-radius: 7px; }
QTabWidget::pane { border: 1px solid #CCC; }
QTabBar::tab { background: #F0F0F0; color: #202020; padding: 6px 14px; border: 1px solid #CCC; border-bottom: none; }
QTabBar::tab:selected { background: #FFFFFF; color: #000000; }
QScrollBar:vertical { background: transparent; width: 8px; }
QScrollBar::handle:vertical { background: rgba(0,0,0,60); border-radius: 4px; min-height: 20px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0px; }
QScrollBar:horizontal { background: transparent; height: 8px; }
QScrollBar::handle:horizontal { background: rgba(0,0,0,60); border-radius: 4px; min-width: 20px; }
QDialog { background: #F0F0F0; }
)";
}

QString highContrastQss()
{
    return R"(
QWidget { color: #FFFF00; font-size: 14px; }
QWidget#MainWindow { background: transparent; }
QLabel { background: transparent; color: #FFFF00; }
QTextEdit { color: #FFFF00; background: #000000; border-radius: 5px; padding: 8px; border: 2px solid #FFFF00; }
QPushButton { color: #FFFF00; background: #000000; border: 2px solid #FFFF00; border-radius: 5px; padding: 6px 16px; }
QPushButton:hover { background: #FFFF00; color: #000000; }
QPushButton:pressed { background: #CCCC00; color: #000000; }
QPushButton#CloseButton { background: #000000; color: #FFFF00; border: 2px solid #FFFF00; border-radius: 12px; font-weight: bold; }
QPushButton#CloseButton:hover { background: #FFFF00; color: #000000; }
QMenu { background: #000000; color: #FFFF00; border: 2px solid #FFFF00; }
QMenu::item:selected { background: #FFFF00; color: #000000; }
QComboBox { background: #000000; color: #FFFF00; border: 2px solid #FFFF00; border-radius: 4px; padding: 4px 8px; }
QComboBox QAbstractItemView { background: #000000; color: #FFFF00; selection-background-color: #FFFF00; selection-color: #000000; }
QCheckBox { background: transparent; color: #FFFF00; }
QSlider::groove:horizontal { height: 4px; background: #FFFF00; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #FFFF00; border: 1px solid #000000; border-radius: 7px; }
QTabWidget::pane { border: 2px solid #FFFF00; }
QTabBar::tab { background: #000000; color: #FFFF00; padding: 6px 14px; border: 2px solid #FFFF00; border-bottom: none; }
QTabBar::tab:selected { background: #FFFF00; color: #000000; }
QScrollBar:vertical { background: transparent; width: 10px; }
QScrollBar::handle:vertical { background: #FFFF00; border-radius: 5px; min-height: 20px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0px; }
QScrollBar:horizontal { background: transparent; height: 10px; }
QScrollBar::handle:horizontal { background: #FFFF00; border-radius: 5px; min-width: 20px; }
QDialog { background: #000000; }
)";
}

} // namespace

QString ThemeManager::styleSheet(const QString& theme)
{
    if (theme == "light") return lightQss();
    if (theme == "highcontrast") return highContrastQss();
    return darkQss();
}

QColor ThemeManager::windowBackground(const QString& theme)
{
    if (theme == "light") return QColor(245, 245, 245, 235);
    if (theme == "highcontrast") return QColor(0, 0, 0, 255);
    return QColor(30, 30, 30, 200);
}

QColor ThemeManager::foregroundColor(const QString& theme)
{
    if (theme == "light") return QColor(30, 30, 30);
    if (theme == "highcontrast") return QColor(255, 255, 0);
    return QColor(255, 255, 255);
}

void ThemeManager::apply(QApplication& app, const QString& theme)
{
    app.setStyleSheet(styleSheet(theme));
}
