#include "qt/viewers/viewer.h"

#include <algorithm>
#include <cctype>

#include <QAbstractSlider>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTextBlock>
#include <QWheelEvent>

#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/game-area.h"
#include "qt/main-window.h"

namespace viewers {

QString Hex(uint32_t v, int digits) {
    return QStringLiteral("%1").arg(v, digits, 16, QChar('0')).toUpper();
}

QLineEdit* NewHexEdit(QWidget* parent, int max_digits) {
    QLineEdit* edit = new QLineEdit(parent);
    edit->setMaxLength(max_digits);
    edit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9A-Fa-f]{0,%1}").arg(max_digits)), edit));
    return edit;
}

static QFont MonoFont() {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

// ---------------------------------------------------------------------------
// Viewer

Viewer::Viewer(QWidget* parent, const QString& name)
    : dialogs::BaseDialog(parent, name), dname(name) {
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint |
                   Qt::WindowMaximizeButtonHint);
    setSizeGripEnabled(true);
    if (vbamApp().self_test()) {
        setAttribute(Qt::WA_DontShowOnScreen, true);
    }
    if (vbamApp().frame) {
        vbamApp().frame->popups.push_back(this);
    }
}

Viewer::~Viewer() {
    if (vbamApp().frame) {
        vbamApp().frame->popups.remove(this);
    }
}

void Viewer::reject() {
    close();
}

void Viewer::closeEvent(QCloseEvent* event) {
    // stop tracking dialog
    if (vbamApp().frame) {
        vbamApp().frame->popups.remove(this);
    }
    dialogs::BaseDialog::closeEvent(event);
    if (event->isAccepted()) {
        deleteLater();
    }
}

QCheckBox* Viewer::NewAutoUpdateCheckBox(const QString& label) {
    QCheckBox* cb = new QCheckBox(label.isEmpty() ? tr("Automatic &update") : label, this);
    cb->setChecked(auto_update);
    connect(cb, &QCheckBox::toggled, this, [this](bool checked) { auto_update = checked; });
    return cb;
}

QPushButton* Viewer::NewCloseButton() {
    QPushButton* btn = new QPushButton(tr("&Close"), this);
    connect(btn, &QPushButton::clicked, this, &Viewer::close);
    return btn;
}

QPushButton* Viewer::NewRefreshButton(const QString& label) {
    QPushButton* btn = new QPushButton(label.isEmpty() ? tr("&Refresh") : label, this);
    connect(btn, &QPushButton::clicked, this, [this] { Update(); });
    return btn;
}

void Viewer::BindRadio(QRadioButton* radio, int* var, int value) {
    radio->setChecked(*var == value);
    connect(radio, &QRadioButton::toggled, this, [this, var, value](bool checked) {
        if (checked) {
            *var = value;
            Update();
        }
    });
}

void Viewer::BindSpin(QSpinBox* spin, int* var) {
    spin->setValue(*var);
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this, var](int v) {
        *var = v;
        Update();
    });
}

void Viewer::BindSlider(QSlider* slider, int* var) {
    slider->setValue(*var);
    connect(slider, &QSlider::valueChanged, this, [this, var](int v) {
        *var = v;
        Update();
    });
}

void Viewer::Fit() {
    adjustSize();
    setMinimumSize(sizeHint());
}

// ---------------------------------------------------------------------------
// DisList

DisList::DisList(QWidget* parent) : QWidget(parent) {
    tc = new QPlainTextEdit(this);
    tc->setReadOnly(true);
    tc->setLineWrapMode(QPlainTextEdit::NoWrap);
    tc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tc->setTextInteractionFlags(Qt::NoTextInteraction);
    tc->setFocusPolicy(Qt::NoFocus);
    tc->setFont(MonoFont());
    tc->setFrameShape(QFrame::NoFrame);
    // Swallow mouse input on the text area (it is display only).
    tc->viewport()->installEventFilter(this);
    tc->installEventFilter(this);

    sb = new QScrollBar(Qt::Vertical, this);
    sb->setRange(0, 500 - 15);
    sb->setPageStep(15);
    sb->setSingleStep(1);
    sb->setTracking(false);
    connect(sb, &QScrollBar::actionTriggered, this, &DisList::OnScrollAction);
    connect(sb, &QScrollBar::sliderReleased, this, &DisList::OnSliderReleased);

    setFocusPolicy(Qt::NoFocus);
}

void DisList::Refit(int cols) {
    QFontMetrics fm(tc->font());
    lineheight = fm.lineSpacing();
    extraheight = tc->document()->documentMargin() * 2 + 2;
    QSize sz(fm.horizontalAdvance(QString(cols, QChar('M'))) + tc->document()->documentMargin() * 2 +
                 sb->sizeHint().width() + 4,
             extraheight + 15 * lineheight);
    setMinimumSize(sz);
    resize(sz);
}

