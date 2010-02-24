# Doesn't compile zlib into fex.a since most systems have a native version already.
# To enable RAR support, edit blargg_config.h.

all: libfex.a demo

libfex.a: fex/fex.h fex/blargg_config.h
	cd   fex;$(CXX) -I.. -c -Os -fno-rtti -fno-exceptions *.cpp
	cd unrar;$(CXX)      -c -Os -fno-rtti -fno-exceptions *.cpp
	cd  7z_C;$(CC)       -c -Os *.c
	$(AR) $(ARFLAGS) libfex.a fex/*.o unrar/*.o 7z_C/*.o
	-ranlib libfex.a
	-$(RM) fex/*.o
	-$(RM) unrar/*.o
	-$(RM) 7z_C/*.o

demo: demo.c libfex.a
	$(CC) -o demo -I. -lz demo.c libfex.a

clean:
	-$(RM) libfex.a demo
