#ifndef VBAM_WX_WIDGETS_SLIDER_VALUE_LABEL_H_
#define VBAM_WX_WIDGETS_SLIDER_VALUE_LABEL_H_

#include <functional>

#include <wx/string.h>

class wxSlider;

namespace widgets {

// Puts a read-only value box under `slider`, updated as the thumb moves rather
// than when it settles. `suffix` is appended to the number, e.g. "%".
//
// The slider is re-parented into a vertical sizer that takes its place in the
// original sizer, keeping the item count and position: sliders in a
// wxFlexGridSizer stay in their own cell instead of pushing the grid along by
// one.
//
// When reset_to_default is supplied, the box also acts as a reset control: it
// reads [Default] while hovered and runs the callback when clicked. Pass nothing
// for a slider with no default to restore.
//
// Safe to call more than once on the same slider; the second call does nothing.
void AttachSliderValueLabel(wxSlider* slider, const wxString& suffix = wxEmptyString,
                            std::function<void()> reset_to_default = nullptr);

// Puts only a reset button under `slider`, permanently labelled Default. For a
// slider whose value is already displayed elsewhere, where a second readout
// would just duplicate it.
void AttachSliderDefaultButton(wxSlider* slider, std::function<void()> reset_to_default);

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_SLIDER_VALUE_LABEL_H_