void DisList::MoveSB() {
    int pos;

    if (topaddr <= 100)
        pos = topaddr;
    else if (topaddr >= maxaddr - 100)
        pos = topaddr - maxaddr + 500;
    else if (topaddr < 1100)
        pos = (topaddr - 100) / 10 + 100;
    else if (topaddr >= maxaddr - 1100)
        pos = (topaddr - maxaddr + 1100) / 10 + 300;
    else  // FIXME this pos is very likely wrong... but I cannot trigger it
        pos = 250;

    const bool blocked = sb->blockSignals(true);
    sb->setRange(0, 500 - 20);
    sb->setPageStep(20);
    sb->setValue(std::clamp(pos, 0, 480));
    sb->blockSignals(blocked);
}

void DisList::OnScrollAction(int action) {
    const int pos = sb->sliderPosition();

    if (pos < 100)
        topaddr = pos;
    else if (pos >= 400)
        topaddr = maxaddr + pos - 500;
    else if (action == QAbstractSlider::SliderSingleStepSub) {
        topaddr -= back_size;
        MoveSB();
    } else if (action == QAbstractSlider::SliderSingleStepAdd) {
        if (addrs.size() > 1)
            topaddr = addrs[1];
        MoveSB();
    } else if (action == QAbstractSlider::SliderPageStepSub) {
        topaddr -= (nlines - 2) * back_size;
        MoveSB();
    } else if (action == QAbstractSlider::SliderPageStepAdd) {
        if (nlines >= 2 && static_cast<int>(addrs.size()) > nlines - 2)
            topaddr = addrs[nlines - 2];
        MoveSB();
    } else if (action == QAbstractSlider::SliderMove) {
        // Thumb tracking: wait for the release (tracking is off).
        return;
    } else {
        return;
    }

    RefillNeeded();
}

void DisList::OnSliderReleased() {
    const int pos = sb->sliderPosition();

    if (pos < 100)
        topaddr = pos;
    else if (pos >= 400)
        topaddr = maxaddr + pos - 500;
    else if (pos <= 200)
        topaddr = (pos - 100) * 10 + 100;
    else if (pos >= 300)
        topaddr = (pos - 300) * 10 + maxaddr - 1100;
    else
        // 200 .. 300 -> 1100 .. maxaddr - 1100
        topaddr = (pos - 200) * ((maxaddr - 2200) / 100) + 1100;

    MoveSB();
    RefillNeeded();
}

void DisList::wheelEvent(QWheelEvent* event) {
    const int steps = -event->angleDelta().y() / 120;
    if (steps == 0) {
        event->ignore();
        return;
    }
    for (int i = 0; i < std::abs(steps); i++) {
        OnScrollAction(steps > 0 ? QAbstractSlider::SliderSingleStepAdd
                                 : QAbstractSlider::SliderSingleStepSub);
    }
    event->accept();
}

bool DisList::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
            return true;
        case QEvent::Wheel:
            wheelEvent(static_cast<QWheelEvent*>(event));
            return true;
        default:
            break;
    }
    return QWidget::eventFilter(watched, event);
}

// Calls the owner's refill callback; it returns only when the refill is
// complete.
void DisList::RefillNeeded() {
    if (refill_callback)
        refill_callback();
}

void DisList::FillText() {
    QString val;
    for (int i = 0; i < nlines && i < strings.size(); i++) {
        val += strings[i];
        val += QLatin1Char('\n');
    }
    tc->setPlainText(val);
    tc->verticalScrollBar()->setValue(0);
}

// called by parent's refill handler or any other time strings have changed
void DisList::Refill() {
    MoveSB();
    FillText();
    SetSel();
}

// on resize, recompute shown lines and refill if necessary
void DisList::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (lineheight <= 0)
        return;

    QSize sz = size();
    const int sbw = sb->sizeHint().width();
    sz.setWidth(sz.width() - sbw);
    sb->move(sz.width(), 0);
    sb->resize(sbw, sz.height());
    nlines = std::max(1, (sz.height() + lineheight - 1) / lineheight);
    tc->move(0, 0);
    tc->resize(sz.width(), (nlines + 1) * lineheight + extraheight);

    if (nlines > strings.size())
        RefillNeeded();
    else {
        FillText();
        SetSel();
    }
}

// highlight selected line, if visible
void DisList::SetSel() {
    tc->setExtraSelections({});

    if (!issel)
        return;

    // Before the first layout / refill there is nothing to highlight yet.
    if (nlines <= 0 || addrs.empty() || static_cast<size_t>(nlines) > addrs.size() ||
        addrs[0] > seladdr || addrs[nlines - 1] <= seladdr)
        return;

    for (int i = 0; i < nlines && static_cast<size_t>(i + 1) < addrs.size(); i++) {
        if (addrs[i + 1] > seladdr) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = QTextCursor(tc->document()->findBlockByNumber(i));
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.format.setBackground(palette().highlight());
            sel.format.setForeground(palette().highlightedText());
            tc->setExtraSelections({sel});
            return;
        }
    }
}

