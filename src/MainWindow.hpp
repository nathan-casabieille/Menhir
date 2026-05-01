#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QToolButton>
#include <ASTERIXCodec/Codec.hpp>

class DecodeView;
class EncodePanel;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private slots:
    void onStatusMessage(const QString& msg);
    void setMode(int idx);

private:
    void buildToolBar();
    void loadSpecs();

    asterix::Codec           codec_;
    std::vector<uint8_t>     registeredCats_;

    QStackedWidget* stack_;
    DecodeView*     decodeView_;
    EncodePanel*    encodePanel_;

    // toolbar actions forwarded to decode view
    QAction* openAct_;
    QAction* decodeAct_;
    QAction* clearAct_;

    QToolButton* modeDecodeBtn_;
    QToolButton* modeEncodeBtn_;
};
