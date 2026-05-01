#include "EncodePanel.hpp"
#include "Theme.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <sstream>

using namespace asterix;

// ── Helpers ───────────────────────────────────────────────────────────────────

static double signedValue(uint64_t raw, int bits) {
    if (raw & (1ULL << (bits - 1)))
        return static_cast<double>(static_cast<int64_t>(raw) - (1LL << bits));
    return static_cast<double>(raw);
}

// ── Constructor ───────────────────────────────────────────────────────────────

EncodePanel::EncodePanel(const Codec& codec, QWidget* parent)
    : QWidget(parent), codec_(codec)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet("background:" + Theme::CS::mantle + "; border-bottom:1px solid " + Theme::CS::surface0 + ";");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 0, 12, 0);
    tbLayout->setSpacing(8);

    auto* catLabel = new QLabel("Category:");
    catLabel->setStyleSheet("color:" + Theme::CS::subtext0 + "; font-weight:600; font-size:12px;");
    catCombo_ = new QComboBox;
    catCombo_->setMinimumWidth(200);
    catCombo_->setFixedHeight(26);

    auto* varLabel = new QLabel("Variation:");
    varLabel->setStyleSheet("color:" + Theme::CS::subtext0 + "; font-weight:600; font-size:12px;");
    varCombo_ = new QComboBox;
    varCombo_->setMinimumWidth(120);
    varCombo_->setFixedHeight(26);

    encodeBtn_ = new QPushButton("Encode ▶");
    encodeBtn_->setObjectName("primaryBtn");
    encodeBtn_->setFixedHeight(26);
    encodeBtn_->setMinimumWidth(100);

    tbLayout->addWidget(catLabel);
    tbLayout->addWidget(catCombo_);
    tbLayout->addSpacing(12);
    tbLayout->addWidget(varLabel);
    tbLayout->addWidget(varCombo_);
    tbLayout->addStretch();
    tbLayout->addWidget(encodeBtn_);

    root->addWidget(toolbar);

    // ── Main splitter ─────────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // Left: item form
    formScroll_ = new QScrollArea;
    formScroll_->setWidgetResizable(true);
    formScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    formScroll_->setMinimumWidth(380);

    formWidget_ = new QWidget;
    formWidget_->setStyleSheet("background:" + Theme::CS::base + ";");
    formLayout_ = new QVBoxLayout(formWidget_);
    formLayout_->setContentsMargins(16, 16, 16, 16);
    formLayout_->setSpacing(10);
    formLayout_->addStretch();

    formScroll_->setWidget(formWidget_);
    splitter->addWidget(formScroll_);

    // Right: output
    auto* outputPanel = new QWidget;
    outputPanel->setMinimumWidth(280);
    auto* outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(12, 10, 12, 10);
    outputLayout->setSpacing(8);

    auto* outRow = new QHBoxLayout;
    outputLabel_ = new QLabel("Encoded output");
    outputLabel_->setStyleSheet("color:" + Theme::CS::text + "; font-weight:600; font-size:13px;");
    copyBtn_ = new QPushButton("Copy Hex");
    copyBtn_->setEnabled(false);
    copyBtn_->setFixedHeight(26);
    outRow->addWidget(outputLabel_);
    outRow->addStretch();
    outRow->addWidget(copyBtn_);
    outputLayout->addLayout(outRow);

    outputHex_ = new HexEditor;
    outputHex_->setReadOnly(true);
    outputLayout->addWidget(outputHex_, 1);

    splitter->addWidget(outputPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter, 1);

    connect(catCombo_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EncodePanel::onCategoryChanged);
    connect(varCombo_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EncodePanel::onVariationChanged);
    connect(encodeBtn_, &QPushButton::clicked, this, &EncodePanel::onEncodeClicked);
    connect(copyBtn_,   &QPushButton::clicked, this, &EncodePanel::onCopyHexClicked);

    encodeTimer_ = new QTimer(this);
    encodeTimer_->setSingleShot(true);
    encodeTimer_->setInterval(300);
    connect(encodeTimer_, &QTimer::timeout, this, &EncodePanel::onEncodeClicked);
}

// ── Category list ─────────────────────────────────────────────────────────────

void EncodePanel::setCategoryList(const std::vector<uint8_t>& cats) {
    catCombo_->blockSignals(true);
    catCombo_->clear();
    for (uint8_t c : cats) {
        try {
            const auto& def = codec_.category(c);
            catCombo_->addItem(
                QString("CAT%1  —  %2").arg(c, 3, 10, QChar('0'))
                                       .arg(QString::fromStdString(def.name)),
                static_cast<int>(c));
        } catch (...) {
            catCombo_->addItem(QString("CAT%1").arg(c, 3, 10, QChar('0')),
                               static_cast<int>(c));
        }
    }
    catCombo_->blockSignals(false);
    if (!cats.empty()) onCategoryChanged(0);
}

