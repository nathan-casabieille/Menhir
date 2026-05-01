#include "HexEditor.hpp"
#include "Theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QClipboard>
#include <QApplication>
#include <algorithm>
#include <cstring>

// ── Helpers ───────────────────────────────────────────────────────────────────

static char toHexHigh(uint8_t b) {
    static const char H[] = "0123456789ABCDEF";
    return H[(b >> 4) & 0xF];
}
static char toHexLow(uint8_t b) {
    static const char H[] = "0123456789ABCDEF";
    return H[b & 0xF];
}
static int fromHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// ── Constructor ───────────────────────────────────────────────────────────────

HexEditor::HexEditor(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    font_ = QFont("Menlo", 12);
    font_.setStyleHint(QFont::Monospace);
    font_.setFixedPitch(true);

    setFont(font_);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QPalette p = viewport()->palette();
    p.setColor(QPalette::Base, Theme::C::mantle);
    viewport()->setPalette(p);
    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setAutoFillBackground(true);

    updateLayout();
}

// ── Public API ────────────────────────────────────────────────────────────────

void HexEditor::setData(const QByteArray& data) {
    data_ = data;
    cursor_ = 0; nibble_ = 0; inAscii_ = false;
    updateScrollbar();
    viewport()->update();
}

void HexEditor::appendData(const QByteArray& data) {
    data_.append(data);
    updateScrollbar();
    viewport()->update();
}

void HexEditor::clear() {
    data_.clear();
    cursor_ = 0; nibble_ = 0;
    clearHighlights();
    updateScrollbar();
    viewport()->update();
}

void HexEditor::setReadOnly(bool ro) {
    readOnly_ = ro;
    viewport()->update();
}

void HexEditor::setHighlights(const QList<HexHighlight>& hl) {
    highlights_ = hl;
    activeStart_ = -1; activeEnd_ = -1;
    viewport()->update();
}

void HexEditor::setActiveHighlight(int start, int end) {
    activeStart_ = start;
    activeEnd_   = end;
    scrollToOffset(start);
    viewport()->update();
}

void HexEditor::clearHighlights() {
    highlights_.clear();
    activeStart_ = -1; activeEnd_ = -1;
    viewport()->update();
}

