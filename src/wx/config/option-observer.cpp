#include "wx/config/option-observer.h"

#include "core/base/check.h"
#include "wx/config/option.h"

namespace config {

// An Option::Observer that calls a callback when an option has changed.
class OptionsObserver::CallbackOptionObserver : public Option::Observer {
public:
    CallbackOptionObserver(const OptionID& option_id,
                           std::function<void(Option*)> callback)
        : Option::Observer(option_id), callback_(std::move(callback)) {
        VBAM_CHECK(callback_);
    }
    ~CallbackOptionObserver() override = default;

private:
    // Option::Observer implementation.
    void OnValueChanged() override { callback_(option()); }

    std::function<void(Option*)> callback_;
};

OptionsObserver::OptionsObserver(const OptionID& option_id,
                                 std::function<void(Option*)> callback)
    : OptionsObserver(std::vector<OptionID>({option_id}), callback) {}

OptionsObserver::OptionsObserver(const std::vector<OptionID>& option_ids,
                                 std::function<void(Option*)> callback) {
    observers_.reserve(option_ids.size());
    for (const OptionID& option_id : option_ids) {
        observers_.emplace(
            std::make_unique<CallbackOptionObserver>(option_id, callback));
    }
}

OptionsObserver::~OptionsObserver() = default;

}  // namespace config
