#ifndef VBAM_QT_VIEWERS_VIEWER_H_
#define VBAM_QT_VIEWERS_VIEWER_H_

// Debugging viewer infrastructure for the Qt frontend: the Viewer dialog base
// class and the custom widgets the viewers are built from. Port of the wx
// port's viewsupt.h.

#include <cstdint>
#include <functional>
#include <vector>

#include <QCheckBox>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "qt/dialogs/base-dialog.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QScrollBar;
class QSlider;
class QSpinBox;

namespace viewers {

// Formats `v` as an upper-case hexadecimal number with `digits` digits.
QString Hex(uint32_t v, int digits);

// A line edit accepting up to `max_digits` hexadecimal digits.
QLineEdit* NewHexEdit(QWidget* parent, int max_digits);

// Common to all viewers:
//   - tracked in MainWindow::popups (removed on close)
//   - a Close button (and Escape) closes and deletes the window
//   - the AutoUpdate checkbox toggles calling Update() every screen refresh
class Viewer : public dialogs::BaseDialog {
    Q_OBJECT

public:
    ~Viewer() override;

    // Refreshes the viewer's display from the emulated machine state. Called
    // every frame by MainWindow::UpdateViewers() when auto_update is set.
    virtual void Update() = 0;

    bool auto_update = false;

    // QDialog overrides: closing (Close button, Escape, window manager) takes
    // the viewer out of the popups list and deletes it.
    void reject() override;

protected:
    Viewer(QWidget* parent, const QString& name);

    void closeEvent(QCloseEvent* event) override;

    // Creates the "Automatic update" checkbox bound to auto_update.
    QCheckBox* NewAutoUpdateCheckBox(const QString& label = QString());
    // Creates a Close button that closes the viewer.
    QPushButton* NewCloseButton();
    // Creates a "Refresh" button calling Update().
    QPushButton* NewRefreshButton(const QString& label = QString());

    // Equivalents of the wx validators: the control writes `var` and the
    // viewer is updated (Viewer::ActiveCtrl semantics).
    void BindRadio(QRadioButton* radio, int* var, int value);
    void BindSpin(QSpinBox* spin, int* var);
    void BindSlider(QSlider* slider, int* var);

    // Shrinks the dialog to its content and fixes that as the minimum size.
    void Fit();

    const QString dname;
};

// Disassembly listing: a read-only text area with no horizontal scrollbar and
// a funky vertical scrollbar:
// range = 1 - 500
//   but/pagesz = # of lines shown/# of lines shown - 1
// 1-100 = normal
// 101-200 = 10x semi-stationary
// 201-300 = stationary @ center
// 301-400 = 10x semi-stationary
// 401-500 = normal
class DisList : public QWidget {
    Q_OBJECT

public:
    explicit DisList(QWidget* parent = nullptr);

    // Called after init to size the panel for `cols` columns.
    void Refit(int cols);
    void MoveSB();

    // Called by the owner's refill callback or any other time strings have
    // changed.
    void Refill();

    // Make addr visible and then select it.
    void SetSel(uint32_t addr);
    void UnSel() { issel = false; }

    // Set by the owner: fills `strings` / `addrs` for `nlines` lines starting
    // at `topaddr`, then calls Refill(). Called synchronously.
    std::function<void()> refill_callback;

    // currently visible lines
    int nlines = 0;
    // at least nlines strings to display
    QStringList strings;
    // and their starting addrs (mostly for scrollbar)
    std::vector<uint32_t> addrs;
    // how far back to scroll for single line
    int back_size = 4;
    // address of top line
    uint32_t topaddr = 0;
    // max address for scrollbar
    uint32_t maxaddr = 0;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void RefillNeeded();
    void SetSel();
    void OnScrollAction(int action);
    void OnSliderReleased();
    void FillText();

    QPlainTextEdit* tc;
    QScrollBar* sb;
    int lineheight = 0, extraheight = 0;
    uint32_t seladdr = 0;
    bool issel = false;
};

// A hex editor with a funky scrollbar like above. Text is drawn directly on a
// child panel so the caret and mouse selection can be controlled precisely.
class MemView : public QWidget {
    Q_OBJECT

public:
    explicit MemView(QWidget* parent = nullptr);

    // Called after init to size the panel.
    void Refit();
    void MoveSB();

    // Called by the owner's refill callback or any other time words have
    // changed.
    void Refill();

    // Make addr visible.
    void ShowAddr(uint32_t addr, bool force_update = false);

    // Current selection, or topaddr if none.
    uint32_t GetAddr();

    // Set by the owner: fills `words` for `nlines` * 4 words starting at
    // `topaddr`, then calls Refill(). Called synchronously.
    std::function<void()> refill_callback;
    // Set by the owner: writes `writeval` (of size 1 << fmt) to `writeaddr`.
    std::function<void()> write_callback;

    // currently visible lines
    int nlines = 0;
    // at least nlines * 4 words to display
    std::vector<uint32_t> words;
    // address of top line
    uint32_t topaddr = 0;
    // max address for scrollbar
    uint32_t maxaddr = 0;
    // bytes per word == (1 << fmt)
    int fmt = 2;
    // after write, these contain write addr and val
    uint32_t writeaddr = 0, writeval = 0;
    // when selection is made, this widget is updated w/ addr
    QLabel* addrlab = nullptr;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    class Display;
    friend class Display;

    void RefillNeeded();
    void OnScrollAction(int action);
    void OnSliderReleased();
    void MouseEvent(QMouseEvent* ev, bool press);
    void KeyEvent(QKeyEvent* ev);
    void Paint(QPainter& dc);
    void ShowCaret();

