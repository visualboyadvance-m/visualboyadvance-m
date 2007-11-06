bin_PROGRAMS = VisualBoyAdvance

noinst_PROGRAMS = TestEmu

VisualBoyAdvance_SOURCES = \
	SDL.cpp			\
	debugger.cpp		\
	debugger.h		\
	../2xSaI.cpp		\
	../AutoBuild.h		\
	../Cheats.cpp		\
	../Cheats.h		\
	../EEprom.cpp		\
	../EEprom.h		\
	../Flash.cpp		\
	../Flash.h		\
	../GBA.cpp		\
	../GBA.h		\
	../GBAinline.h		\
	../Gfx.cpp		\
	../Gfx.h		\
	../Globals.cpp		\
	../Globals.h		\
	../Mode0.cpp		\
	../Mode1.cpp		\
	../Mode2.cpp		\
	../Mode3.cpp		\
	../Mode4.cpp		\
	../Mode5.cpp		\
	../NLS.h		\
	../Port.h		\
	../RTC.cpp		\
	../RTC.h		\
	../Sound.cpp		\
	../Sound.h		\
	../Sram.cpp		\
	../Sram.h		\
	../System.h		\
	../Text.cpp		\
	../Text.h		\
	../Util.cpp		\
	../Util.h		\
	../admame.cpp		\
	../agbprint.cpp		\
	../agbprint.h		\
	../arm-new.h		\
	../armdis.cpp		\
	../armdis.h		\
	../bilinear.cpp		\
	../bios.cpp		\
	../bios.h		\
	../elf.cpp		\
	../elf.h		\
	../expr-lex.cpp		\
	../expr.cpp		\
	../expr.cpp.h		\
	../exprNode.cpp		\
	../exprNode.h		\
	../getopt.c		\
	../getopt.h		\
	../getopt1.c		\
	../hq2x.cpp		\
	../hq2x.h		\
	../interframe.cpp	\
	../interp.h		\
	../lq2x.h		\
	../memgzio.c		\
	../memgzio.h		\
	../motionblur.cpp	\
	../pixel.cpp		\
	../remote.cpp		\
	../scanline.cpp		\
	../simple2x.cpp		\
	../thumb.h		\
	../unzip.cpp		\
	../unzip.h

VisualBoyAdvance_LDADD = @VBA_LIBS@ @SDL_LIBS@

VisualBoyAdvance_DEPENDENCIES = @VBA_LIBS@

TestEmu_SOURCES = \
	TestEmu.cpp		\
	debugger.cpp		\
	debugger.h		\
	../2xSaI.cpp		\
	../AutoBuild.h		\
	../Cheats.cpp		\
	../Cheats.h		\
	../EEprom.cpp		\
	../EEprom.h		\
	../Flash.cpp		\
	../Flash.h		\
	../GBA.cpp		\
	../GBA.h		\
	../GBAinline.h		\
	../Gfx.cpp		\
	../Gfx.h		\
	../Globals.cpp		\
	../Globals.h		\
	../Mode0.cpp		\
	../Mode1.cpp		\
	../Mode2.cpp		\
	../Mode3.cpp		\
	../Mode4.cpp		\
	../Mode5.cpp		\
	../NLS.h		\
	../Port.h		\
	../RTC.cpp		\
	../RTC.h		\
	../Sound.cpp		\
	../Sound.h		\
	../Sram.cpp		\
	../Sram.h		\
	../System.h		\
	../Text.cpp		\
	../Text.h		\
	../Util.cpp		\
	../Util.h		\
	../admame.cpp		\
	../agbprint.cpp		\
	../agbprint.h		\
	../arm-new.h		\
	../armdis.cpp		\
	../armdis.h		\
	../bios.cpp		\
	../bios.h		\
	../elf.cpp		\
	../elf.h		\
	../expr-lex.cpp		\
	../expr.cpp		\
	../expr.cpp.h		\
	../exprNode.cpp		\
	../exprNode.h		\
	../hq2x.cpp		\
	../hq2x.h		\
	../interp.h		\
	../lq2x.h		\
	../memgzio.c		\
	../memgzio.h		\
	../motionblur.cpp	\
	../pixel.cpp		\
	../remote.cpp		\
	../scanline.cpp		\
	../simple2x.cpp		\
	../thumb.h		\
	../unzip.cpp		\
	../unzip.h

TestEmu_LDADD = @VBA_LIBS@ @SDL_LIBS@

TestEmu_DEPENDENCIES = @VBA_LIBS@

AM_CPPFLAGS = \
	-I$(top_srcdir)/src		\
	-DSDL				\
	-DSYSCONFDIR=\"$(sysconfdir)\"

AM_CXXFLAGS = -fno-exceptions @SDL_CFLAGS@