// ── Category / variation change ───────────────────────────────────────────────

void EncodePanel::onCategoryChanged(int idx) {
    if (idx < 0 || idx >= catCombo_->count()) return;
    currentCat_ = static_cast<uint8_t>(catCombo_->itemData(idx).toInt());

    varCombo_->blockSignals(true);
    varCombo_->clear();
    try {
        const auto& def = codec_.category(currentCat_);
        for (const auto& [name, _] : def.uap_variations)
            varCombo_->addItem(QString::fromStdString(name));
        // Put default first
        int di = varCombo_->findText(QString::fromStdString(def.default_variation));
        if (di > 0) {
            varCombo_->removeItem(di);
            varCombo_->insertItem(0, QString::fromStdString(def.default_variation));
        }
    } catch (...) {}
    varCombo_->blockSignals(false);

    if (varCombo_->count() > 0) {
        currentVariation_ = varCombo_->currentText().toStdString();
        buildForm(currentCat_, currentVariation_);
    }
}

void EncodePanel::onVariationChanged(int idx) {
    if (idx < 0) return;
    currentVariation_ = varCombo_->itemText(idx).toStdString();
    buildForm(currentCat_, currentVariation_);
}

// ── Form building ─────────────────────────────────────────────────────────────

void EncodePanel::clearForm() {
    itemForms_.clear();
    // Remove all widgets except the stretch at end
    while (formLayout_->count() > 1) {
        auto* item = formLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

static QWidget* makeFieldWidget(const ElementDef& elem, QWidget* parent) {
    switch (elem.encoding) {
    case Encoding::Table: {
        auto* cb = new QComboBox(parent);
        for (const auto& [val, meaning] : elem.table)
            cb->addItem(QString("%1  —  %2").arg(val).arg(QString::fromStdString(meaning)),
                        static_cast<qulonglong>(val));
        return cb;
    }
    case Encoding::UnsignedQuantity: {
        auto* sb = new QDoubleSpinBox(parent);
        sb->setDecimals(6);
        sb->setSingleStep(elem.scale);
        if (elem.has_range) {
            sb->setMinimum(elem.min_val);
            sb->setMaximum(elem.max_val);
        } else {
            sb->setMinimum(0);
            sb->setMaximum(elem.scale * ((1ULL << elem.bits) - 1));
        }
        if (!elem.unit.empty()) sb->setSuffix("  " + QString::fromStdString(elem.unit));
        return sb;
    }
    case Encoding::SignedQuantity: {
        auto* sb = new QDoubleSpinBox(parent);
        sb->setDecimals(6);
        sb->setSingleStep(elem.scale);
        double max_v = elem.has_range ? elem.max_val : elem.scale * ((1LL << (elem.bits - 1)) - 1);
        double min_v = elem.has_range ? elem.min_val : elem.scale * -(1LL << (elem.bits - 1));
        sb->setMinimum(min_v);
        sb->setMaximum(max_v);
        if (!elem.unit.empty()) sb->setSuffix("  " + QString::fromStdString(elem.unit));
        return sb;
    }
    case Encoding::StringOctal: {
        auto* le = new QLineEdit(parent);
        le->setPlaceholderText("0000");
        le->setMaxLength(4);
        le->setInputMask("0000");
        le->setToolTip("4-digit octal code (Mode-3A squawk)");
        return le;
    }
    default: {
        auto* le = new QLineEdit(parent);
        int hex_digits = (elem.bits + 3) / 4;
        le->setPlaceholderText("0x" + QString(hex_digits, '0'));
        le->setToolTip(QString("%1-bit raw value (hex or decimal)").arg(elem.bits));
        return le;
    }
    }
}

void EncodePanel::buildForm(uint8_t cat, const std::string& variation) {
    clearForm();
    if (cat == 0) return;

    const CategoryDef* def = nullptr;
    try { def = &codec_.category(cat); } catch (...) { return; }

    auto vit = def->uap_variations.find(variation);
    if (vit == def->uap_variations.end()) return;
    const auto& uap = vit->second;

    // Add header
    auto* hdr = new QLabel(
        QString("Items  —  CAT%1  [%2]")
            .arg(cat, 3, 10, QChar('0'))
            .arg(QString::fromStdString(variation)));
    hdr->setObjectName("sectionLabel");
    hdr->setStyleSheet("color:" + Theme::CS::blue + "; font-weight:700; font-size:14px; padding:4px 0;");
    // Insert before stretch
    formLayout_->insertWidget(formLayout_->count()-1, hdr);

    // Build one group box per UAP item
    for (const auto& item_id : uap) {
        if (item_id == "-" || item_id == "rfs") continue;

        auto iit = def->items.find(item_id);
        if (iit == def->items.end()) continue;
        const DataItemDef& idef = iit->second;

        ItemForm form;
        form.item_id = item_id;
        form.type    = idef.type;

        // Group box label
        QString label = QString("I%1/%2")
            .arg(cat, 3, 10, QChar('0'))
            .arg(QString::fromStdString(item_id));
        if (!idef.name.empty())
            label += "  " + QString::fromStdString(idef.name);

        auto* box = new QGroupBox(formWidget_);
        box->setCheckable(false);
        box->setTitle(label);

        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(8,8,8,8);
        boxLayout->setSpacing(6);

        // Enable checkbox
        auto* enableCheck = new QCheckBox("Include in record");
        enableCheck->setChecked(idef.presence == Presence::Mandatory);
        form.enableCheck = enableCheck;
        form.box = box;
        boxLayout->addWidget(enableCheck);

        // Fields container — created BEFORE fields so lambda can capture its layout
        auto* fields_container = new QWidget(box);
        auto* fc_layout = new QVBoxLayout(fields_container);
        fc_layout->setContentsMargins(0, 4, 0, 0);
        fc_layout->setSpacing(6);

        auto addElementFields = [&](const std::vector<ElementDef>& elems) {
            for (const auto& elem : elems) {
                if (elem.is_spare) continue;

                auto* row = new QHBoxLayout;
                row->setSpacing(8);

                auto* lbl = new QLabel(QString::fromStdString(elem.name));
                lbl->setStyleSheet("color:" + Theme::CS::subtext0 + "; font-size:12px; min-width:80px;");
                lbl->setFixedWidth(100);
                row->addWidget(lbl);

                auto* w = makeFieldWidget(elem, fields_container);
                row->addWidget(w, 1);

                auto* bits_lbl = new QLabel(QString("%1b").arg(elem.bits));
                bits_lbl->setStyleSheet("color:" + Theme::CS::overlay1 + "; font-size:11px;");
                bits_lbl->setFixedWidth(32);
                row->addWidget(bits_lbl);

                FieldWidget fw;
                fw.field_name = elem.name;
                fw.encoding   = elem.encoding;
                fw.bits       = elem.bits;
                fw.scale      = elem.scale;
                fw.unit       = elem.unit;
                fw.table      = elem.table;
                fw.widget     = w;
                form.fields.push_back(fw);

                // Live encode on any field change
                if (auto* sb = qobject_cast<QDoubleSpinBox*>(w))
                    connect(sb, &QDoubleSpinBox::valueChanged, this, &EncodePanel::scheduleEncode);
                else if (auto* cb = qobject_cast<QComboBox*>(w))
                    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EncodePanel::scheduleEncode);
                else if (auto* le = qobject_cast<QLineEdit*>(w))
                    connect(le, &QLineEdit::textChanged, this, &EncodePanel::scheduleEncode);

                fc_layout->addLayout(row);
            }
        };

        auto addRawHexField = [&](const QString& placeholder, const QString& tip) {
            auto* note = new QLabel(tip);
            note->setStyleSheet("color:" + Theme::CS::overlay0 + "; font-size:12px;");
            auto* le = new QLineEdit(fields_container);
            le->setPlaceholderText(placeholder);
            le->setFont(QFont("Menlo", 12));
            connect(le, &QLineEdit::textChanged, this, &EncodePanel::scheduleEncode);
            FieldWidget fw;
            fw.field_name = "__raw__";
            fw.encoding   = Encoding::Raw;
            fw.widget     = le;
            form.fields.push_back(fw);
            fc_layout->addWidget(note);
            fc_layout->addWidget(le);
        };

        switch (idef.type) {
        case ItemType::Fixed:
            addElementFields(idef.elements);
            break;
        case ItemType::Extended:
            addElementFields(idef.elements);
            for (const auto& oct : idef.octets)
                addElementFields(oct.elements);
            break;
        case ItemType::Repetitive:
        case ItemType::RepetitiveGroup:
        case ItemType::RepetitiveGroupFX:
        case ItemType::Compound:
            addRawHexField("AB CD EF 01 ...", "Raw hex bytes (space-separated):");
            break;
        case ItemType::SP:
        case ItemType::Explicit:
            addRawHexField("AB CD EF ...", "Raw payload bytes (hex, without length prefix):");
            break;
        }

        fields_container->setVisible(enableCheck->isChecked());
        boxLayout->addWidget(fields_container);

        QObject::connect(enableCheck, &QCheckBox::toggled,
                         [fields_container](bool on){ fields_container->setVisible(on); });
        connect(enableCheck, &QCheckBox::toggled, this, &EncodePanel::scheduleEncode);

        itemForms_.push_back(std::move(form));
        formLayout_->insertWidget(formLayout_->count()-1, box);
    }
}

// ── Encode ────────────────────────────────────────────────────────────────────

uint64_t EncodePanel::fieldValue(const FieldWidget& fw) const {
    if (!fw.widget) return 0;

    switch (fw.encoding) {
    case Encoding::Table: {
        auto* cb = qobject_cast<QComboBox*>(fw.widget);
        if (cb) return static_cast<uint64_t>(cb->currentData().toULongLong());
        return 0;
    }
    case Encoding::UnsignedQuantity: {
        auto* sb = qobject_cast<QDoubleSpinBox*>(fw.widget);
        if (!sb) return 0;
        double v = sb->value();
        uint64_t raw = static_cast<uint64_t>(std::round(v / fw.scale));
        uint64_t mask = fw.bits < 64 ? ((1ULL << fw.bits) - 1) : ~0ULL;
        return raw & mask;
    }
    case Encoding::SignedQuantity: {
        auto* sb = qobject_cast<QDoubleSpinBox*>(fw.widget);
        if (!sb) return 0;
        double v = sb->value();
        int64_t signed_raw = static_cast<int64_t>(std::round(v / fw.scale));
        uint64_t mask = fw.bits < 64 ? ((1ULL << fw.bits) - 1) : ~0ULL;
        return static_cast<uint64_t>(signed_raw) & mask;
    }
    case Encoding::StringOctal: {
        auto* le = qobject_cast<QLineEdit*>(fw.widget);
        if (!le) return 0;
        QString s = le->text().trimmed();
        uint64_t val = 0;
        for (QChar c : s) {
            int d = c.digitValue();
            if (d >= 0 && d <= 7) val = (val << 3) | d;
        }
        return val;
    }
    default: {
        auto* le = qobject_cast<QLineEdit*>(fw.widget);
        if (!le) return 0;
        QString s = le->text().trimmed();
        bool ok = false;
        uint64_t v = s.startsWith("0x") || s.startsWith("0X")
                         ? s.mid(2).toULongLong(&ok, 16)
                         : s.toULongLong(&ok);
        return ok ? v : 0;
    }
    }
}

DecodedItem EncodePanel::buildItem(const ItemForm& form, const DataItemDef& def) const {
    DecodedItem item;
    item.item_id = form.item_id;
    item.type    = form.type;

    if (form.type == ItemType::Fixed || form.type == ItemType::Extended) {
        for (const auto& fw : form.fields) {
            if (fw.field_name != "__raw__")
                item.fields[fw.field_name] = fieldValue(fw);
        }
    } else {
        // Raw hex input
        for (const auto& fw : form.fields) {
            if (fw.field_name == "__raw__") {
                auto* le = qobject_cast<QLineEdit*>(fw.widget);
                if (!le) break;
                QString hex = le->text().simplified().remove(' ');
                for (int i = 0; i + 1 < hex.size(); i += 2) {
                    bool ok = false;
                    uint8_t b = static_cast<uint8_t>(hex.mid(i,2).toUInt(&ok, 16));
                    if (ok) item.raw_bytes.push_back(b);
                }
            }
        }
    }
    return item;
}

void EncodePanel::scheduleEncode() {
    if (encodeTimer_) encodeTimer_->start();
}

void EncodePanel::onEncodeClicked() {
    if (currentCat_ == 0) return;

    DecodedRecord rec;
    rec.uap_variation = currentVariation_;

    const CategoryDef* cat_def = nullptr;
    try { cat_def = &codec_.category(currentCat_); } catch (...) {
        QMessageBox::warning(this, "Encode", "Category not registered.");
        return;
    }

    for (const auto& form : itemForms_) {
        if (!form.enableCheck || !form.enableCheck->isChecked()) continue;
        auto iit = cat_def->items.find(form.item_id);
        if (iit == cat_def->items.end()) continue;
        rec.items[form.item_id] = buildItem(form, iit->second);
    }

    try {
        auto bytes = codec_.encode(currentCat_, {rec});
        outputHex_->setData(QByteArray(reinterpret_cast<const char*>(bytes.data()),
                                       static_cast<int>(bytes.size())));
        copyBtn_->setEnabled(true);
        outputLabel_->setText(QString("Encoded output  —  %1 bytes").arg(bytes.size()));
        emit statusMessage(QString("Encoded %1 bytes").arg(bytes.size()));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Encode Error", ex.what());
        emit statusMessage(QString("Encode error: %1").arg(ex.what()));
    }
}

void EncodePanel::onCopyHexClicked() {
    QByteArray data = outputHex_->data();
    QString hex;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) hex += ' ';
        hex += QString("%1").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    QApplication::clipboard()->setText(hex);
    emit statusMessage("Hex copied to clipboard");
}
