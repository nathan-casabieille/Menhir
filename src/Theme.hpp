#pragma once
#include <QColor>
#include <QPalette>
#include <QString>

namespace Theme {

// ── Catppuccin Latte palette ──────────────────────────────────────────────────

namespace C {
    inline const QColor base      {0xef, 0xf1, 0xf5};
    inline const QColor mantle    {0xe6, 0xe9, 0xef};
    inline const QColor crust     {0xdc, 0xe0, 0xe8};
    inline const QColor surface0  {0xcc, 0xd0, 0xda};
    inline const QColor surface1  {0xbc, 0xc0, 0xcc};
    inline const QColor surface2  {0xac, 0xb0, 0xbe};
    inline const QColor overlay0  {0x9c, 0xa0, 0xb0};
    inline const QColor overlay1  {0x8c, 0x8f, 0xa1};
    inline const QColor overlay2  {0x7c, 0x7f, 0x93};
    inline const QColor subtext0  {0x6c, 0x6f, 0x85};
    inline const QColor subtext1  {0x5c, 0x5f, 0x77};
    inline const QColor text      {0x4c, 0x4f, 0x69};
    inline const QColor lavender  {0x72, 0x87, 0xfd};
    inline const QColor blue      {0x1e, 0x66, 0xf5};
    inline const QColor sapphire  {0x20, 0x9f, 0xb5};
    inline const QColor sky       {0x04, 0xa5, 0xe5};
    inline const QColor teal      {0x17, 0x92, 0x99};
    inline const QColor green     {0x40, 0xa0, 0x2b};
    inline const QColor yellow    {0xdf, 0x8e, 0x1d};
    inline const QColor peach     {0xfe, 0x64, 0x0b};
    inline const QColor maroon    {0xe6, 0x45, 0x53};
    inline const QColor red       {0xd2, 0x0f, 0x39};
    inline const QColor mauve     {0x88, 0x39, 0xef};
    inline const QColor pink      {0xea, 0x76, 0xcb};
    inline const QColor flamingo  {0xdd, 0x78, 0x78};
    inline const QColor rosewater {0xdc, 0x8a, 0x78};
}

// ── CSS-string companions (for inline setStyleSheet calls) ────────────────────

namespace CS {
    inline const QString base     = "#eff1f5";
    inline const QString mantle   = "#e6e9ef";
    inline const QString crust    = "#dce0e8";
    inline const QString surface0 = "#ccd0da";
    inline const QString surface1 = "#bcc0cc";
    inline const QString surface2 = "#acb0be";
    inline const QString overlay0 = "#9ca0b0";
    inline const QString overlay1 = "#8c8fa1";
    inline const QString subtext0 = "#6c6f85";
    inline const QString subtext1 = "#5c5f77";
    inline const QString text     = "#4c4f69";
    inline const QString blue     = "#1e66f5";
    inline const QString sapphire = "#209fb5";
    inline const QString teal     = "#179299";
    inline const QString green    = "#40a02b";
    inline const QString red      = "#d20f39";
    inline const QString mauve    = "#8839ef";
    inline const QString yellow   = "#df8e1d";
    inline const QString peach    = "#fe640b";
    inline const QString maroon   = "#e64553";
}

// ── Highlight colors (translucent, visible on light background) ───────────────

inline const QColor ITEM_COLORS[] = {
    QColor(0x1e, 0x66, 0xf5, 50),   // blue
    QColor(0x88, 0x39, 0xef, 50),   // mauve
    QColor(0x17, 0x92, 0x99, 50),   // teal
    QColor(0xdf, 0x8e, 0x1d, 60),   // yellow
    QColor(0x40, 0xa0, 0x2b, 50),   // green
    QColor(0xfe, 0x64, 0x0b, 50),   // peach
    QColor(0x04, 0xa5, 0xe5, 50),   // sky
    QColor(0xea, 0x76, 0xcb, 50),   // pink
    QColor(0xd2, 0x0f, 0x39, 45),   // red
    QColor(0xe6, 0x45, 0x53, 45),   // maroon
};
inline const QColor ITEM_COLORS_STRONG[] = {
    QColor(0x1e, 0x66, 0xf5, 120),
    QColor(0x88, 0x39, 0xef, 120),
    QColor(0x17, 0x92, 0x99, 120),
    QColor(0xdf, 0x8e, 0x1d, 130),
    QColor(0x40, 0xa0, 0x2b, 120),
    QColor(0xfe, 0x64, 0x0b, 120),
    QColor(0x04, 0xa5, 0xe5, 120),
    QColor(0xea, 0x76, 0xcb, 120),
    QColor(0xd2, 0x0f, 0x39, 110),
    QColor(0xe6, 0x45, 0x53, 110),
};
inline constexpr int ITEM_COLOR_COUNT = 10;

// ── Palette ───────────────────────────────────────────────────────────────────

inline QPalette makePalette() {
    QPalette p;
    p.setColor(QPalette::Window,          C::base);
    p.setColor(QPalette::WindowText,      C::text);
    p.setColor(QPalette::Base,            C::mantle);
    p.setColor(QPalette::AlternateBase,   C::crust);
    p.setColor(QPalette::Text,            C::text);
    p.setColor(QPalette::Button,          C::surface0);
    p.setColor(QPalette::ButtonText,      C::text);
    p.setColor(QPalette::BrightText,      C::red);
    p.setColor(QPalette::Highlight,       C::blue);
    p.setColor(QPalette::HighlightedText, C::base);
    p.setColor(QPalette::Link,            C::blue);
    p.setColor(QPalette::LinkVisited,     C::mauve);
    p.setColor(QPalette::ToolTipBase,     C::crust);
    p.setColor(QPalette::ToolTipText,     C::text);
    p.setColor(QPalette::PlaceholderText, C::overlay0);
    p.setColor(QPalette::Mid,             C::surface1);
    p.setColor(QPalette::Dark,            C::crust);
    p.setColor(QPalette::Shadow,          C::surface2);
    p.setColor(QPalette::Disabled, QPalette::WindowText,  C::overlay0);
    p.setColor(QPalette::Disabled, QPalette::Text,        C::overlay0);
    p.setColor(QPalette::Disabled, QPalette::ButtonText,  C::overlay0);
    p.setColor(QPalette::Disabled, QPalette::Button,      C::surface0);
    return p;
}

// ── Stylesheet ────────────────────────────────────────────────────────────────

inline QString styleSheet() {
    return R"(
QMainWindow, QDialog {
    background: #eff1f5;
}
QMenuBar {
    background: #e6e9ef;
    color: #4c4f69;
    border-bottom: 1px solid #ccd0da;
    padding: 2px;
}
QMenuBar::item { padding: 4px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: #ccd0da; }
QMenu {
    background: #eff1f5;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    border-radius: 6px;
    padding: 4px;
}
QMenu::item { padding: 6px 24px; border-radius: 4px; }
QMenu::item:selected { background: #ccd0da; color: #1e66f5; }
QMenu::separator { height: 1px; background: #ccd0da; margin: 4px 8px; }
QToolBar {
    background: #e6e9ef;
    border-bottom: 1px solid #ccd0da;
    spacing: 4px;
    padding: 3px 8px;
}
QToolBar QToolButton {
    background: transparent;
    color: #4c4f69;
    border: none;
    border-radius: 4px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 600;
}
QToolBar QToolButton:hover   { background: #ccd0da; }
QToolBar QToolButton:pressed { background: #bcc0cc; }
QToolBar::separator { width: 1px; background: #ccd0da; margin: 3px 2px; }
QStatusBar {
    background: #e6e9ef;
    color: #6c6f85;
    border-top: 1px solid #ccd0da;
    font-size: 12px;
}
QSplitter::handle:horizontal { background: #ccd0da; width: 1px; }
QSplitter::handle:vertical   { background: #ccd0da; height: 1px; }
QTreeWidget {
    background: #eff1f5;
    alternate-background-color: #e6e9ef;
    color: #4c4f69;
    border: none;
    selection-background-color: transparent;
    selection-color: #1e66f5;
    outline: none;
    font-size: 13px;
}
QTreeWidget::item { padding: 3px 6px; }
QTreeWidget::item:selected { background: #d0d8f0; color: #1e66f5; border-radius: 3px; }
QTreeWidget::item:hover:!selected { background: #dce0e8; }
QTreeWidget::branch { background: #eff1f5; }
QTreeWidget::branch:has-siblings:!adjoins-item { border-image: none; border: none; }
QTreeWidget::branch:has-siblings:adjoins-item  { border-image: none; border: none; }
QTreeWidget::branch:!has-children:!has-siblings:adjoins-item { border-image: none; border: none; }
QHeaderView::section {
    background: #e6e9ef;
    color: #8c8fa1;
    padding: 5px 10px;
    border: none;
    border-right: 1px solid #ccd0da;
    border-bottom: 1px solid #ccd0da;
    font-size: 11px;
    font-weight: 700;
}
QScrollBar:vertical {
    background: #e6e9ef;
    width: 8px;
    margin: 0;
    border: none;
}
QScrollBar::handle:vertical {
    background: #acb0be;
    border-radius: 4px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #8c8fa1; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { height: 0; background: none; }
QScrollBar:horizontal {
    background: #e6e9ef;
    height: 8px;
    margin: 0;
    border: none;
}
QScrollBar::handle:horizontal {
    background: #acb0be;
    border-radius: 4px;
    min-width: 24px;
}
QScrollBar::handle:horizontal:hover { background: #8c8fa1; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { width: 0; background: none; }
QPushButton {
    background: #ccd0da;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    border-radius: 5px;
    padding: 5px 16px;
    font-weight: 600;
    font-size: 12px;
}
QPushButton:hover   { background: #bcc0cc; border-color: #1e66f5; }
QPushButton:pressed { background: #acb0be; }
QPushButton:disabled { color: #acb0be; border-color: #dce0e8; background: #e6e9ef; }
QPushButton#primaryBtn {
    background: #1e66f5;
    color: #ffffff;
    border: none;
    font-weight: 700;
}
QPushButton#primaryBtn:hover   { background: #1a58d8; }
QPushButton#primaryBtn:pressed { background: #1649ba; }
QPushButton#primaryBtn:disabled { background: #acb0be; color: #e6e9ef; }
QComboBox {
    background: #e6e9ef;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    border-radius: 5px;
    padding: 4px 10px;
    font-size: 12px;
    min-width: 100px;
}
QComboBox:hover { border-color: #1e66f5; }
QComboBox:focus { border-color: #1e66f5; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox::down-arrow {
    border-left:  4px solid transparent;
    border-right: 4px solid transparent;
    border-top:   5px solid #6c6f85;
    width: 0; height: 0;
}
QComboBox QAbstractItemView {
    background: #eff1f5;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    selection-background-color: #ccd0da;
    selection-color: #1e66f5;
    outline: none;
}
QLineEdit {
    background: #ffffff;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    border-radius: 5px;
    padding: 5px 10px;
    font-size: 13px;
}
QLineEdit:focus { border-color: #1e66f5; }
QLabel { color: #4c4f69; }
QLabel#sectionLabel { color: #1e66f5; font-weight: 700; font-size: 14px; }
QLabel#subLabel { color: #8c8fa1; font-size: 11px; }
QGroupBox {
    color: #6c6f85;
    border: 1px solid #ccd0da;
    border-radius: 6px;
    margin-top: 14px;
    padding: 8px;
    font-weight: 600;
    font-size: 12px;
    background: #eff1f5;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 8px;
    left: 12px;
    color: #1e66f5;
}
QCheckBox { color: #4c4f69; spacing: 8px; font-size: 13px; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1.5px solid #bcc0cc;
    border-radius: 4px;
    background: #ffffff;
}
QCheckBox::indicator:checked {
    background: #dde5fd;
    border-color: #1e66f5;
    image: url(:/icons/check.svg);
}
QCheckBox::indicator:hover { border-color: #1e66f5; }
QDoubleSpinBox, QSpinBox {
    background: #ffffff;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    border-radius: 5px;
    padding: 4px 8px;
    font-size: 13px;
}
QDoubleSpinBox:focus, QSpinBox:focus { border-color: #1e66f5; }
QDoubleSpinBox::up-button, QSpinBox::up-button,
QDoubleSpinBox::down-button, QSpinBox::down-button {
    border: none; background: transparent; width: 18px;
}
QToolTip {
    background: #eff1f5;
    color: #4c4f69;
    border: 1px solid #bcc0cc;
    border-radius: 4px;
    padding: 4px 8px;
    font-size: 12px;
}
QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
)";
}

} // namespace Theme
