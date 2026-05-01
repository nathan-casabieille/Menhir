#include "MainWindow.hpp"
#include "DecodeView.hpp"
#include "EncodePanel.hpp"
#include "Theme.hpp"

#include <ASTERIXCodec/SpecLoader.hpp>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

// ── Spec loading ──────────────────────────────────────────────────────────────

static asterix::CategoryDef loadSpecFromResource(const QString& rcPath) {
    QFile f(rcPath);
    if (!f.open(QIODevice::ReadOnly))
        throw asterix::SpecLoadError("Cannot open resource: " + rcPath.toStdString());
    QByteArray data = f.readAll();
    QTemporaryFile tmp(QDir::tempPath() + "/asterix_spec_XXXXXX.xml");
    tmp.setAutoRemove(true);
    if (!tmp.open())
        throw asterix::SpecLoadError("Cannot create temp file for spec");
    tmp.write(data);
    tmp.flush();
    QString tmpPath = tmp.fileName();
    tmp.close();
    return asterix::loadSpec(tmpPath.toStdString());
}

// ── Constructor ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Menhir");
    setMinimumSize(1100, 720);
    resize(1440, 900);
    setAcceptDrops(true);

    loadSpecs();

    decodeView_  = new DecodeView(codec_);
    encodePanel_ = new EncodePanel(codec_);
    encodePanel_->setCategoryList(registeredCats_);

    stack_ = new QStackedWidget;
    stack_->addWidget(decodeView_);
    stack_->addWidget(encodePanel_);
    setCentralWidget(stack_);

    buildToolBar();

    // Status bar
    statusBar()->showMessage("Ready");

    // Menu bar
    auto* file = menuBar()->addMenu("File");
    auto* openM = new QAction("Open ASTERIX File…", this);
    openM->setShortcut(QKeySequence::Open);
    connect(openM, &QAction::triggered, openAct_, &QAction::trigger);
    file->addAction(openM);
    file->addSeparator();
    auto* quit = new QAction("Quit", this);
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    file->addAction(quit);

    auto* help = menuBar()->addMenu("Help");
    auto* about = new QAction("About", this);
    connect(about, &QAction::triggered, [this]{
        QMessageBox::about(this, "Menhir",
            "<b>Menhir 1.0</b><br>"
            "ASTERIX protocol encode/decode tool.<br>"
            "Supports CAT001, CAT002, CAT034, CAT048, CAT062.");
    });
    help->addAction(about);

    connect(decodeView_,  &DecodeView::statusMessage,  this, &MainWindow::onStatusMessage);
    connect(encodePanel_, &EncodePanel::statusMessage, this, &MainWindow::onStatusMessage);
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

void MainWindow::buildToolBar() {
    auto* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setFloatable(false);
    tb->setIconSize({16, 16});
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // File actions
    openAct_ = new QAction("Open File…", this);
    openAct_->setShortcut(QKeySequence::Open);
    connect(openAct_, &QAction::triggered, [this]{
        QString path = QFileDialog::getOpenFileName(this, "Open ASTERIX File",
            {}, "Binary files (*.bin *.ast *.asterix *.raw);;All files (*)");
        if (!path.isEmpty()) {
            setMode(0);
            decodeView_->loadFile(path);
        }
    });
    tb->addAction(openAct_);
    tb->addSeparator();

    // Decode actions — forwarded to DecodeView
    decodeAct_ = new QAction("Decode ▶", this);
    decodeAct_->setShortcut(Qt::Key_F5);
    connect(decodeAct_, &QAction::triggered, decodeView_, &DecodeView::decodeCurrentBytes);
    tb->addAction(decodeAct_);

    clearAct_ = new QAction("Clear", this);
    connect(clearAct_, &QAction::triggered, [this]{
        if (stack_->currentIndex() == 0)
            decodeView_->onClearClicked();
    });
    tb->addAction(clearAct_);
    tb->addSeparator();

    // Spacer
    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    // Mode selector — right-aligned segmented buttons
    const QString baseSeg =
        "QToolButton {"
        "  background:" + Theme::CS::surface0 + "; color:" + Theme::CS::subtext0 + ";"
        "  border:1px solid " + Theme::CS::surface1 + ";"
        "  padding:3px 18px; font-size:12px; font-weight:600;"
        "  border-radius:0;"
        "}"
        "QToolButton:hover { color:" + Theme::CS::text + "; background:" + Theme::CS::crust + "; }"
        "QToolButton:checked { background:" + Theme::CS::mantle + "; color:" + Theme::CS::blue + "; border-bottom:2px solid " + Theme::CS::blue + "; }";

    modeDecodeBtn_ = new QToolButton;
    modeDecodeBtn_->setText("Decode");
    modeDecodeBtn_->setCheckable(true);
    modeDecodeBtn_->setChecked(true);
    modeDecodeBtn_->setAutoExclusive(true);
    modeDecodeBtn_->setStyleSheet(baseSeg +
        "QToolButton { border-right:none; border-radius:4px 0 0 4px; }");

    modeEncodeBtn_ = new QToolButton;
    modeEncodeBtn_->setText("Encode");
    modeEncodeBtn_->setCheckable(true);
    modeEncodeBtn_->setAutoExclusive(true);
    modeEncodeBtn_->setStyleSheet(baseSeg +
        "QToolButton { border-radius:0 4px 4px 0; }");

    connect(modeDecodeBtn_, &QToolButton::clicked, this, [this]{ setMode(0); });
    connect(modeEncodeBtn_, &QToolButton::clicked, this, [this]{ setMode(1); });

    tb->addWidget(modeDecodeBtn_);
    tb->addWidget(modeEncodeBtn_);

    // Show/hide decode actions when switching mode
    connect(stack_, &QStackedWidget::currentChanged, [this](int i){
        decodeAct_->setVisible(i == 0);
        clearAct_->setVisible(i == 0);
    });
}

void MainWindow::setMode(int idx) {
    stack_->setCurrentIndex(idx);
    modeDecodeBtn_->setChecked(idx == 0);
    modeEncodeBtn_->setChecked(idx == 1);
}

// ── Specs ─────────────────────────────────────────────────────────────────────

void MainWindow::loadSpecs() {
    static const char* specs[] = {
        ":/specs/CAT01.xml", ":/specs/CAT02.xml", ":/specs/CAT34.xml",
        ":/specs/CAT48.xml", ":/specs/CAT62.xml",
    };
    for (const char* s : specs) {
        try {
            auto def = loadSpecFromResource(s);
            uint8_t c = def.cat;
            codec_.registerCategory(std::move(def));
            registeredCats_.push_back(c);
        } catch (const std::exception& ex) {
            qWarning("Failed to load %s: %s", s, ex.what());
        }
    }
    std::sort(registeredCats_.begin(), registeredCats_.end());
}

// ── Drag & drop ───────────────────────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    for (const QUrl& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) {
            setMode(0);
            decodeView_->loadFile(url.toLocalFile());
            break;
        }
    }
}

void MainWindow::onStatusMessage(const QString& msg) {
    statusBar()->showMessage(msg, 8000);
}
