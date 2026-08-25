#ifndef VBAM_CORE_GB_GBSGB_H_
#define VBAM_CORE_GB_GBSGB_H_

#include <cstdint>

#if !defined(__LIBRETRO__)
#include <zlib.h>
#endif  // !defined(__LIBRETRO__)

void gbSgbInit();
void gbSgbShutdown();
void gbSgbCommand();
// Abandons the packet in progress but keeps the position in a multi-packet
// command; safe to call whenever the receiver should give up on the current
// packet without discarding the ones already accepted.
void gbSgbResetPacketState();

// Abandons the whole command, position included. For a receiver that cannot
// resume what it was doing -- a boot ROM handover, or an index past the end of
// the packet buffer.
void gbSgbResetPacketStateFull();
void gbSgbReset();
void gbSgbDoBitTransfer(uint8_t);
void gbSgbRenderBorder();
#ifdef __LIBRETRO__
void gbSgbSaveGame(uint8_t*&);
void gbSgbReadGame(const uint8_t*&);
#else
void gbSgbSaveGame(gzFile);
void gbSgbReadGame(gzFile, int version);
#endif

extern uint8_t gbSgbATF[20 * 18];
extern int gbSgbMask;
extern int gbSgbMultiplayer;
extern uint8_t gbSgbNextController;
extern int gbSgbPacketTimeout;
extern uint8_t gbSgbReadingController;
extern int gbSgbFourPlayers;

#endif // VBAM_CORE_GB_GBSGB_H_
