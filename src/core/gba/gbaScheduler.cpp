// See gbaScheduler.h for design notes.

#include "core/gba/gbaScheduler.h"

#include <cstring>
#include <climits>

namespace {

// Since there are only a handful of event types (bounded by kSchedCount)
// and typical pending count is <= 4, a flat array keyed by type is simpler
// and faster than a binary heap. A "slot" is either unused (active=false)
// or holds the absolute cycle at which the event fires.
struct Slot {
    bool    active;
    int64_t when;   // absolute cycle
};

Slot    g_slots[kSchedCount];
int64_t g_now;      // current cycle cursor

} // namespace

namespace gbaScheduler {

void Reset() {
    for (uint32_t i = 0; i < kSchedCount; ++i) {
        g_slots[i].active = false;
        g_slots[i].when   = 0;
    }
    g_now = 0;
}

void AdvanceCycles(int32_t cycles) {
    g_now += cycles;
}

void Schedule(SchedulerEventType type, int32_t fromNow) {
    if ((uint32_t)type >= (uint32_t)kSchedCount) return;
    g_slots[type].active = true;
    g_slots[type].when   = g_now + (int64_t)fromNow;
}

void Cancel(SchedulerEventType type) {
    if ((uint32_t)type >= (uint32_t)kSchedCount) return;
    g_slots[type].active = false;
}

bool IsScheduled(SchedulerEventType type) {
    if ((uint32_t)type >= (uint32_t)kSchedCount) return false;
    return g_slots[type].active;
}

bool HasPending() {
    for (uint32_t i = 0; i < kSchedCount; ++i) {
        if (g_slots[i].active) return true;
    }
    return false;
}

int32_t NextDelta() {
    int64_t best = INT64_MAX;
    for (uint32_t i = 0; i < kSchedCount; ++i) {
        if (g_slots[i].active && g_slots[i].when < best) best = g_slots[i].when;
    }
    if (best == INT64_MAX) return INT32_MAX;
    int64_t d = best - g_now;
    if (d < 0)            return 0;
    if (d > INT32_MAX)    return INT32_MAX;
    return (int32_t)d;
}

SchedulerEventType PopExpired() {
    int64_t          best_when = INT64_MAX;
    uint32_t         best_idx  = kSchedCount;
    for (uint32_t i = 0; i < kSchedCount; ++i) {
        if (g_slots[i].active && g_slots[i].when < best_when) {
            best_when = g_slots[i].when;
            best_idx  = i;
        }
    }
    if (best_idx == kSchedCount)           return kSchedCount;
    if (best_when > g_now)                 return kSchedCount;
    g_slots[best_idx].active = false;
    g_now = best_when;
    return (SchedulerEventType)best_idx;
}

// --- State serialization -----------------------------------------------
// Layout: [uint64 now][for each slot: uint8 active, uint64 when].
// Total: 8 + kSchedCount * 9 bytes.

static constexpr size_t kSavedBytes = 8 + (size_t)kSchedCount * 9;

size_t SaveStateSize() { return kSavedBytes; }

void SaveState(void* buf) {
    uint8_t* p = (uint8_t*)buf;
    uint64_t now_u = (uint64_t)g_now;
    std::memcpy(p, &now_u, 8);
    p += 8;
    for (uint32_t i = 0; i < kSchedCount; ++i) {
        *p++ = g_slots[i].active ? 1u : 0u;
        uint64_t w = (uint64_t)g_slots[i].when;
        std::memcpy(p, &w, 8);
        p += 8;
    }
}

void LoadState(const void* buf) {
    const uint8_t* p = (const uint8_t*)buf;
    uint64_t now_u = 0;
    std::memcpy(&now_u, p, 8);
    g_now = (int64_t)now_u;
    p += 8;
    for (uint32_t i = 0; i < kSchedCount; ++i) {
        g_slots[i].active = (*p++ != 0);
        uint64_t w = 0;
        std::memcpy(&w, p, 8);
        g_slots[i].when = (int64_t)w;
        p += 8;
    }
}

} // namespace gbaScheduler
