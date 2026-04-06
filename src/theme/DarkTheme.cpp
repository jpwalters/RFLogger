#include "DarkTheme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

void DarkTheme::apply(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window,          QColor(30, 30, 30));
    palette.setColor(QPalette::WindowText,      QColor(208, 208, 208));
    palette.setColor(QPalette::Base,            QColor(22, 22, 22));
    palette.setColor(QPalette::AlternateBase,   QColor(40, 40, 40));
    palette.setColor(QPalette::ToolTipBase,     QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipText,     QColor(208, 208, 208));
    palette.setColor(QPalette::Text,            QColor(208, 208, 208));
    palette.setColor(QPalette::Button,          QColor(45, 45, 45));
    palette.setColor(QPalette::ButtonText,      QColor(208, 208, 208));
    palette.setColor(QPalette::BrightText,      QColor(255, 51, 51));
    palette.setColor(QPalette::Link,            QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, QColor(240, 240, 240));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 100, 100));
    palette.setColor(QPalette::Disabled, QPalette::Text,       QColor(100, 100, 100));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));

    app.setPalette(palette);

    app.setStyleSheet(R"(
        QToolTip {
            color: #d0d0d0;
            background-color: #2d2d2d;
            border: 1px solid #555;
            padding: 4px;
        }
        QGroupBox {
            border: 1px solid #555;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
        QStatusBar {
            background-color: #1a1a2e;
            color: #d0d0d0;
        }
        QDockWidget {
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background-color: #2a2a3e;
            padding: 6px;
            font-weight: bold;
        }
        QMenuBar {
            background-color: #2d2d2d;
            border-bottom: 1px solid #444;
        }
        QMenuBar::item:selected {
            background-color: #3d3d5c;
        }
        QMenu {
            background-color: #2d2d2d;
            border: 1px solid #555;
        }
        QMenu::item:selected {
            background-color: #3d3d5c;
        }
        QPushButton {
            background-color: #3d3d5c;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 6px 16px;
            min-width: 60px;
        }
        QPushButton:hover {
            background-color: #4d4d6c;
        }
        QPushButton:pressed {
            background-color: #2a2a4e;
        }
        QPushButton:disabled {
            background-color: #2d2d2d;
            color: #666;
        }
        QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit {
            background-color: #222;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 4px;
            color: #d0d0d0;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QLineEdit:focus {
            border: 1px solid #2a82da;
        }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 16px;
            border-left: 1px solid #555;
            border-bottom: 1px solid #555;
            background-color: #3a3a3a;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 16px;
            border-left: 1px solid #555;
            border-top: 1px solid #555;
            background-color: #3a3a3a;
        }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
            background-color: #505050;
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: url(:/icons/arrow-up.svg);
            width: 8px;
            height: 8px;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: url(:/icons/arrow-down.svg);
            width: 8px;
            height: 8px;
        }
        QSplitter::handle {
            background-color: #444;
        }
        QSplitter::handle:horizontal { width: 3px; }
        QSplitter::handle:vertical   { height: 3px; }
        QTabWidget::pane {
            border: 1px solid #555;
        }
        QTabBar::tab {
            background-color: #2d2d2d;
            border: 1px solid #555;
            padding: 6px 12px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #3d3d5c;
            border-bottom-color: #3d3d5c;
        }
        QScrollBar:vertical {
            background: #1e1e1e;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background: #555;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");
}
