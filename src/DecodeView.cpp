#include "DecodeView.hpp"
#include "Theme.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <cmath>

using namespace asterix;

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString fmtHex(const std::vector<uint8_t>& bytes) {
    QString s;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) s += ' ';
        s += QString("%1").arg(bytes[i], 2, 16, QChar('0')).toUpper();
    }
    return s;
}

static double signedValue(uint64_t raw, int bits) {
    if (raw & (1ULL << (bits - 1)))
        return static_cast<double>(static_cast<int64_t>(raw) - (1LL << bits));
    return static_cast<double>(raw);
}

static QString formatOctal4(uint64_t raw) {
    return QString("%1%2%3%4")
        .arg((raw >> 9) & 7).arg((raw >> 6) & 7)
        .arg((raw >> 3) & 7).arg(raw & 7);
}

// ── Bit grid widget ───────────────────────────────────────────────────────────

class BitGridWidget : public QWidget {
    uint64_t val_  = 0;
    int      bits_ = 0;
    bool     valid_= false;

    static constexpr int CW   = 18;  // cell width
    static constexpr int CH   = 20;  // cell height
    static constexpr int GAP  = 3;   // gap between cells
    static constexpr int BGAP = 7;   // extra gap between byte groups
    static constexpr int LBLH = 14;  // bit-position label height
    static constexpr int VGAP = 10;  // vertical gap between rows

public:
    explicit BitGridWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(0);
    }

    void setField(uint64_t val, int bits) {
        val_ = val; bits_ = bits; valid_ = true;
        int rows = (bits_ + 15) / 16;
        setFixedHeight(rows * (CH + LBLH + VGAP) - VGAP + 4);
        update();
    }

    void clear() { valid_ = false; setFixedHeight(0); update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        if (!valid_ || bits_ <= 0) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int perRow = qMin(bits_, 16);
        const int nRows  = (bits_ + perRow - 1) / perRow;

        for (int row = 0; row < nRows; ++row) {
            const int rowHighBit = bits_ - 1 - row * perRow;
            const int bitsInRow  = qMin(perRow, rowHighBit + 1);

            // Center the row horizontally
            const int extraGaps = qMax(0, (bitsInRow - 1) / 8);
            const int rowW = bitsInRow * (CW + GAP) - GAP + extraGaps * BGAP;
            const int startX = (width() - rowW) / 2;
            const int y = row * (CH + LBLH + VGAP);

            for (int i = 0; i < bitsInRow; ++i) {
                const int bitPos = rowHighBit - i;
                const bool bitVal = (val_ >> bitPos) & 1ULL;
                const int x = startX + i * (CW + GAP) + (i / 8) * BGAP;

                QColor bg = bitVal ? Theme::C::blue : Theme::C::surface0;
                QColor fg = bitVal ? Theme::C::base : Theme::C::subtext1;

                QRect cell(x, y, CW, CH);
                p.setPen(Qt::NoPen);
                p.setBrush(bg);
                p.drawRoundedRect(cell, 4, 4);

                QFont vf = font(); vf.setPixelSize(11); vf.setBold(true);
                p.setFont(vf); p.setPen(fg);
                p.drawText(cell, Qt::AlignCenter, bitVal ? "1" : "0");

                QFont lf = font(); lf.setPixelSize(9);
                p.setFont(lf); p.setPen(Theme::C::overlay0);
                p.drawText(QRect(x, y + CH + 2, CW, LBLH - 2),
                           Qt::AlignCenter, QString::number(bitPos));
            }
        }
    }
};

// ── Field item delegate (two-line display for leaf field rows) ─────────────

