#ifndef VBAM_WX_CONFIG_OPTION_OBSERVER_H_
#define VBAM_WX_CONFIG_OPTION_OBSERVER_H_

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

#include "wx/config/option-id.h"

namespace config {

// Forward declarations.
class Option;

// An observer for one or multiple options. Will call `callback` every time any
// of the options is modified.
class OptionsObserver {
public:
    OptionsObserver(const OptionID& option_id,
                    std::function<void(Option*)> callback);
    OptionsObserver(const std::vector<OptionID>& option_ids,
                    std::function<void(Option*)> callback);
    ~OptionsObserver();

private:
    class CallbackOptionObserver;
    std::unordered_set<std::unique_ptr<CallbackOptionObserver>> observers_;
};

}  // namespace config

#endif  // VBAM_WX_CONFIG_OPTION_OBSERVER_H_
