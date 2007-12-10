# I don't compile zlib into fex.a since most systems have a native version.
# RAR support must be enabled in blargg_config.h.

all: libfex.a demo.exe

libfex.a: fex/fex.h fex/blargg_config.h
	cd   fex;$(CXX) -I.. -c -Os -fno-rtti -fno-exceptions *.cpp
	cd unrar;$(CXX)      -c -Os -fno-rtti -fno-exceptions *.cpp
	cd  7z_C;$(CC)       -c -Os                           *.c
	$(AR) $(ARFLAGS) libfex.a fex/*.o unrar/*.o 7z_c/*.o
	-ranlib libfex.a
	-$(RM) fex/*.o unrar/*.o 7z_C/*.o

demo.exe: demo.c libfex.a
	$(CC) -o demo.exe -I. -lz demo.c libfex.a

clean:
	-$(RM) libfex.a demo.exe
