#ifndef GBSGB_H
#define GBSGB_H

void gbSgbInit();
void gbSgbShutdown();
void gbSgbCommand();
void gbSgbResetPacketState();
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
extern int gbSgbMode;
extern int gbSgbMask;
extern int gbSgbMultiplayer;
extern uint8_t gbSgbNextController;
extern int gbSgbPacketTimeout;
extern uint8_t gbSgbReadingController;
extern int gbSgbFourPlayers;

#endif // GBSGB_H