class FieldItemDelegate : public QStyledItemDelegate {
    static constexpr int ROW_H = 38;
public:
    explicit FieldItemDelegate(QObject* p = nullptr) : QStyledItemDelegate(p) {}

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        if (idx.data(Qt::UserRole + 4).isValid())
            return {QStyledItemDelegate::sizeHint(opt, idx).width(), ROW_H};
        return QStyledItemDelegate::sizeHint(opt, idx);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override {
        if (!idx.data(Qt::UserRole + 4).isValid()) {
            QStyledItemDelegate::paint(p, opt, idx);
            return;
        }

        p->save();
        p->setClipRect(opt.rect);

        const bool sel   = opt.state & QStyle::State_Selected;
        const bool hover = opt.state & QStyle::State_MouseOver;
        QColor bg = sel   ? QColor("#d0d8f0")
                  : hover ? Theme::C::crust
                          : (idx.row() % 2 == 0 ? Theme::C::base : Theme::C::mantle);
        p->fillRect(opt.rect, bg);

        QColor mainC = sel ? Theme::C::blue : Theme::C::text;
        QColor subC  = sel ? Theme::C::blue : Theme::C::overlay1;
        if (sel) subC.setAlpha(180);

        const QRect r   = opt.rect.adjusted(8, 3, -4, -3);
        const int   mid = r.top() + r.height() / 2 - 1;
        const QRect topR(r.left(), r.top(),  r.width(), mid - r.top());
        const QRect botR(r.left(), mid + 2,  r.width(), r.bottom() - mid - 2);

        QFont mainF = opt.font; mainF.setPixelSize(12);
        QFont subF  = opt.font; subF.setPixelSize(10);

        if (idx.column() == 0) {
            p->setFont(mainF); p->setPen(mainC);
            p->drawText(topR, Qt::AlignLeft | Qt::AlignVCenter,
                        idx.data(Qt::DisplayRole).toString());
            const int bits = idx.data(Qt::UserRole + 4).toInt();
            p->setFont(subF); p->setPen(subC);
            p->drawText(botR, Qt::AlignLeft | Qt::AlignVCenter,
                        QString("%1 bit%2").arg(bits).arg(bits > 1 ? "s" : ""));
        } else {
            const QString full = idx.data(Qt::DisplayRole).toString();
            QString primary = full, secondary;
            const int sep = full.indexOf("  ·  ");
            if (sep >= 0) { primary = full.left(sep).trimmed(); secondary = full.mid(sep + 5).trimmed(); }

            p->setFont(mainF); p->setPen(mainC);
            p->drawText(topR, Qt::AlignLeft | Qt::AlignVCenter, primary);
            if (!secondary.isEmpty()) {
                p->setFont(subF); p->setPen(subC);
                p->drawText(botR, Qt::AlignLeft | Qt::AlignVCenter, secondary);
            }
        }
        p->restore();
    }
};

// ── Constructor ───────────────────────────────────────────────────────────────

