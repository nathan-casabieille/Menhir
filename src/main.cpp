#include "MainWindow.hpp"
#include "Theme.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QStyle>

int main(int argc, char* argv[]) {
    // High-DPI scaling
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Menhir");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Oratis");
    app.setOrganizationDomain("oratis.fr");

    app.setWindowIcon(QIcon(":/icons/menhir.png"));

    // Fusion style + Catppuccin Latte light palette
    app.setStyle("Fusion");
    app.setPalette(Theme::makePalette());
    app.setStyleSheet(Theme::styleSheet());

    // Prefer system monospace font for the hex editor
    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono.ttf");  // no-op if missing

    MainWindow w;
    w.show();

    return app.exec();
}
