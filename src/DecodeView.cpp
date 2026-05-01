#include "DecodeView.hpp"
#include "Theme.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
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

// ── Constructor ───────────────────────────────────────────────────────────────

DecodeView::DecodeView(const Codec& codec, QWidget* parent)
    : QWidget(parent), codec_(codec)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Thin status bar inside the view (byte count) ──────────────────────────
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

    // ── Vertical splitter: tree (top) | hex (bottom) ─────────────────────────
    splitter_ = new QSplitter(Qt::Vertical);
    splitter_->setHandleWidth(3);
    splitter_->setStyleSheet(
        "QSplitter::handle:vertical {"
        "  background:" + Theme::CS::surface0 + ";"
        "  border-top:1px solid " + Theme::CS::surface1 + ";"
        "  border-bottom:1px solid " + Theme::CS::surface1 + ";"
        "}");

    // ── Top: decoded fields tree ──────────────────────────────────────────────
    tree_ = new QTreeWidget;
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({"Field", "Value"});
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->header()->setDefaultSectionSize(260);
    tree_->setAlternatingRowColors(false);
    tree_->setIndentation(16);
    tree_->setUniformRowHeights(false);
    tree_->setAnimated(true);
    splitter_->addWidget(tree_);

    // ── Bottom: hex editor ────────────────────────────────────────────────────
    hexEditor_ = new HexEditor;
    hexEditor_->setMinimumHeight(140);
    splitter_->addWidget(hexEditor_);

    splitter_->setStretchFactor(0, 5);   // tree: 5 parts
    splitter_->setStretchFactor(1, 3);   // hex:  3 parts
    root->addWidget(splitter_, 1);

    // ── Timer ─────────────────────────────────────────────────────────────────
    decodeTimer_ = new QTimer(this);
    decodeTimer_->setSingleShot(true);
    decodeTimer_->setInterval(400);
    connect(decodeTimer_, &QTimer::timeout, this, &DecodeView::decodeCurrentBytes);

    connect(hexEditor_, &HexEditor::dataChanged,  this, &DecodeView::onHexDataChanged);
    connect(tree_,      &QTreeWidget::itemClicked, this, &DecodeView::onTreeItemSelected);
    connect(hexEditor_, &HexEditor::cursorChanged, this, &DecodeView::onHexCursorChanged);
}

// ── Data change ───────────────────────────────────────────────────────────────

void DecodeView::onHexDataChanged(const QByteArray& data) {
    int n = data.size();
    byteCountLabel_->setText(
        n == 0 ? QString() : QString("%1 bytes").arg(n));
    hexEditor_->clearHighlights();
    tree_->clear();
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
    // setData triggers dataChanged → onHexDataChanged → decodeTimer
    // Decode immediately (don't wait for timer)
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

        // Byte-range tracking
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
    switch (elem.encoding) {
    case Encoding::Table: {
        auto it = elem.table.find(raw);
        QString meaning = it != elem.table.end()
                              ? QString::fromStdString(it->second) : "(unknown)";
        return QString("%1  —  %2").arg(raw).arg(meaning);
    }
    case Encoding::UnsignedQuantity: {
        double v = elem.scale * static_cast<double>(raw);
        QString s = QString::number(v, 'g', 8);
        if (!elem.unit.empty()) s += "  " + QString::fromStdString(elem.unit);
        s += QString("  [raw %1]").arg(raw);
        return s;
    }
    case Encoding::SignedQuantity: {
        double v = elem.scale * signedValue(raw, elem.bits);
        QString s = QString::number(v, 'g', 8);
        if (!elem.unit.empty()) s += "  " + QString::fromStdString(elem.unit);
        s += QString("  [raw %1]").arg(static_cast<int64_t>(signedValue(raw, elem.bits)));
        return s;
    }
    case Encoding::StringOctal:
        return formatOctal4(raw) + "  (Mode-3A)";
    case Encoding::Raw:
        return QString("0x%1  (%2 dec)").arg(raw, 0, 16).toUpper().arg(raw);
    default:
        return QString("0x%1").arg(raw, 0, 16).toUpper();
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
                QString("@%1 – %2").arg(trk->fspec.start).arg(trk->fspec.end - 1),
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
                                       : QString("0x%1").arg(fv, 0, 16).toUpper();
                    auto* f = makeItem(QString::fromStdString(fn), val, Theme::C::text);
                    if (range.valid()) {
                        f->setData(0, ROLE_BYTE_START, range.start);
                        f->setData(0, ROLE_BYTE_END,   range.end);
                        f->setData(0, ROLE_COLOR_IDX,  ci);
                    }
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
                    QString val = edef ? formatFieldValue(*edef, ditem.repetitions[i])
                                       : QString("0x%1").arg(ditem.repetitions[i], 0, 16).toUpper();
                    auto* f = makeItem(QString("[%1]").arg(i), val, Theme::C::text);
                    if (range.valid()) { f->setData(0, ROLE_BYTE_START, range.start); f->setData(0, ROLE_BYTE_END, range.end); f->setData(0, ROLE_COLOR_IDX, ci); }
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
                auto* raw = makeItem("raw", fmtHex(ditem.raw_bytes), Theme::C::overlay1);
                if (range.valid()) { raw->setData(0, ROLE_BYTE_START, range.start); raw->setData(0, ROLE_BYTE_END, range.end); raw->setData(0, ROLE_COLOR_IDX, ci); }
                itemNode->addChild(raw);
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
    hasDecoded_ = false;
    emit statusMessage("Cleared");
}