DecodeView::DecodeView(const Codec& codec, QWidget* parent)
    : QWidget(parent), codec_(codec)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Thin status strip (byte count + hint) ─────────────────────────────────
    auto* statusStrip = new QWidget;
    statusStrip->setFixedHeight(24);
    statusStrip->setStyleSheet(
        "background:" + Theme::CS::crust + "; border-bottom:1px solid " + Theme::CS::surface0 + ";");
    auto* ssLayout = new QHBoxLayout(statusStrip);
    ssLayout->setContentsMargins(12, 0, 12, 0);
    ssLayout->setSpacing(16);

    byteCountLabel_ = new QLabel;
    byteCountLabel_->setStyleSheet("color:" + Theme::CS::subtext0 + "; font-size:11px;");

    auto* hintLabel = new QLabel(
        "Click hex area and type digits  ·  Ctrl+O to open file  ·  F5 to decode");
    hintLabel->setStyleSheet("color:" + Theme::CS::overlay1 + "; font-size:11px;");

    ssLayout->addWidget(byteCountLabel_);
    ssLayout->addStretch();
    ssLayout->addWidget(hintLabel);
    root->addWidget(statusStrip);

    // ── Main vertical splitter: top pane | hex editor ─────────────────────────
    splitter_ = new QSplitter(Qt::Vertical);
    splitter_->setHandleWidth(3);
    splitter_->setStyleSheet(
        "QSplitter::handle:vertical {"
        "  background:" + Theme::CS::surface0 + ";"
        "  border-top:1px solid "    + Theme::CS::surface1 + ";"
        "  border-bottom:1px solid " + Theme::CS::surface1 + ";"
        "}");

    // ── Top pane: horizontal splitter (tree | inspector) ─────────────────────
    auto* topSplit = new QSplitter(Qt::Horizontal);
    topSplit->setHandleWidth(1);
    topSplit->setStyleSheet(
        "QSplitter::handle:horizontal { background:" + Theme::CS::surface0 + "; }");

    tree_ = new QTreeWidget;
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({"Field", "Value"});
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->header()->setDefaultSectionSize(220);
    tree_->setAlternatingRowColors(false);
    tree_->setIndentation(16);
    tree_->setUniformRowHeights(false);
    tree_->setAnimated(true);
    tree_->setItemDelegate(new FieldItemDelegate(tree_));
    topSplit->addWidget(tree_);

    buildDetailPanel();
    topSplit->addWidget(detailPanel_);
    topSplit->setStretchFactor(0, 3);
    topSplit->setStretchFactor(1, 2);

    splitter_->addWidget(topSplit);

    // ── Bottom: hex editor ────────────────────────────────────────────────────
    hexEditor_ = new HexEditor;
    hexEditor_->setMinimumHeight(140);
    splitter_->addWidget(hexEditor_);

    splitter_->setStretchFactor(0, 5);
    splitter_->setStretchFactor(1, 3);
    root->addWidget(splitter_, 1);

    // ── Timer ─────────────────────────────────────────────────────────────────
    decodeTimer_ = new QTimer(this);
    decodeTimer_->setSingleShot(true);
    decodeTimer_->setInterval(400);
    connect(decodeTimer_, &QTimer::timeout, this, &DecodeView::decodeCurrentBytes);

    connect(hexEditor_, &HexEditor::dataChanged,  this, &DecodeView::onHexDataChanged);
    connect(tree_,      &QTreeWidget::itemClicked, this, &DecodeView::onTreeItemSelected);
    connect(tree_,      &QTreeWidget::currentItemChanged,
            [this](QTreeWidgetItem* cur, QTreeWidgetItem*) {
                updateDetailPanel(cur);
            });
    connect(hexEditor_, &HexEditor::cursorChanged, this, &DecodeView::onHexCursorChanged);
}

// ── Detail panel ──────────────────────────────────────────────────────────────

