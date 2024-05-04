#ifndef VBAM_WX_WIDGETS_CLIENT_DATA_H_
#define VBAM_WX_WIDGETS_CLIENT_DATA_H_

#include <cassert>

#include <wx/clntdata.h>
#include <wx/ctrlsub.h>
#include <wx/window.h>

namespace widgets {

// A simple wxClientData subclass that holds a single piece of data.
template <typename T>
class ClientData : public wxClientData {
public:
    // Returns the data stored in the ClientData object.
    static const T& From(wxWindow* window) {
        wxClientData* data = window->GetClientObject();
        assert(data);
        return static_cast<ClientData<T>*>(data)->data();
    }

    // Returns the data stored in the ClientData object for a container.
    static const T& From(wxItemContainer* container, size_t index) {
        wxClientData* data = container->GetClientObject(index);
        assert(data);
        return static_cast<ClientData<T>*>(data)->data();
    }

    explicit ClientData(const T& data) : data_(data) {}
    ~ClientData() override = default;

    const T& data() const { return data_; }

private:
    const T data_;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_CLIENT_DATA_H_