void HexEditor::scrollToOffset(int offset) {
    if (data_.isEmpty() || lh_ == 0) return;
    int row = std::max(0, offset) / BYTES_PER_ROW;
    int firstVis = visibleFirstRow();
    int lastVis  = visibleLastRow();
    if (row < firstVis || row > lastVis)
        verticalScrollBar()->setValue(row * lh_);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void HexEditor::updateLayout() {
    QFontMetrics fm(font_);
    cw_ = fm.horizontalAdvance('F');
    ch_ = fm.height();
    lh_ = ch_ + 4;
    headerH_ = lh_ + 6;

    // Address column: "00000000  " = 8 chars + 2 space = 10 chars
    int x = MARGIN + cw_ * 10;

    for (int i = 0; i < BYTES_PER_ROW; ++i) {
        hexX_[i] = x;
        x += cw_ * 2;  // two hex chars "AB"
        if (i == BYTES_PER_ROW - 1)
            x += 0;        // no trailing space on last
        else if (i == 7)
            x += cw_ * 2;  // "  " extra gap at midpoint
        else
            x += cw_;      // space separator
    }

    // ASCII column: 2 spaces + pipe char, then 16 chars
    asciiStartX_ = x + cw_ * 3;  // "  |" = 3 chars gap
    totalWidth_  = asciiStartX_ + cw_ * BYTES_PER_ROW + MARGIN;

    updateScrollbar();
}

void HexEditor::updateScrollbar() {
    if (lh_ == 0) return;
    int rows = rowCount();
    int visH = viewport()->height() - headerH_;
    int contentH = rows * lh_;
    verticalScrollBar()->setRange(0, std::max(0, contentH - visH));
    verticalScrollBar()->setPageStep(visH);
    verticalScrollBar()->setSingleStep(lh_);
}

// ── Geometry ──────────────────────────────────────────────────────────────────

int HexEditor::rowCount() const {
    // Reserve one extra slot when cursor is past the end (ready to append)
    int sz = std::max(static_cast<int>(data_.size()), cursor_ + 1);
    sz = std::max(1, sz);
    return (sz + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
}

int HexEditor::rowY(int row) const {
    int scroll = verticalScrollBar()->value();
    return headerH_ + row * lh_ - scroll;
}

int HexEditor::visibleFirstRow() const {
    int scroll = verticalScrollBar()->value();
    return scroll / lh_;
}

int HexEditor::visibleLastRow() const {
    int scroll = verticalScrollBar()->value();
    int visH = viewport()->height() - headerH_;
    return std::min(rowCount() - 1, (scroll + visH) / lh_ + 1);
}

// ── Hit testing ───────────────────────────────────────────────────────────────

int HexEditor::hitTest(const QPoint& pos, bool* inAscii) const {
    if (inAscii) *inAscii = false;
    if (lh_ == 0) return -1;

    int y = pos.y() - headerH_ + verticalScrollBar()->value();
    if (y < 0) return -1;
    int row = y / lh_;
    if (row >= rowCount()) return -1;

    int x = pos.x();

    // Check ASCII area
    if (x >= asciiStartX_) {
        int col = (x - asciiStartX_) / cw_;
        if (col < 0 || col >= BYTES_PER_ROW) return -1;
        int offset = row * BYTES_PER_ROW + col;
        if (offset >= data_.size()) return -1;
        if (inAscii) *inAscii = true;
        return offset;
    }

    // Check hex area
    for (int i = 0; i < BYTES_PER_ROW; ++i) {
        int hx = hexX_[i];
        if (x >= hx && x < hx + cw_ * 2 + cw_) {  // includes trailing space
            int offset = row * BYTES_PER_ROW + i;
            if (offset >= data_.size()) return -1;
            return offset;
        }
    }
    return -1;
}

// ── Highlight helpers ─────────────────────────────────────────────────────────

QColor HexEditor::highlightAt(int byteIdx) const {
    if (activeStart_ >= 0 && byteIdx >= activeStart_ && byteIdx < activeEnd_) {
        QColor c = Theme::C::blue; c.setAlpha(100); return c;
    }

    for (const auto& hl : highlights_) {
        if (byteIdx >= hl.start && byteIdx < hl.end)
            return hl.color;
    }
    return {};
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void HexEditor::paintHeader(QPainter& p) const {
    // Background
    p.fillRect(0, 0, viewport()->width(), headerH_, Theme::C::crust);

    QFont f = font_;
    f.setBold(true);
    p.setFont(f);
    p.setPen(Theme::C::overlay0);

    int y = headerH_ - 6;  // baseline

    // "Offset" label
    p.drawText(MARGIN, y, "Offset");

    // Byte column labels
    for (int i = 0; i < BYTES_PER_ROW; ++i) {
        QString label = QString("%1").arg(i, 2, 16, QChar('0')).toUpper();
        p.drawText(hexX_[i], y, label);
    }

    // ASCII header
    p.drawText(asciiStartX_, y, "Text");

    // Separator line
    p.setPen(QPen(Theme::C::surface0, 1));
    p.drawLine(0, headerH_ - 1, viewport()->width(), headerH_ - 1);
}

void HexEditor::paintRow(QPainter& p, int row) const {
    int y      = rowY(row);
    int ascent = QFontMetrics(font_).ascent();
    int textY  = y + ascent + 2;

    if (textY - ascent > viewport()->height()) return;  // off screen
    if (textY < 0) return;

    int rowStart = row * BYTES_PER_ROW;

    // Row background (alternate)
    QColor rowBg = (row % 2 == 0) ? Theme::C::mantle : Theme::C::base;
    p.fillRect(0, y, viewport()->width(), lh_, rowBg);

    // Address
    p.setFont(font_);
    p.setPen(Theme::C::sky);
    p.drawText(MARGIN, textY,
               QString("%1").arg(rowStart, 8, 16, QChar('0')).toUpper());

    // Separator line between address and hex
    p.setPen(QPen(Theme::C::surface0, 1));
    p.drawLine(MARGIN + cw_ * 9, y, MARGIN + cw_ * 9, y + lh_);

    // Hex bytes
    for (int i = 0; i < BYTES_PER_ROW; ++i) {
        int offset = rowStart + i;
        if (offset >= data_.size()) {
            // Draw empty cursor slot when cursor is at this append position
            if (offset == cursor_ && hasFocus_ && !inAscii_) {
                QColor c70 = Theme::C::blue; c70.setAlpha(70);
                QColor c90 = Theme::C::blue; c90.setAlpha(90);
                p.fillRect(hexX_[i] - 1, y + 1, cw_ * 2 + 2, lh_ - 2, c70);
                p.fillRect(hexX_[i], y + lh_ - 2, cw_, 2, Theme::C::blue);
                p.setPen(c90);
                p.drawText(hexX_[i], textY, "__");
            }
            break;
        }

        uint8_t b = static_cast<uint8_t>(data_[offset]);

        // Highlight background
        QColor hlColor = highlightAt(offset);
        if (hlColor.isValid()) {
            int bg_x = hexX_[i] - 1;
            int bg_w = cw_ * 2 + 2;
            p.fillRect(bg_x, y, bg_w, lh_, hlColor);
        }

        // Active highlight → bright text
        bool isCursor = (offset == cursor_) && hasFocus_;
        bool isActive = (activeStart_ >= 0 && offset >= activeStart_ && offset < activeEnd_);

        if (isCursor && !inAscii_) {
            // Block cursor background
            QColor c160 = Theme::C::blue; c160.setAlpha(160);
            p.fillRect(hexX_[i] - 1, y + 1, cw_ * 2 + 2, lh_ - 2, c160);
            // Nibble indicator underline on active nibble
            int nibX = hexX_[i] + nibble_ * cw_;
            p.fillRect(nibX, y + lh_ - 2, cw_, 2, Theme::C::base);
        }

        p.setPen((isCursor && !inAscii_) ? Theme::C::base
                                         : (isActive ? Theme::C::base : Theme::C::text));
        QString hb = QString("%1%2").arg(toHexHigh(b)).arg(toHexLow(b));
        p.drawText(hexX_[i], textY, hb);

        // Mid separator
        if (i == 7) {
            p.setPen(QPen(Theme::C::surface0, 1));
            p.drawLine(hexX_[8] - cw_, y, hexX_[8] - cw_, y + lh_);
        }
    }

    // ASCII pipe separator
    p.setPen(QPen(Theme::C::surface1, 1));
    p.drawLine(asciiStartX_ - cw_, y, asciiStartX_ - cw_, y + lh_);

    // ASCII bytes
    for (int i = 0; i < BYTES_PER_ROW; ++i) {
        int offset = rowStart + i;
        if (offset >= data_.size()) break;

        uint8_t b = static_cast<uint8_t>(data_[offset]);

        // Highlight background in ASCII area
        QColor hlColor = highlightAt(offset);
        if (hlColor.isValid())
            p.fillRect(asciiStartX_ + i * cw_, y, cw_, lh_, hlColor);

        bool isCursor = (offset == cursor_) && hasFocus_ && inAscii_;
        if (isCursor) {
            QColor c160 = Theme::C::blue; c160.setAlpha(160);
            p.fillRect(asciiStartX_ + i * cw_, y + 1, cw_, lh_ - 2, c160);
        }

        bool isActive = (activeStart_ >= 0 && offset >= activeStart_ && offset < activeEnd_);
        if (b >= 0x20 && b < 0x7F) {
            p.setPen((isCursor || isActive) ? Theme::C::base : Theme::C::subtext0);
            p.drawText(asciiStartX_ + i * cw_, textY, QString(static_cast<char>(b)));
        } else {
            p.setPen(Theme::C::surface2);
            p.drawText(asciiStartX_ + i * cw_, textY, "·");
        }
    }
}

void HexEditor::paintEvent(QPaintEvent*) {
    QPainter p(viewport());
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setFont(font_);

    // Fill entire background
    p.fillRect(viewport()->rect(), Theme::C::mantle);

    if (data_.isEmpty()) {
        paintHeader(p);
        // Still paint an empty first row so the cursor slot is visible
        paintRow(p, 0);
        p.setPen(Theme::C::overlay0);
        QFont f = font_;
        f.setItalic(true);
        p.setFont(f);
        QRect hint(0, headerH_ + lh_ + 8, viewport()->width(), 32);
        p.drawText(hint, Qt::AlignHCenter | Qt::AlignTop,
                   "Click here and type hex digits, or open a file");
        return;
    }

    int first = visibleFirstRow();
    int last  = visibleLastRow();
    for (int row = first; row <= last; ++row)
        paintRow(p, row);

    paintHeader(p);  // drawn last so it covers any scrolled-up row
}

// ── Events ────────────────────────────────────────────────────────────────────

void HexEditor::keyPressEvent(QKeyEvent* e) {
    // Navigation — only meaningful when data exists
    if (!data_.isEmpty()) {
        auto moveCursor = [&](int delta) {
            cursor_ = std::clamp(cursor_ + delta, 0, (int)data_.size() - 1);
            nibble_ = 0;
            ensureCursorVisible();
            emit cursorChanged(cursor_);
            viewport()->update();
        };

        switch (e->key()) {
        case Qt::Key_Left:  moveCursor(-1); return;
        case Qt::Key_Right: moveCursor(+1); return;
        case Qt::Key_Up:    moveCursor(-BYTES_PER_ROW); return;
        case Qt::Key_Down:  moveCursor(+BYTES_PER_ROW); return;
        case Qt::Key_Home:
            cursor_ = (cursor_ / BYTES_PER_ROW) * BYTES_PER_ROW;
            nibble_ = 0; viewport()->update(); return;
        case Qt::Key_End:
            cursor_ = std::min((int)data_.size()-1,
                               (cursor_ / BYTES_PER_ROW + 1) * BYTES_PER_ROW - 1);
            nibble_ = 0; viewport()->update(); return;
        case Qt::Key_PageUp:
            moveCursor(-(viewport()->height() / lh_) * BYTES_PER_ROW); return;
        case Qt::Key_PageDown:
            moveCursor(+(viewport()->height() / lh_) * BYTES_PER_ROW); return;
        default: break;
        }
    }

    if (readOnly_) { QAbstractScrollArea::keyPressEvent(e); return; }

    // Paste (Ctrl+V)
    if (e->matches(QKeySequence::Paste)) {
        QString text = QApplication::clipboard()->text().simplified().remove(' ');
        QByteArray bytes;
        for (int i = 0; i + 1 < text.size(); i += 2) {
            int hi = fromHex(text[i].toLatin1());
            int lo = fromHex(text[i+1].toLatin1());
            if (hi >= 0 && lo >= 0)
                bytes.append(static_cast<char>((hi << 4) | lo));
        }
        if (!bytes.isEmpty()) {
            data_.insert(cursor_, bytes);
            cursor_ += bytes.size();
            nibble_ = 0;
            updateScrollbar();
            emit dataChanged(data_);
            viewport()->update();
        }
        return;
    }

    // Backspace — delete byte before cursor (or undo nibble in progress)
    if (e->key() == Qt::Key_Backspace) {
        if (nibble_ == 1) {
            // Undo the high nibble just typed: clear it and stay on this byte
            data_[cursor_] = static_cast<char>(data_[cursor_] & 0x0F);
            nibble_ = 0;
        } else if (cursor_ > 0) {
            data_.remove(cursor_ - 1, 1);
            cursor_--;
            nibble_ = 0;
        } else if (!data_.isEmpty()) {
            data_.remove(0, 1);
            cursor_ = 0;
            nibble_ = 0;
        }
        updateScrollbar();
        ensureCursorVisible();
        emit dataChanged(data_);
        viewport()->update();
        return;
    }

    // Delete — delete byte at cursor
    if (e->key() == Qt::Key_Delete && !data_.isEmpty() && cursor_ < (int)data_.size()) {
        data_.remove(cursor_, 1);
        cursor_ = std::min(cursor_, (int)data_.size() - 1);
        cursor_ = std::max(cursor_, 0);
        nibble_ = 0;
        updateScrollbar();
        ensureCursorVisible();
        emit dataChanged(data_);
        viewport()->update();
        return;
    }

    if (inAscii_ && !data_.isEmpty()) {
        if (!e->text().isEmpty()) {
            char c = e->text().at(0).toLatin1();
            if (c >= 0x20 && c < 0x7F) {
                data_[cursor_] = c;
                cursor_ = std::min(cursor_ + 1, (int)data_.size() - 1);
                emit dataChanged(data_);
                viewport()->update();
            }
        }
        return;
    }

    // Hex editing: two-nibble per byte, appends when past end
    int hex = -1;
    if (!e->text().isEmpty())
        hex = fromHex(e->text().at(0).toLatin1());

    if (hex >= 0) {
        // If cursor is at/past end, insert a new zero byte
        if (cursor_ >= (int)data_.size()) {
            data_.append('\0');
            cursor_ = (int)data_.size() - 1;
            nibble_ = 0;
        }
        uint8_t b = static_cast<uint8_t>(data_[cursor_]);
        if (nibble_ == 0) {
            data_[cursor_] = static_cast<char>((hex << 4) | (b & 0x0F));
            nibble_ = 1;
        } else {
            data_[cursor_] = static_cast<char>((b & 0xF0) | hex);
            nibble_ = 0;
            cursor_++;  // may now equal data_.size() → ready for next append
        }
        updateScrollbar();
        ensureCursorVisible();
        emit dataChanged(data_);
        viewport()->update();
        return;
    }

    QAbstractScrollArea::keyPressEvent(e);
}

void HexEditor::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    setFocus();
    bool inAscii = false;
    int offset = hitTest(e->pos(), &inAscii);
    if (offset >= 0) {
        cursor_  = offset;
        nibble_  = 0;
        inAscii_ = inAscii;
        emit cursorChanged(cursor_);
        viewport()->update();
    }
    // Click anywhere when empty → place cursor at 0 so typing works
    if (data_.isEmpty()) {
        cursor_ = 0; nibble_ = 0; inAscii_ = false;
        viewport()->update();
    }
}

void HexEditor::wheelEvent(QWheelEvent* e) {
    int delta = -e->angleDelta().y() / 120 * lh_ * 3;
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta);
}

void HexEditor::resizeEvent(QResizeEvent*) {
    updateLayout();
}

void HexEditor::focusInEvent(QFocusEvent*) {
    hasFocus_ = true;
    viewport()->update();
}

void HexEditor::focusOutEvent(QFocusEvent*) {
    hasFocus_ = false;
    viewport()->update();
}

// ── Scroll helpers ────────────────────────────────────────────────────────────

void HexEditor::ensureCursorVisible() {
    if (lh_ == 0) return;
    int row = cursor_ / BYTES_PER_ROW;
    int y   = rowY(row);
    if (y < headerH_)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + y - headerH_);
    else if (y + lh_ > viewport()->height())
        verticalScrollBar()->setValue(verticalScrollBar()->value() + y + lh_ - viewport()->height());
}