void DisList::SetSel(uint32_t addr) {
    seladdr = addr;
    issel = true;

    if (addrs.size() < 4 || addrs.size() < static_cast<size_t>(nlines) || topaddr > addr ||
        addrs[addrs.size() - 4] < addr) {
        topaddr = addr;
        strings.clear();
        addrs.clear();
        RefillNeeded();
    } else
        SetSel();
}

// ---------------------------------------------------------------------------
// MemView

class MemView::Display final : public QWidget {
public:
    explicit Display(MemView* owner) : QWidget(owner), owner_(owner) {
        setFocusPolicy(Qt::StrongFocus);
        setAutoFillBackground(true);
        setBackgroundRole(QPalette::Base);
        setFont(MonoFont());
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter dc(this);
        dc.fillRect(rect(), palette().base());
        owner_->Paint(dc);
    }
    void mousePressEvent(QMouseEvent* ev) override { owner_->MouseEvent(ev, true); }
    void mouseMoveEvent(QMouseEvent* ev) override { owner_->MouseEvent(ev, false); }
    void mouseReleaseEvent(QMouseEvent* ev) override { owner_->MouseEvent(ev, false); }
    void keyPressEvent(QKeyEvent* ev) override { owner_->KeyEvent(ev); }

private:
    MemView* owner_;
};

MemView::MemView(QWidget* parent) : QWidget(parent) {
    disp = new Display(this);
    sb = new QScrollBar(Qt::Vertical, this);
    sb->setRange(0, 500 - 15);
    sb->setPageStep(15);
    sb->setSingleStep(1);
    sb->setTracking(false);
    connect(sb, &QScrollBar::actionTriggered, this, &MemView::OnScrollAction);
    connect(sb, &QScrollBar::sliderReleased, this, &MemView::OnSliderReleased);
    disp->installEventFilter(this);
    setFocusProxy(disp);
}

void MemView::Refit() {
    addrlen = maxaddr > 0xffff ? 8 : 4;

    QFontMetrics fm(disp->font());
    charwidth = fm.horizontalAdvance(QChar('M'));
    charheight = fm.lineSpacing();

    QSize sz(charwidth * (69 + addrlen) + sb->sizeHint().width() + 2, charheight * 15);
    setMinimumSize(sz);
    resize(sz);
}