void DecodeView::buildDetailPanel() {
    detailPanel_ = new QWidget;
    detailPanel_->setMinimumWidth(180);
    detailPanel_->setStyleSheet("background:" + Theme::CS::base + ";");

    auto* root = new QVBoxLayout(detailPanel_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header strip
    auto* header = new QWidget;
    header->setFixedHeight(24);
    header->setStyleSheet(
        "background:" + Theme::CS::crust + ";"
        "border-bottom:1px solid " + Theme::CS::surface0 + ";");
    auto* hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(12, 0, 12, 0);
    auto* hTitle = new QLabel("Field Inspector");
    hTitle->setStyleSheet("color:" + Theme::CS::subtext0 + "; font-size:11px; font-weight:600;");
    hLay->addWidget(hTitle);
    root->addWidget(header);

    // Scroll area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget;
    auto* cLay = new QVBoxLayout(content);
    cLay->setContentsMargins(16, 16, 16, 16);
    cLay->setSpacing(0);

    // Placeholder
    detailPlaceholder_ = new QLabel("Select a field\nto inspect its value");
    detailPlaceholder_->setAlignment(Qt::AlignCenter);
    detailPlaceholder_->setStyleSheet(
        "color:" + Theme::CS::overlay0 + "; font-size:12px;");
    cLay->addWidget(detailPlaceholder_);

    // Populated content (hidden until a field is selected)
    detailContent_ = new QWidget;
    auto* dLay = new QVBoxLayout(detailContent_);
    dLay->setContentsMargins(0, 0, 0, 0);
    dLay->setSpacing(0);

    detailFieldName_ = new QLabel;
    detailFieldName_->setStyleSheet(
        "color:" + Theme::CS::text + "; font-size:20px; font-weight:700;");
    detailFieldName_->setWordWrap(true);

    detailParentLabel_ = new QLabel;
    detailParentLabel_->setStyleSheet(
        "color:" + Theme::CS::subtext0 + "; font-size:11px;");
    detailParentLabel_->setWordWrap(true);

    dLay->addWidget(detailFieldName_);
    dLay->addSpacing(2);
    dLay->addWidget(detailParentLabel_);
    dLay->addSpacing(20);

    // Section: BINARY
    auto* binSec = new QLabel("BINARY");
    binSec->setStyleSheet(
        "color:" + Theme::CS::overlay0 + "; font-size:10px; font-weight:700; letter-spacing:1px;");
    dLay->addWidget(binSec);
    dLay->addSpacing(10);

    bitGrid_ = new BitGridWidget(detailContent_);
    dLay->addWidget(bitGrid_);
    dLay->addSpacing(20);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:" + Theme::CS::surface0 + ";");
    dLay->addWidget(sep);
    dLay->addSpacing(16);

    // Section: VALUES
    auto* valSec = new QLabel("VALUES");
    valSec->setStyleSheet(
        "color:" + Theme::CS::overlay0 + "; font-size:10px; font-weight:700; letter-spacing:1px;");
    dLay->addWidget(valSec);
    dLay->addSpacing(10);

    auto makeRow = [&](const QString& key) -> QLabel* {
        auto* rowW = new QWidget;
        auto* rowL = new QHBoxLayout(rowW);
        rowL->setContentsMargins(0, 0, 0, 0);
        rowL->setSpacing(10);
        auto* keyLbl = new QLabel(key);
        keyLbl->setStyleSheet(
            "color:" + Theme::CS::overlay1 + "; font-size:11px; font-weight:600;");
        keyLbl->setFixedWidth(28);
        auto* valLbl = new QLabel;
        valLbl->setStyleSheet(
            "color:" + Theme::CS::text + "; font-size:12px; font-family:monospace;");
        valLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valLbl->setWordWrap(true);
        rowL->addWidget(keyLbl);
        rowL->addWidget(valLbl, 1);
        dLay->addWidget(rowW);
        dLay->addSpacing(4);
        return valLbl;
    };

    detailHex_ = makeRow("Hex");
    detailDec_ = makeRow("Dec");
    detailBin_ = makeRow("Bin");

    dLay->addStretch();
    detailContent_->setVisible(false);

    cLay->addWidget(detailContent_);
    cLay->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

void DecodeView::updateDetailPanel(QTreeWidgetItem* item) {
    if (!item) {
        detailPlaceholder_->setVisible(true);
        detailContent_->setVisible(false);
        return;
    }

    QVariant rawVar  = item->data(0, ROLE_RAW_VALUE);
    QVariant bitsVar = item->data(0, ROLE_BIT_WIDTH);

    if (!rawVar.isValid() || !bitsVar.isValid()) {
        detailPlaceholder_->setVisible(true);
        detailContent_->setVisible(false);
        return;
    }

    const uint64_t raw  = rawVar.toULongLong();
    const int      bits = bitsVar.toInt();
    const QString  name = item->text(0);
    const QString  parent = item->data(0, ROLE_PARENT_LABEL).toString();

    detailFieldName_->setText(name);
    detailParentLabel_->setText(parent);

    static_cast<BitGridWidget*>(bitGrid_)->setField(raw, bits);

    const int hexDigits = (bits + 3) / 4;
    detailHex_->setText(QString("0x%1").arg(raw, hexDigits, 16, QChar('0')).toUpper());
    detailDec_->setText(QString::number(raw));

    // Binary string grouped by 4 bits
    QString binStr;
    for (int i = bits - 1; i >= 0; --i) {
        if (i < bits - 1 && (bits - 1 - i) % 4 == 0) binStr += ' ';
        binStr += ((raw >> i) & 1ULL) ? '1' : '0';
    }
    detailBin_->setText(binStr);

    detailPlaceholder_->setVisible(false);
    detailContent_->setVisible(true);
}

// ── Data change ───────────────────────────────────────────────────────────────

void DecodeView::onHexDataChanged(const QByteArray& data) {
    int n = data.size();
    byteCountLabel_->setText(
        n == 0 ? QString() : QString("%1 bytes").arg(n));
    hexEditor_->clearHighlights();
    tree_->clear();
    updateDetailPanel(nullptr);
    hasDecoded_ = false;
    if (n > 0) decodeTimer_->start();
}

// ── File loading ──────────────────────────────────────────────────────────────

void DecodeView::loadFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit statusMessage("Cannot open: " + path);
        return;
    }
    QByteArray bytes = f.readAll();
    hexEditor_->setData(bytes);
    decodeTimer_->stop();
    decodeCurrentBytes();
    emit statusMessage(QString("Loaded %1  (%2 bytes)")
                           .arg(QFileInfo(path).fileName()).arg(bytes.size()));
}

