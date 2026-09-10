#ifndef VBAM_QT_DIALOGS_GAME_MAKER_H_
#define VBAM_QT_DIALOGS_GAME_MAKER_H_

#include <string>

#include <QString>

namespace dialogs {

// Returns the Game Maker (licensee) name for a two-character maker code.
const QString& GetGameMakerName(const std::string& makerCode);

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_GAME_MAKER_H_