bool MemView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == disp && event->type() == QEvent::Wheel) {
        wheelEvent(static_cast<QWheelEvent*>(event));
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MemView::wheelEvent(QWheelEvent* event) {
    const int steps = -event->angleDelta().y() / 120;
    if (steps == 0) {
        event->ignore();
        return;
    }
    for (int i = 0; i < std::abs(steps); i++) {
        OnScrollAction(steps > 0 ? QAbstractSlider::SliderSingleStepAdd
                                 : QAbstractSlider::SliderSingleStepSub);
    }
    event->accept();
}

void MemView::MouseEvent(QMouseEvent* ev, bool press) {
    // Take focus on left-down so subsequent keyboard navigation reaches
    // this MemView's KeyEvent handler.
    if (press)
        disp->setFocus();

    if (!press && !(ev->buttons() & Qt::LeftButton))
        return;
    if (press && ev->button() != Qt::LeftButton)
        return;

    if (charwidth <= 0 || charheight <= 0)
        return;

    const QPoint p = ev->pos();
    int x = p.x() / charwidth, y = p.y() / charheight;
    x -= addrlen + 3;

    if (x < 0 || y < 0 || y > nlines)
        return;

    int word, nib;
    int nnib = 2 << fmt, nword = 16 >> fmt;
    int preasc = (nnib + 1) * nword + 2;
    isasc = x >= preasc;

    if (isasc) {
        word = (x - preasc) * 2 / nnib;
        nib = (x - preasc) * 2 % nnib;
    } else {
        word = x / (nnib + 1);
        nib = x % (nnib + 1);
        nib = nnib - nib - 1;
    }

    if (nib < 0 || word >= nword)
        return;

    seladdr = topaddr + y * 16;
    selnib = word * nnib + nib;
    ShowAddr(seladdr);
}

void MemView::ShowCaret() {
    if (seladdr < static_cast<int>(topaddr) || seladdr >= static_cast<int>(topaddr) + nlines * 16)
        selnib = -1;

    if (selnib < 0) {
        caretx = carety = -1;
        if (addrlab)
            addrlab->setText(QString());
        disp->update();
        return;
    }

    if (addrlab) {
        uint32_t addr = seladdr + selnib / 2;

        if (!isasc)
            addr &= ~((1 << fmt) - 1);

        addrlab->setText(QStringLiteral("0x") + Hex(addr, addrlen));
    }

    int y = (seladdr - topaddr) / 16;
    int x = addrlen + 3;
    int nnib = 2 << fmt, nword = 16 >> fmt;

    if (isasc)
        x += (nnib + 1) * nword + 2 + selnib / 2;
    else
        x += (nnib + 1) * (selnib / nnib) + nnib - selnib % nnib - 1;

    caretx = x;
    carety = y;
    disp->update();
}

void MemView::KeyEvent(QKeyEvent* ev) {
    const int key = ev->key();
    const bool shift = ev->modifiers() & Qt::ShiftModifier;

    if (selnib < 0) {
        ev->ignore();
        return;
    }

    int nnib = 2 << fmt;
    switch (key) {
        case Qt::Key_Right:
            if (isasc)
                selnib += 2;
            else if (shift)
                selnib += 2 << fmt;
            else if (!(selnib % nnib))
                selnib += nnib + nnib - 1;
            else
                selnib--;

            if (selnib >= 32) {
                if (seladdr == static_cast<int>(maxaddr) - 16)
                    selnib = 32 - nnib;
                else {
                    selnib -= 32;
                    seladdr += 16;
                }
            }

            break;

        case Qt::Key_Left:
            if (isasc)
                selnib -= 2;
            else if (shift)
                selnib -= 2 << fmt;
            else if (!(++selnib % nnib))
                selnib -= nnib * 2;

            if (selnib < 0) {
                if (!seladdr)
                    selnib = nnib - 1;
                else {
                    selnib += 32;
                    seladdr -= 16;
                }
            }

            break;

        case Qt::Key_Down:
            if (seladdr < static_cast<int>(maxaddr) - 16)
                seladdr += 16;

            break;

        case Qt::Key_Up:
            if (seladdr > 0)
                seladdr -= 16;

            break;

        default: {
            const QString text = ev->text();
            if (text.size() != 1) {
                ev->ignore();
                return;
            }
            const int c = text.at(0).unicode();
            if (c > 0x7f || (isasc && !isprint(c)) || (!isasc && !isxdigit(c))) {
                ev->ignore();
                return;
            }

            // location in data array
            int wno = (seladdr - topaddr) / 4 + selnib / 8;
            int bno = (selnib % 8) / 2;
            int nibno = selnib % 2;

            if (wno < 0 || static_cast<size_t>(wno) >= words.size()) {
                ev->ignore();
                return;
            }

            // now that selnib/seladdr isn't needed any more, advance pointer
            if (isasc)
                selnib += 2;
            else if (!(selnib % nnib))
                selnib += nnib + nnib - 1;
            else
                selnib--;

            if (selnib >= 32) {
                if (seladdr == static_cast<int>(maxaddr) - 16)
                    selnib = 32 - nnib;
                else {
                    selnib -= 32;
                    seladdr += 16;
                }
            }

            uint32_t mask, val;

            if (isasc) {
                mask = 0xff << bno * 8;
                val = c << bno * 8;
            } else {
                mask = 8 * (0xf << bno) + 4 * nibno;
                val = isdigit(c) ? c - '0' : tolower(c) + 10 - 'a';
                val <<= bno * 8 + nibno * 4;
            }

            if ((words[wno] & mask) == val)
                break;

            words[wno] = ((words[wno] & ~mask) | val);
            writeaddr = topaddr + 4 * wno;
            val = words[wno];

            switch (fmt) {
                case 0:
                    writeval = (val >> bno * 8) & 0xff;
                    writeaddr += bno;
                    break;

                case 1:
                    writeval = (val >> (bno / 2) * 16) & 0xffff;
                    writeaddr += bno & ~1;
                    break;

                case 2:
                    writeval = val;
                    break;
            }

            // write value; this will not return until value has been written
            if (write_callback)
                write_callback();
            disp->update();
        }
    }

    ev->accept();
    ShowAddr(seladdr);
}

void MemView::MoveSB() {
    int pos;

    if (topaddr / 16 <= 100)  // <= 100
        pos = topaddr / 16;
    else if (topaddr / 16 >= maxaddr / 16 - 100)  // >= 400
        pos = topaddr / 16 - maxaddr / 16 + 500;
    else if (topaddr / 16 < 1100)  // <= 200
        pos = (topaddr / 16 - 100) / 10 + 100;
    else if (topaddr / 16 >= maxaddr / 16 - 1100)  // >= 300
        pos = (topaddr / 16 - maxaddr / 16 + 1100) / 10 + 300;
    else  // > 200 && < 300
        pos = ((topaddr / 16) - 1100) / (((maxaddr / 16) - 2200) / 100) + 200;

    const bool blocked = sb->blockSignals(true);
    sb->setRange(0, 500 - 20);
    sb->setPageStep(20);
    sb->setValue(std::clamp(pos, 0, 480));
    sb->blockSignals(blocked);
}

void MemView::OnScrollAction(int action) {
    const int pos = sb->sliderPosition();

    if (pos < 100)
        topaddr = pos * 16;
    else if (pos >= 400)
        topaddr = maxaddr + (pos - 500) * 16;
    else if (action == QAbstractSlider::SliderSingleStepSub) {
        topaddr -= 16;
        MoveSB();
    } else if (action == QAbstractSlider::SliderSingleStepAdd) {
        topaddr += 16;
        MoveSB();
    } else if (action == QAbstractSlider::SliderPageStepSub) {
        topaddr -= (nlines - 2) * 16;
        MoveSB();
    } else if (action == QAbstractSlider::SliderPageStepAdd) {
        topaddr += (nlines - 2) * 16;
        MoveSB();
    } else if (action == QAbstractSlider::SliderMove) {
        // Thumb tracking: handled on release.
        return;
    } else if (pos <= 200)
        topaddr = ((pos - 100) * 10 + 100) * 16;
    else if (pos >= 300)
        topaddr = ((pos - 300) * 10 - 1100) * 16 + maxaddr;
    else
        topaddr = ((pos - 200) * ((maxaddr / 16 - 2200) / 100) + 1100) * 16;

    RefillNeeded();
}

void MemView::OnSliderReleased() {
    const int pos = sb->sliderPosition();

    if (pos < 100)
        topaddr = pos * 16;
    else if (pos >= 400)
        topaddr = maxaddr + (pos - 500) * 16;
    else if (pos <= 200)
        topaddr = ((pos - 100) * 10 + 100) * 16;
    else if (pos >= 300)
        topaddr = ((pos - 300) * 10 - 1100) * 16 + maxaddr;
    else
        topaddr = ((pos - 200) * ((maxaddr / 16 - 2200) / 100) + 1100) * 16;

    MoveSB();
    RefillNeeded();
}

void MemView::RefillNeeded() {
    if (refill_callback)
        refill_callback();
}

// called by parent's refill handler or any other time words have changed
void MemView::Refill() {
    MoveSB();
    disp->update();
    ShowCaret();
}

void MemView::Paint(QPainter& dc) {
    dc.setFont(disp->font());
    dc.setPen(disp->palette().color(QPalette::Text));
    const QFontMetrics fm(disp->font());
    const int ascent = fm.ascent();

    for (size_t i = 0; i < static_cast<size_t>(nlines) && i < words.size() / 4; i++) {
        QString line = Hex(topaddr + static_cast<int>(i) * 16, maxaddr > 0xffff ? 8 : 4) +
                       QStringLiteral("   ");

        for (int j = 0; j < 4; j++) {
            uint32_t v = words[i * 4 + j];

            switch (fmt) {
                case 0:
                    line += Hex(v & 0xff, 2) + QLatin1Char(' ') + Hex((v >> 8) & 0xff, 2) +
                            QLatin1Char(' ') + Hex((v >> 16) & 0xff, 2) + QLatin1Char(' ') +
                            Hex((v >> 24) & 0xff, 2) + QLatin1Char(' ');
                    break;

                case 1:
                    line += Hex(v & 0xffff, 4) + QLatin1Char(' ') + Hex((v >> 16) & 0xffff, 4) +
                            QLatin1Char(' ');
                    break;

                case 2:
                    line += Hex(v, 8) + QLatin1Char(' ');
                    break;
            }
        }

        line += QStringLiteral("  ");

        for (int j = 0; j < 4; j++) {
            uint32_t v = words[i * 4 + j];
            auto appendc = [&line](uint32_t c) {
                c &= 0xff;
                line += (c < 0x80 && isprint(static_cast<int>(c))) ? QChar(static_cast<char>(c))
                                                                     : QLatin1Char('.');
            };
            appendc(v);
            appendc(v >> 8);
            appendc(v >> 16);
            appendc(v >> 24);
        }

        dc.drawText(0, static_cast<int>(i) * charheight + ascent, line);
    }

    int lloc = charwidth * ((addrlen + 1) * 2 + 1) / 2;
    dc.drawLine(lloc, 0, lloc, nlines * charheight);
    lloc = charwidth *
           (2 * (addrlen + 3 + 32 + 4 + (fmt == 0 ? 3 * 4 : fmt == 1 ? 4 : 0)) + 1) / 2;
    dc.drawLine(lloc, 0, lloc, nlines * charheight);

    // caret
    if (caretx >= 0 && carety >= 0) {
        dc.fillRect(QRect(caretx * charwidth, carety * charheight, charwidth, charheight),
                    disp->hasFocus() ? disp->palette().highlight()
                                     : disp->palette().mid());
        dc.setPen(disp->palette().color(QPalette::HighlightedText));
        // redraw the character under the caret
        if (carety < nlines && static_cast<size_t>(carety) < words.size() / 4) {
            // Recompute only the character at the caret from the words.
            int x = caretx - (addrlen + 3);
            int nnib = 2 << fmt, nword = 16 >> fmt;
            int preasc = (nnib + 1) * nword + 2;
            QChar ch;
            if (x >= preasc) {
                int byte = x - preasc;
                uint32_t v = words[carety * 4 + byte / 4];
                uint32_t c = (v >> ((byte % 4) * 8)) & 0xff;
                ch = (c < 0x80 && isprint(static_cast<int>(c))) ? QChar(static_cast<char>(c))
                                                                 : QLatin1Char('.');
            } else if (x >= 0 && x % (nnib + 1) != nnib) {
                int word = x / (nnib + 1);
                int nib = nnib - (x % (nnib + 1)) - 1;  // nibble index within word, LSB = 0
                int byteidx = word * (1 << fmt) + nib / 2;
                uint32_t v = words[carety * 4 + byteidx / 4];
                uint32_t nv = (v >> (((byteidx % 4) * 8) + (nib % 2) * 4)) & 0xf;
                ch = QString::number(nv, 16).toUpper().at(0);
            }
            if (!ch.isNull())
                dc.drawText(caretx * charwidth, carety * charheight + ascent, QString(ch));
        }
    }
}

// on resize, recompute shown lines and refill if necessary
void MemView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (charheight <= 0)
        return;

    QSize sz = size();
    const int sbw = sb->sizeHint().width();
    sz.setWidth(sz.width() - sbw);
    sb->move(sz.width(), 0);
    sb->resize(sbw, sz.height());
    nlines = std::max(1, (sz.height() + charheight - 1) / charheight);
    disp->move(0, 0);
    disp->resize(sz.width(), (nlines + 1) * charheight);

    if (static_cast<size_t>(nlines) > words.size() / 4) {
        if (topaddr + nlines * 16 > maxaddr)
            topaddr = maxaddr - nlines * 16 + 1;

        RefillNeeded();
    } else
        Refill();
}

