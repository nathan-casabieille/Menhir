#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QTimer>
#include <ASTERIXCodec/Codec.hpp>
#include "HexEditor.hpp"
#include "ByteTracker.hpp"

class QSplitter;

class DecodeView : public QWidget {
    Q_OBJECT
public:
    explicit DecodeView(const asterix::Codec& codec, QWidget* parent = nullptr);

    void loadFile(const QString& path);
    void decodeCurrentBytes();
    void onClearClicked();

signals:
    void statusMessage(const QString& msg);

private slots:
    void onHexDataChanged(const QByteArray& data);
    void onDecodeClicked();
    void onOpenFileClicked();
    void onTreeItemSelected(QTreeWidgetItem* item, int col);
    void onHexCursorChanged(int offset);

private:
    const asterix::Codec& codec_;

    QLabel*      byteCountLabel_;
    HexEditor*   hexEditor_;
    QTreeWidget* tree_;
    QSplitter*   splitter_;
    QTimer*      decodeTimer_;

    // Field inspector panel
    QWidget* detailPanel_;
    QLabel*  detailPlaceholder_;
    QWidget* detailContent_;
    QLabel*  detailFieldName_;
    QLabel*  detailParentLabel_;
    QWidget* bitGrid_;
    QLabel*  detailHex_;
    QLabel*  detailDec_;
    QLabel*  detailBin_;

    struct RecordTrack {
        asterix::DecodedBlock block;
        std::vector<TrackedRecord> tracks;
    };
    RecordTrack lastDecode_;
    bool        hasDecoded_{false};

    void buildDetailPanel();
    void updateDetailPanel(QTreeWidgetItem* item);

    void populateTree(const asterix::DecodedBlock& block,
                      const std::vector<TrackedRecord>& tracks);
    void rebuildHighlights(const std::vector<TrackedRecord>& tracks);
    QString formatFieldValue(const asterix::ElementDef& elem, uint64_t raw) const;

    static constexpr int ROLE_BYTE_START   = Qt::UserRole + 0;
    static constexpr int ROLE_BYTE_END     = Qt::UserRole + 1;
    static constexpr int ROLE_COLOR_IDX    = Qt::UserRole + 2;
    static constexpr int ROLE_RAW_VALUE    = Qt::UserRole + 3;
    static constexpr int ROLE_BIT_WIDTH    = Qt::UserRole + 4;
    static constexpr int ROLE_PARENT_LABEL = Qt::UserRole + 5;
};