void DecodeView::onOpenFileClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Open ASTERIX File",
        {}, "Binary files (*.bin *.ast *.asterix *.raw);;All files (*)");
    if (!path.isEmpty()) loadFile(path);
}

// ── Decode ────────────────────────────────────────────────────────────────────

void DecodeView::onDecodeClicked() { decodeCurrentBytes(); }

void DecodeView::decodeCurrentBytes() {
    QByteArray bytes = hexEditor_->data();
    if (bytes.isEmpty()) return;

    try {
        auto block = codec_.decode(
            std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(bytes.constData()),
                static_cast<size_t>(bytes.size())));

        if (!block.valid) {
            emit statusMessage("Decode error: " + QString::fromStdString(block.error));
            return;
        }

        int record_offset = 3;
        std::vector<TrackedRecord> tracks;
        const CategoryDef* cat_def = nullptr;
        try { cat_def = &codec_.category(block.cat); } catch (...) {}

        for (size_t ri = 0; ri < block.records.size() && cat_def; ++ri) {
            int available = static_cast<int>(bytes.size()) - record_offset;
            if (available <= 0) break;
            std::span<const uint8_t> rec_span{
                reinterpret_cast<const uint8_t*>(bytes.constData()) + record_offset,
                static_cast<size_t>(available)};
            TrackedRecord tr = trackRecord(rec_span, *cat_def,
                                           block.records[ri].uap_variation, record_offset);
            tracks.push_back(tr);
            int max_end = record_offset;
            for (const auto& [id, range] : tr.items) max_end = std::max(max_end, range.end);
            max_end = std::max(max_end, tr.fspec.end);
            record_offset = max_end;
        }

        lastDecode_ = {block, tracks};
        hasDecoded_ = true;

        populateTree(block, tracks);
        rebuildHighlights(tracks);

        int n = static_cast<int>(block.records.size());
        QString msg = QString("CAT%1 · %2 record%3 · %4 bytes")
                          .arg(block.cat, 3, 10, QChar('0'))
                          .arg(n).arg(n != 1 ? "s" : "")
                          .arg(block.length);
        emit statusMessage("✓  " + msg);

    } catch (const std::exception& ex) {
        emit statusMessage(QString("Error: %1").arg(ex.what()));
    }
}

// ── Field formatting ──────────────────────────────────────────────────────────

QString DecodeView::formatFieldValue(const ElementDef& elem, uint64_t raw) const {
    const int hexDig = (elem.bits + 3) / 4;
    switch (elem.encoding) {
    case Encoding::Table: {
        auto it = elem.table.find(raw);
        QString meaning = it != elem.table.end()
                              ? QString::fromStdString(it->second) : "(unknown)";
        return meaning + QString("  ·  %1").arg(raw);
    }
    case Encoding::UnsignedQuantity: {
        double v = elem.scale * static_cast<double>(raw);
        QString s = QString::number(v, 'g', 8);
        if (!elem.unit.empty()) s += " " + QString::fromStdString(elem.unit);
        s += QString("  ·  0x%1").arg(raw, hexDig, 16, QChar('0')).toUpper();
        return s;
    }
    case Encoding::SignedQuantity: {
        double v = elem.scale * signedValue(raw, elem.bits);
        QString s = QString::number(v, 'g', 8);
        if (!elem.unit.empty()) s += " " + QString::fromStdString(elem.unit);
        uint64_t mask = elem.bits < 64 ? ((1ULL << elem.bits) - 1) : ~0ULL;
        s += QString("  ·  0x%1").arg(raw & mask, hexDig, 16, QChar('0')).toUpper();
        return s;
    }
    case Encoding::StringOctal:
        return formatOctal4(raw) + "  ·  Mode-3A";
    case Encoding::Raw:
        return QString("0x%1  ·  %2")
            .arg(raw, hexDig, 16, QChar('0')).toUpper().arg(raw);
    default:
        return QString("0x%1").arg(raw, hexDig, 16, QChar('0')).toUpper();
    }
}