void MemView::ShowAddr(uint32_t addr, bool force_update) {
    if (addr < topaddr || addr >= topaddr + (nlines - 1) * 16) {
        // align to nearest 16-byte block
        uint32_t newtopaddr = addr & ~0xf;

        if (newtopaddr + nlines * 16 > maxaddr)
            newtopaddr = maxaddr - nlines * 16 + 1;

        force_update = newtopaddr != topaddr;
        topaddr = newtopaddr;
    }

    if (force_update) {
        words.clear();
        RefillNeeded();
    } else
        ShowCaret();
}

uint32_t MemView::GetAddr() {
    if (selnib < 0)
        return topaddr;
    else
        return seladdr + (selnib / 2 & ~((1 << fmt) - 1));
}

// ---------------------------------------------------------------------------
// ColorView

ColorView::ColorView(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* sz = new QHBoxLayout(this);
    sz->setContentsMargins(0, 0, 0, 0);
    QFrame* frame = new QFrame(this);
    frame->setFrameShape(QFrame::Panel);
    frame->setFrameShadow(QFrame::Sunken);
    frame->setFixedSize(75, 75);
    frame->setAutoFillBackground(true);
    cp = frame;
    sz->addWidget(cp);
    QGridLayout* gs = new QGridLayout();
    gs->addWidget(new QLabel(tr("Red:"), this), 0, 0);
    rt = new QLabel(QStringLiteral("255"), this);
    rt->setMinimumWidth(rt->sizeHint().width());
    gs->addWidget(rt, 0, 1);
    gs->addWidget(new QLabel(tr("Green:"), this), 1, 0);
    gt = new QLabel(QStringLiteral("255"), this);
    gt->setMinimumWidth(gt->sizeHint().width());
    gs->addWidget(gt, 1, 1);
    gs->addWidget(new QLabel(tr("Blue:"), this), 2, 0);
    bt = new QLabel(QStringLiteral("255"), this);
    bt->setMinimumWidth(bt->sizeHint().width());
    gs->addWidget(bt, 2, 1);
    sz->addLayout(gs);
    SetRGB(-1, -1, -1);
}

