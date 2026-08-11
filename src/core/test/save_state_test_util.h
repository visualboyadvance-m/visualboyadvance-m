#ifndef VBAM_CORE_TEST_SAVE_STATE_TEST_UTIL_H_
#define VBAM_CORE_TEST_SAVE_STATE_TEST_UTIL_H_

// Support for the save state deserialization hardening tests.
//
// Each of those tests hands a hand-crafted blob to one of the core's
// *ReadGame() functions, which is exactly what loading a malicious .sgm does:
// utilReadData() walks a variable_desc[] and dumps each entry's bytes straight
// into the corresponding global, so a save state is just the concatenation of
// those fields. Building one by hand is therefore a matter of appending the
// fields in declaration order.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

#include "core/base/file_util.h"

#ifndef VBAM_TEST_TMP_DIR
#error "VBAM_TEST_TMP_DIR must be defined by the build"
#endif

namespace vbam_test {

// Accumulates save state fields in declaration order.
class StateBlob {
public:
    // Append a POD field exactly as utilWriteData() would.
    template <typename T>
    StateBlob& Add(const T& value)
    {
        return AddBytes(&value, sizeof(T));
    }

    StateBlob& AddBytes(const void* data, size_t size)
    {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        bytes_.insert(bytes_.end(), p, p + size);
        return *this;
    }

    // Append `size` bytes of `fill`, for the bulk memory regions (VRAM, flash,
    // EEPROM, ...) whose contents don't matter to these tests.
    StateBlob& AddFill(size_t size, uint8_t fill = 0)
    {
        bytes_.insert(bytes_.end(), size, fill);
        return *this;
    }

    // Overwrite a field that was already appended, for tests that start from a
    // state the core itself produced and corrupt one value in it.
    template <typename T>
    void PatchAt(size_t offset, const T& value)
    {
        memcpy(bytes_.data() + offset, &value, sizeof(T));
    }

    template <typename T>
    T ReadAt(size_t offset) const
    {
        T value{};
        memcpy(&value, bytes_.data() + offset, sizeof(T));
        return value;
    }

    const uint8_t* data() const { return bytes_.data(); }
    size_t size() const { return bytes_.size(); }

private:
    std::vector<uint8_t> bytes_;
};

// A gzip file under the build tree holding a save state blob. The core's
// desktop deserializers take a gzFile, so the blob has to reach them through
// one; `name` must be unique per test since ctest may run tests in parallel.
class TempStateFile {
public:
    explicit TempStateFile(const char* name)
        : path_(std::string(VBAM_TEST_TMP_DIR) + "/" + name + ".state")
    {
    }

    ~TempStateFile() { std::remove(path_.c_str()); }

    TempStateFile(const TempStateFile&) = delete;
    TempStateFile& operator=(const TempStateFile&) = delete;

    // Returns false if the blob could not be written in full.
    bool Write(const StateBlob& blob) const
    {
        return Write(blob.data(), blob.size());
    }

    bool Write(const void* data, size_t size) const
    {
        gzFile out = utilGzOpen(path_.c_str(), "wb");
        if (out == nullptr)
            return false;
        const int written = utilGzWrite(out, const_cast<void*>(data),
            static_cast<unsigned int>(size));
        utilGzClose(out);
        return written == static_cast<int>(size);
    }

    // Caller owns the handle and must utilGzClose() it.
    gzFile OpenRead() const { return utilGzOpen(path_.c_str(), "rb"); }

    // Open for writing so a core *SaveGame() can produce a real state.
    gzFile OpenWrite() const { return utilGzOpen(path_.c_str(), "wb"); }

    // Slurp the decompressed contents back, so a test can corrupt one field of
    // a state the core produced and feed it back in.
    StateBlob ReadAll() const
    {
        StateBlob blob;
        gzFile in = utilGzOpen(path_.c_str(), "rb");
        if (in == nullptr)
            return blob;

        uint8_t chunk[4096];
        for (;;) {
            const int got = utilGzRead(in, chunk, sizeof(chunk));
            if (got <= 0)
                break;
            blob.AddBytes(chunk, static_cast<size_t>(got));
            if (got < static_cast<int>(sizeof(chunk)))
                break;
        }
        utilGzClose(in);
        return blob;
    }

private:
    std::string path_;
};

}  // namespace vbam_test

#endif  // VBAM_CORE_TEST_SAVE_STATE_TEST_UTIL_H_
