#include "../filters.hpp"

// Initialize color tables
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
    int systemRedShift    = 3;
    int systemGreenShift  = 11;
    int systemBlueShift   = 19;
    int RGB_LOW_BITS_MASK = 0x00010101;
#else
    int systemRedShift    = 27;
    int systemGreenShift  = 19;
    int systemBlueShift   = 11;
    int RGB_LOW_BITS_MASK = 0x01010100;
#endif