void ColorView::SetRGB(int r, int g, int b) {
    r_ = r;
    g_ = g;
    b_ = b;
    QPalette pal = cp->palette();
    if (r == -1 || g == -1 || b == -1) {
        pal.setColor(QPalette::Window, palette().color(QPalette::Window));
        cp->setPalette(pal);
        rt->setText(QString());
        gt->setText(QString());
        bt->setText(QString());
        return;
    }

    pal.setColor(QPalette::Window, QColor(r, g, b));
    cp->setPalette(pal);
    // FIXME: make shift an option; currently hard-coded to rgb555
    rt->setText(QString::number(r >> 3));
    gt->setText(QString::number(g >> 3));
    bt->setText(QString::number(b >> 3));
}

// ---------------------------------------------------------------------------
// PixView

PixView::PixView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(64, 64);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizePolicy(sp);
}

bool PixView::InitBMP(int w, int h, ColorView* cv) {
    im = QImage(w, h, QImage::Format_RGB888);
    im.fill(Qt::black);
    inited = true;
    selx = sely = -1;
    cview = cv;
    setMinimumSize(w * 8, h * 8);
    return true;
}

void PixView::SetData(const unsigned char* data, int stride, int x, int y) {
    if (!inited)
        return;

    ox = x;
    oy = y;

    if (!data) {
        im.fill(Qt::black);
        selx = sely = -1;
    } else {
        data += (y * stride + x) * 3;

        for (y = 0; y < im.height(); y++) {
            uint8_t* row = im.scanLine(y);
            for (x = 0; x < im.width(); x++) {
                row[x * 3] = data[0];
                row[x * 3 + 1] = data[1];
                row[x * 3 + 2] = data[2];
                data += 3;
            }

            data += 3 * (stride - x);
        }
    }

    if (selx >= 0 && cview) {
        const QColor c = im.pixelColor(selx, sely);
        cview->SetRGB(c.red(), c.green(), c.blue());
    } else if (cview)
        cview->SetRGB(-1, -1, -1);

    update();
}