    // easier than checking maxaddr
    int addrlen = 8;
    Display* disp;
    QScrollBar* sb;
    int charheight = 0, charwidth = 0;
    // selection info
    int selnib = -1, seladdr = 0;
    bool isasc = false;
    // caret position in characters, or -1 when hidden
    int caretx = -1, carety = -1;
};

// Display a color in a square, with the RGB value to its right.
class ColorView : public QWidget {
    Q_OBJECT

public:
    explicit ColorView(QWidget* parent = nullptr);
    void SetRGB(int r, int g, int b);
    void GetRGB(int& _r, int& _g, int& _b) {
        _r = r_;
        _g = g_;
        _b = b_;
    }

protected:
    int r_ = -1, g_ = -1, b_ = -1;
    QWidget* cp;
    QLabel *rt, *gt, *bt;
};

// Display a small bitmap in jumbopixel style. If a pixel is selected, it is
// highlighted with a border. A ColorView can be assigned to it, and on
// selection that widget will be updated to the selected color.
class PixView : public QWidget {
    Q_OBJECT

public:
    explicit PixView(QWidget* parent = nullptr);
    bool InitBMP(int w = 8, int h = 8, ColorView* cv = nullptr);
    // stride is in pixels
    // format is rgb24
    // x/y is added to data and returned coords
    // if data == NULL, bitmap will be reset to default (all-black)
    virtual void SetData(const unsigned char* data, int stride, int x = 0, int y = 0);
    // desel if out of displayed range
    void SetSel(int x, int y, bool dsel_cview_update = true);
    // -1, -1 = no sel
    void GetSel(int& x, int& y) {
        x = selx < 0 ? -1 : ox + selx;
        y = sely < 0 ? -1 : oy + sely;
    }
    ColorView* cview = nullptr;

Q_SIGNALS:
    // Emitted by PixViewEvt when a point is selected (coords in bitmap
    // pixels, -1/-1 when deselected).
    void gfxClick(int x, int y);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void SelPoint(QMouseEvent* ev);

    QImage im;
    bool inited = false;
    int ox = 0, oy = 0, selx = -1, sely = -1;
};

// If the jumbopixel view is all there is, send a gfxClick on selection.
class PixViewEvt : public PixView {
    Q_OBJECT

public:
    explicit PixViewEvt(QWidget* parent = nullptr) : PixView(parent) {}
    // generates a gfxClick if a point is selected
    void SetData(const unsigned char* data, int stride, int x = 0, int y = 0) override;

protected:
    // always generates a gfxClick
    void SelPoint(QMouseEvent* ev) override;
    void click();
};

// A graphics viewer panel; expected to be inside of a QScrollArea. Draws the
// `bmw` x `bmh` top-left region of `*im` scaled to its size and feeds the
// zoom PixView with the pixels around the clicked point.
class GfxPanel : public QWidget {
    Q_OBJECT

public:
    explicit GfxPanel(QWidget* parent = nullptr);
    int bmw = 0, bmh = 0;
    const QImage* im = nullptr;
    PixView* pv = nullptr;

Q_SIGNALS:
    // Emitted on mouse click/drag with coords adjusted to the original bitmap
    // size regardless of scaling.
    void gfxClick(int x, int y);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void DoSel(QMouseEvent* ev, bool force = false);

private:
    int selx = -1, sely = -1;
};

// Like Viewer, common stuff to all gfx viewers; this is what actually manages
// the GfxPanel. Subclasses lay out the members below.
class GfxViewer : public Viewer {
    Q_OBJECT

public:
    void ChangeBMP();
    void BMPSize(int w, int h);

protected:
    GfxViewer(QWidget* parent, const QString& dname, int maxw, int maxh);

    // Pixel data of the (maxw x maxh) RGB24 image, stride maxw * 3.
    uint8_t* ImageData() { return image_data_.data(); }
    int ImageWidth() const { return image.width(); }
    int ImageHeight() const { return image.height(); }

    void StretchTog(bool checked);
    void SaveBMP();

    // The image wraps image_data_ (stride = maxw * 3, no padding).
    QImage image;
    GfxPanel* gv;
    // The widgets subclasses arrange: the scroll area holding gv, the zoom
    // PixView, its ColorView, and the standard checkboxes/buttons.
    QScrollArea* gvs_;
    PixView* zoom_;
    ColorView* cv_;
    QCheckBox* str_;
    QCheckBox* auto_update_;
    QPushButton* refresh_;
    QPushButton* save_;
    QPushButton* close_;

private:
    std::vector<uint8_t> image_data_;
    static QString bmp_save_dir_;
};

// A display-only checkbox which does not look like it's disabled.
class DispCheckBox : public QCheckBox {
    Q_OBJECT

public:
    explicit DispCheckBox(const QString& text, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent*) override {}
    void mouseReleaseEvent(QMouseEvent*) override {}
    void keyPressEvent(QKeyEvent*) override {}
};

// Factories for the concrete viewers (viewers.cpp / gfxviewers.cpp). The
// returned dialog is shown and owned by the popups list.
Viewer* NewDisassembleViewer(QWidget* parent);
Viewer* NewGBDisassembleViewer(QWidget* parent);
Viewer* NewIOViewer(QWidget* parent);
Viewer* NewMemViewer(QWidget* parent);
Viewer* NewGBMemViewer(QWidget* parent);
Viewer* NewMapViewer(QWidget* parent);
Viewer* NewGBMapViewer(QWidget* parent);
Viewer* NewOAMViewer(QWidget* parent);
Viewer* NewGBOAMViewer(QWidget* parent);
Viewer* NewPaletteViewer(QWidget* parent);
Viewer* NewGBPaletteViewer(QWidget* parent);
Viewer* NewTileViewer(QWidget* parent);
Viewer* NewGBTileViewer(QWidget* parent);

}  // namespace viewers

#endif  // VBAM_QT_VIEWERS_VIEWER_H_
