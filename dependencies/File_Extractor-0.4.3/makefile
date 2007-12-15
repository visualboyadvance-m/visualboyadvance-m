# I don't compile zlib into fex.a since most systems have a native version.
# RAR support must be enabled in blargg_config.h.

all: libfex.a demo.exe

libfex.a: fex/fex.h fex/blargg_config.h
	cd   fex;$(CXX) -I.. -c -Os -fno-rtti -fno-exceptions *.cpp
	cd  7z_C;$(CC)       -c -Os                           *.c
	cd  7z_C/LZMA_C;$(CC)       -c -Os                    *.c
	$(AR) $(ARFLAGS) libfex.a fex/*.o 7z_C/*.o 7z_C/LZMA_C/*.o
	-ranlib libfex.a

demo.exe: demo.c libfex.a
	$(CC) -o demo.exe -I. -lz demo.c libfex.a

clean:
      -$(RM) fex/*.o unrar/*.o 7z_C/*.o 7z_C/LZMA_C/*.o
      -$(RM) libfex.a demo.exe