void PixView::SetSel(int x, int y, bool desel_cview_update) {
    if (x >= ox && y >= oy && x - ox < im.width() && y - oy < im.height()) {
        int oselx = selx, osely = sely;
        selx = x - ox;
        sely = y - oy;

        if (selx != oselx || sely != osely) {
            if (cview) {
                const QColor c = im.pixelColor(selx, sely);
                cview->SetRGB(c.red(), c.green(), c.blue());
            }

            update();
        }
    } else {
        bool r = selx >= 0 && sely >= 0;
        selx = sely = -1;

        if (r) {
            if (cview && desel_cview_update)
                cview->SetRGB(-1, -1, -1);

            update();
        }
    }
}

void PixView::paintEvent(QPaintEvent*) {
    if (!inited)
        return;

    QPainter dc(this);
    const int w = width(), h = height();
    const double sx = static_cast<double>(w) / im.width();
    const double sy = static_cast<double>(h) / im.height();
    dc.drawImage(QRect(0, 0, w, h), im);

    // grid color is hard-coded to gray; grid is only on top/left
    dc.setPen(Qt::gray);
    for (int y = 0; y < im.height(); y++) {
        const int py = static_cast<int>(y * sy);
        dc.drawLine(0, py, w, py);
    }
    for (int x = 0; x < im.width(); x++) {
        const int px = static_cast<int>(x * sx);
        dc.drawLine(px, 0, px, h);
    }

    if (selx >= 0) {
        // sel color is hard-coded to red
        dc.setPen(Qt::red);
        dc.setBrush(Qt::NoBrush);
        dc.drawRect(QRect(static_cast<int>(selx * sx), static_cast<int>(sely * sy),
                          static_cast<int>(sx), static_cast<int>(sy)));
    }
}

void PixView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        SelPoint(event);
}

void PixView::SelPoint(QMouseEvent* ev) {
    if (!inited)
        return;

    const int w = width(), h = height();
    const QPoint p = ev->pos();

    if (p.x() < 0 || p.x() >= w || p.y() < 0 || p.y() >= h) {
        bool r = selx >= 0 && sely >= 0;
        selx = sely = -1;

        if (r) {
            update();

            if (cview)
                cview->SetRGB(-1, -1, -1);
        }

        return;
    }

    int oselx = selx, osely = sely;
    selx = p.x() * im.width() / w;
    sely = p.y() * im.height() / h;

    if (selx != oselx || sely != osely) {
        update();

        if (cview) {
            const QColor c = im.pixelColor(selx, sely);
            cview->SetRGB(c.red(), c.green(), c.blue());
        }
    }
}

void PixViewEvt::SetData(const unsigned char* data, int stride, int x, int y) {
    PixView::SetData(data, stride, x, y);

    if (selx >= 0 && sely >= 0)
        click();
}

void PixViewEvt::SelPoint(QMouseEvent* ev) {
    PixView::SelPoint(ev);
    click();
}

void PixViewEvt::click() {
    Q_EMIT gfxClick(selx, sely);
}

// ---------------------------------------------------------------------------
// GfxPanel

GfxPanel::GfxPanel(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Dark);
}

void GfxPanel::paintEvent(QPaintEvent*) {
    if (!im || bmw <= 0 || bmh <= 0)
        return;

    QPainter dc(this);
    dc.drawImage(rect(), *im, QRect(0, 0, bmw, bmh));

    if (selx >= 0 && pv) {
        if (selx > bmw - 4 || sely > bmh - 4)
            pv->SetData(nullptr, 0, 0, 0);
        else
            pv->SetData(im->constBits(), im->bytesPerLine() / 3, selx - 4, sely - 4);
    }
}

void GfxPanel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        DoSel(event, true);
}

void GfxPanel::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        DoSel(event, false);
}

