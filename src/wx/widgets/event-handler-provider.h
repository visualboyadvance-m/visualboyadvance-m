#ifndef VBAM_WX_WIDGETS_EVENT_HANDLER_PROVIDER_H_
#define VBAM_WX_WIDGETS_EVENT_HANDLER_PROVIDER_H_

// Forward declaration.
class wxEvtHandler;

namespace widgets {

// Abstract interface to provide the currently active event handler to use for
// user input events.
class EventHandlerProvider {
public:
    virtual ~EventHandlerProvider() = default;

    // Returns the currently active event handler to use for user input events.
    virtual wxEvtHandler* event_handler() = 0;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_EVENT_HANDLER_PROVIDER_H_
