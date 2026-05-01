#pragma once
#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QList>
#include <QByteArray>

struct HexHighlight {
    int     start{-1};   // byte offset, inclusive
    int     end{-1};     // byte offset, exclusive
    QColor  color;
    bool    active{false};  // render with stronger border/color
};

class HexEditor : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit HexEditor(QWidget* parent = nullptr);

    void setData(const QByteArray& data);
    void appendData(const QByteArray& data);
    QByteArray data() const { return data_; }
    void clear();

    void setReadOnly(bool ro);
    bool isReadOnly() const { return readOnly_; }

    // Replace any existing highlight list; -1 active index = none
    void setHighlights(const QList<HexHighlight>& hl);
    void setActiveHighlight(int start, int end);   // single range emphasis
    void clearHighlights();

    void scrollToOffset(int offset);
    int  cursorOffset() const { return cursor_; }

signals:
    void dataChanged(const QByteArray& newData);
    void cursorChanged(int offset);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void focusOutEvent(QFocusEvent*) override;

private:
    // Layout
    static constexpr int BYTES_PER_ROW = 16;
    static constexpr int MARGIN        = 12;   // px
    static constexpr int HEADER_ROWS   = 1;

    QFont   font_;
    int     cw_{0};   // char width
    int     ch_{0};   // char height (ascent + descent)
    int     lh_{0};   // line height (with leading)
    int     headerH_{0};

    // Pixel x of the first char of each hex byte column (0-15)
    int hexX_[BYTES_PER_ROW]{};
    int asciiStartX_{0};
    int totalWidth_{0};

    QByteArray data_;
    bool       readOnly_{false};

    // Cursor
    int  cursor_{0};     // byte index
    int  nibble_{0};     // 0=high, 1=low
    bool inAscii_{false};

    // Highlights
    QList<HexHighlight> highlights_;
    int  activeStart_{-1};
    int  activeEnd_{-1};

    bool hasFocus_{false};

    void updateLayout();
    void updateScrollbar();
    void ensureCursorVisible();

    int  rowCount() const;
    int  rowY(int row) const;       // y pixel of row top
    int  visibleFirstRow() const;
    int  visibleLastRow() const;

    // Map pixel point → byte offset (-1 if outside data area)
    // Returns whether the hit is in the ASCII column
    int  hitTest(const QPoint& pos, bool* inAscii = nullptr) const;

    void paintRow(QPainter& p, int row) const;
    void paintHeader(QPainter& p) const;
    QColor highlightAt(int byteIdx) const; // blended highlight color or invalid
};