void GfxPanel::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        event->ignore();
        return;
    }

    DoSel(event);
}

void GfxPanel::DoSel(QMouseEvent* ev, bool force) {
    if (!im || bmw <= 0 || bmh <= 0)
        return;

    int x = ev->pos().x(), y = ev->pos().y();

    if (x < 0 || y < 0)
        return;

    const QSize sz = size();

    if (x > sz.width() || y > sz.height())
        return;

    x = x * bmw / sz.width();
    y = y * bmh / sz.height();
    Q_EMIT gfxClick(x, y);

    if (x < 4)
        x = 4;
    else if (x > bmw - 4)
        x = bmw - 4;

    if (y < 4)
        y = 4;
    else if (y > bmh - 4)
        y = bmh - 4;

    if (force || selx != x || sely != y) {
        selx = x;
        sely = y;
        if (pv)
            pv->SetData(im->constBits(), im->bytesPerLine() / 3, selx - 4, sely - 4);
    }
}

// ---------------------------------------------------------------------------
// GfxViewer

QString GfxViewer::bmp_save_dir_;

GfxViewer::GfxViewer(QWidget* parent, const QString& dname, int maxw, int maxh)
    : Viewer(parent, dname), image_data_(static_cast<size_t>(maxw) * maxh * 3, 0) {
    image = QImage(image_data_.data(), maxw, maxh, maxw * 3, QImage::Format_RGB888);

    gvs_ = new QScrollArea(this);
    gv = new GfxPanel();
    gv->im = &image;
    gv->bmw = maxw;
    gv->bmh = maxh;
    gv->setFixedSize(maxw, maxh);
    gvs_->setWidget(gv);
    gvs_->setWidgetResizable(false);
    gvs_->setMinimumSize(std::min(maxw, 256) + 20, std::min(maxh, 256) + 20);

    cv_ = new ColorView(this);
    zoom_ = new PixView(this);
    zoom_->InitBMP(8, 8, cv_);
    gv->pv = zoom_;

    str_ = new QCheckBox(tr("Stretch to &fit"), this);
    connect(str_, &QCheckBox::toggled, this, &GfxViewer::StretchTog);
    auto_update_ = NewAutoUpdateCheckBox();
    refresh_ = NewRefreshButton();
    save_ = new QPushButton(tr("&Save..."), this);
    connect(save_, &QPushButton::clicked, this, &GfxViewer::SaveBMP);
    close_ = NewCloseButton();
}

void GfxViewer::ChangeBMP() {
    gv->update();
}

void GfxViewer::BMPSize(int w, int h) {
    if (gv->bmw != w || gv->bmh != h) {
        gv->bmw = w;
        gv->bmh = h;

        if (!str_->isChecked()) {
            gv->setFixedSize(w, h);
        }
    }
}

void GfxViewer::StretchTog(bool checked) {
    if (checked) {
        gv->setMinimumSize(1, 1);
        gv->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        gvs_->setWidgetResizable(true);
    } else {
        gvs_->setWidgetResizable(false);
        gv->setFixedSize(gv->bmw, gv->bmh);
    }
}

void GfxViewer::SaveBMP() {
    GameArea* panel = vbamApp().frame->GetPanel();
    bmp_save_dir_ = vbamApp().frame->GetGamePath(OPTION(kGenScreenshotDir));
    // no attempt is made here to translate the dialog type name
    // it's just a suggested name, anyway
    QString def_name = panel->game_name() + QLatin1Char('-') + dname;
    def_name.chop(6);  // strlen("Viewer")

    const int capture_format = OPTION(kPrefCaptureFormat);
    if (capture_format == 0)
        def_name.append(QStringLiteral(".png"));
    else
        def_name.append(QStringLiteral(".bmp"));

    const QString png_filter = tr("PNG images (*.png)");
    const QString bmp_filter = tr("BMP images (*.bmp)");
    QString selected = capture_format ? bmp_filter : png_filter;
    const QString fn = QFileDialog::getSaveFileName(
        this, tr("Select output file"), QDir(bmp_save_dir_).filePath(def_name),
        png_filter + QStringLiteral(";;") + bmp_filter, &selected);

    if (fn.isEmpty())
        return;

    bmp_save_dir_ = QFileInfo(fn).absolutePath();

    const char* fmt = selected == bmp_filter ? "BMP" : "PNG";
    if (fn.size() > 4) {
        if (fn.endsWith(QStringLiteral(".bmp"), Qt::CaseInsensitive))
            fmt = "BMP";
        else if (fn.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
            fmt = "PNG";
    }

    image.copy(0, 0, gv->bmw, gv->bmh).save(fn, fmt);
}

// ---------------------------------------------------------------------------
// DispCheckBox

DispCheckBox::DispCheckBox(const QString& text, QWidget* parent) : QCheckBox(text, parent) {
    setFocusPolicy(Qt::NoFocus);
}

}  // namespace viewers