// ── Tree population ───────────────────────────────────────────────────────────

static QTreeWidgetItem* makeItem(const QString& field, const QString& value,
                                  const QColor& fg = {}) {
    auto* it = new QTreeWidgetItem({field, value});
    if (fg.isValid()) { it->setForeground(0, fg); it->setForeground(1, fg); }
    return it;
}

void DecodeView::populateTree(const DecodedBlock& block,
                               const std::vector<TrackedRecord>& tracks) {
    tree_->clear();
    updateDetailPanel(nullptr);

    const CategoryDef* cat_def = nullptr;
    try { cat_def = &codec_.category(block.cat); } catch (...) {}

    QFont bold = tree_->font(); bold.setBold(true);

    auto* blockItem = new QTreeWidgetItem({
        QString("CAT%1").arg(block.cat, 3, 10, QChar('0')),
        QString("%1 bytes · %2 record%3")
            .arg(block.length).arg(block.records.size())
            .arg(block.records.size() != 1 ? "s" : "")
    });
    blockItem->setFont(0, bold);
    blockItem->setForeground(0, Theme::C::blue);
    blockItem->setForeground(1, Theme::C::subtext0);
    tree_->addTopLevelItem(blockItem);

    for (size_t ri = 0; ri < block.records.size(); ++ri) {
        const DecodedRecord& rec = block.records[ri];
        const TrackedRecord* trk = (ri < tracks.size()) ? &tracks[ri] : nullptr;

        QString varStr = rec.uap_variation.empty() ? QString()
            : QString("  [%1]").arg(QString::fromStdString(rec.uap_variation));

        auto* recItem = new QTreeWidgetItem({
            QString("Record %1%2").arg(ri + 1).arg(varStr),
            QString("%1 items").arg(rec.items.size())
        });
        recItem->setForeground(0, Theme::C::mauve);
        recItem->setFont(0, bold);
        blockItem->addChild(recItem);

        // FSPEC
        if (trk && trk->fspec.valid()) {
            auto* fi = makeItem("FSPEC",
                QString("@%1 – %2  (%3 B)")
                    .arg(trk->fspec.start).arg(trk->fspec.end - 1).arg(trk->fspec.length()),
                Theme::C::overlay1);
            fi->setData(0, ROLE_BYTE_START, trk->fspec.start);
            fi->setData(0, ROLE_BYTE_END,   trk->fspec.end);
            fi->setData(0, ROLE_COLOR_IDX, -1);
            recItem->addChild(fi);
        }

        std::vector<std::string> order;
        if (trk && !trk->item_order.empty()) order = trk->item_order;
        else for (const auto& [id, _] : rec.items) order.push_back(id);

        int colorIdx = 0;
        for (const std::string& item_id : order) {
            auto iit = rec.items.find(item_id);
            if (iit == rec.items.end()) { ++colorIdx; continue; }
            const DecodedItem& ditem = iit->second;

            ByteRange range{-1,-1};
            if (trk) { auto rit = trk->items.find(item_id); if (rit != trk->items.end()) range = rit->second; }

            QString itemLabel = QString::fromStdString(item_id);
            if (cat_def) {
                auto df = cat_def->items.find(item_id);
                if (df != cat_def->items.end() && !df->second.name.empty())
                    itemLabel += "  " + QString::fromStdString(df->second.name);
            }
            QString rangeStr = range.valid()
                ? QString("@%1 – %2  (%3 B)").arg(range.start).arg(range.end-1).arg(range.length())
                : QString();

            const DataItemDef* def = nullptr;
            if (cat_def) { auto df = cat_def->items.find(item_id); if (df != cat_def->items.end()) def = &df->second; }

            int ci = colorIdx % Theme::ITEM_COLOR_COUNT;
            QColor itemColor = Theme::ITEM_COLORS_STRONG[ci];
            itemColor.setAlpha(255);

            auto* itemNode = new QTreeWidgetItem({
                QString("I%1/%2").arg(block.cat, 3, 10, QChar('0')).arg(QString::fromStdString(item_id)),
                rangeStr
            });
            itemNode->setFont(0, bold);
            itemNode->setForeground(0, itemColor);
            if (def) itemNode->setToolTip(0, QString::fromStdString(def->name));
            if (range.valid()) {
                itemNode->setData(0, ROLE_BYTE_START, range.start);
                itemNode->setData(0, ROLE_BYTE_END,   range.end);
                itemNode->setData(0, ROLE_COLOR_IDX,  ci);
            }
            recItem->addChild(itemNode);

            // Parent label string for the inspector
            const QString parentLabel =
                QString("I%1/%2").arg(block.cat, 3, 10, QChar('0'))
                                  .arg(QString::fromStdString(item_id))
                + (def && !def->name.empty()
                       ? "  ·  " + QString::fromStdString(def->name)
                       : QString());

            auto addFields = [&](QTreeWidgetItem* parent, const DataItemDef* idef,
                                  const std::map<std::string, uint64_t>& fields) {
                std::vector<const ElementDef*> elems;
                if (idef) {
                    for (const auto& e : idef->elements)   elems.push_back(&e);
                    for (const auto& oct : idef->octets)
                        for (const auto& e : oct.elements) elems.push_back(&e);
                }
                for (const auto& [fn, fv] : fields) {
                    const ElementDef* edef = nullptr;
                    for (const auto* e : elems) if (e->name == fn) { edef = e; break; }
                    QString val = edef ? formatFieldValue(*edef, fv)
                                       : QString("0x%1  ·  %2").arg(fv, 0, 16).toUpper().arg(fv);
                    auto* f = makeItem(QString::fromStdString(fn), val, Theme::C::text);
                    if (range.valid()) {
                        f->setData(0, ROLE_BYTE_START, range.start);
                        f->setData(0, ROLE_BYTE_END,   range.end);
                        f->setData(0, ROLE_COLOR_IDX,  ci);
                    }
                    f->setData(0, ROLE_RAW_VALUE,    QVariant::fromValue<quint64>(fv));
                    if (edef) f->setData(0, ROLE_BIT_WIDTH, edef->bits);
                    f->setData(0, ROLE_PARENT_LABEL, parentLabel);
                    parent->addChild(f);
                }
            };

            switch (ditem.type) {
            case ItemType::Fixed:
            case ItemType::Extended:
                addFields(itemNode, def, ditem.fields);
                break;
            case ItemType::Repetitive: {
                const ElementDef* edef = def ? &def->rep_element : nullptr;
                for (size_t i = 0; i < ditem.repetitions.size(); ++i) {
                    uint64_t rv = ditem.repetitions[i];
                    QString val = edef ? formatFieldValue(*edef, rv)
                                       : QString("0x%1").arg(rv, 0, 16).toUpper();
                    auto* f = makeItem(QString("[%1]").arg(i), val, Theme::C::text);
                    if (range.valid()) { f->setData(0, ROLE_BYTE_START, range.start); f->setData(0, ROLE_BYTE_END, range.end); f->setData(0, ROLE_COLOR_IDX, ci); }
                    f->setData(0, ROLE_RAW_VALUE, QVariant::fromValue<quint64>(rv));
                    if (edef) f->setData(0, ROLE_BIT_WIDTH, edef->bits);
                    f->setData(0, ROLE_PARENT_LABEL, parentLabel);
                    itemNode->addChild(f);
                }
                break;
            }
            case ItemType::RepetitiveGroup:
            case ItemType::RepetitiveGroupFX: {
                for (size_t i = 0; i < ditem.group_repetitions.size(); ++i) {
                    auto* grp = new QTreeWidgetItem({QString("Group [%1]").arg(i), ""});
                    grp->setForeground(0, Theme::C::teal);
                    if (range.valid()) { grp->setData(0, ROLE_BYTE_START, range.start); grp->setData(0, ROLE_BYTE_END, range.end); grp->setData(0, ROLE_COLOR_IDX, ci); }
                    addFields(grp, def, ditem.group_repetitions[i]);
                    itemNode->addChild(grp);
                }
                break;
            }
            case ItemType::Compound: {
                for (const auto& [sname, sfields] : ditem.compound_sub_fields) {
                    auto* sub = new QTreeWidgetItem({QString::fromStdString(sname), ""});
                    sub->setForeground(0, Theme::C::teal);
                    if (range.valid()) { sub->setData(0, ROLE_BYTE_START, range.start); sub->setData(0, ROLE_BYTE_END, range.end); sub->setData(0, ROLE_COLOR_IDX, ci); }
                    const CompoundSubItemDef* sidef = nullptr;
                    if (def) for (const auto& si : def->compound_sub_items) if (si.name == sname) { sidef = &si; break; }
                    std::map<std::string, uint64_t> sf_copy = sfields;
                    DataItemDef fake_def;
                    if (sidef) fake_def.elements = sidef->elements;
                    addFields(sub, sidef ? &fake_def : nullptr, sf_copy);
                    itemNode->addChild(sub);
                }
                break;
            }
            case ItemType::SP:
            case ItemType::Explicit: {
                auto* raw_item = makeItem("raw", fmtHex(ditem.raw_bytes), Theme::C::overlay1);
                if (range.valid()) { raw_item->setData(0, ROLE_BYTE_START, range.start); raw_item->setData(0, ROLE_BYTE_END, range.end); raw_item->setData(0, ROLE_COLOR_IDX, ci); }
                itemNode->addChild(raw_item);
                break;
            }
            }

            itemNode->setExpanded(true);
            ++colorIdx;
        }
        recItem->setExpanded(true);
    }
    blockItem->setExpanded(true);
    tree_->resizeColumnToContents(0);
}

