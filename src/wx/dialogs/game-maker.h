#ifndef VBAM_WX_DIALOGS_GAME_MAKER_H_
#define VBAM_WX_DIALOGS_GAME_MAKER_H_

#include <string>

#include <wx/string.h>

namespace dialogs {

// TODO: Move this file to an internal directory once GBARomInfo has been
// converted.

// Returns the Game Maker as a string.
const wxString& GetGameMakerName(const std::string& makerCode);

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_GAME_MAKER_H_