// ── Highlights ────────────────────────────────────────────────────────────────

void DecodeView::rebuildHighlights(const std::vector<TrackedRecord>& tracks) {
    QList<HexHighlight> hl;
    int ci = 0;
    for (const auto& tr : tracks) {
        for (const auto& id : tr.item_order) {
            auto it = tr.items.find(id);
            if (it != tr.items.end() && it->second.valid())
                hl.append({it->second.start, it->second.end,
                            Theme::ITEM_COLORS[ci % Theme::ITEM_COLOR_COUNT], false});
            ++ci;
        }
    }
    hexEditor_->setHighlights(hl);
}

// ── Interactions ──────────────────────────────────────────────────────────────

void DecodeView::onTreeItemSelected(QTreeWidgetItem* item, int) {
    if (!item) return;
    QVariant vs = item->data(0, ROLE_BYTE_START);
    QVariant ve = item->data(0, ROLE_BYTE_END);
    if (vs.isValid() && ve.isValid())
        hexEditor_->setActiveHighlight(vs.toInt(), ve.toInt());
}

void DecodeView::onHexCursorChanged(int offset) {
    if (!hasDecoded_) return;
    for (const auto& tr : lastDecode_.tracks) {
        for (const auto& [id, range] : tr.items) {
            if (!range.valid() || offset < range.start || offset >= range.end) continue;
            for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
                auto* block = tree_->topLevelItem(i);
                for (int j = 0; j < block->childCount(); ++j) {
                    auto* rec = block->child(j);
                    for (int k = 0; k < rec->childCount(); ++k) {
                        auto* itm = rec->child(k);
                        if (itm->text(0).contains(QString::fromStdString(id))) {
                            tree_->setCurrentItem(itm);
                            return;
                        }
                    }
                }
            }
        }
    }
}

void DecodeView::onClearClicked() {
    hexEditor_->clear();
    tree_->clear();
    byteCountLabel_->clear();
    updateDetailPanel(nullptr);
    hasDecoded_ = false;
    emit statusMessage("Cleared");
}
