File_Extractor 1.0.0
--------------------
File_Extractor is a modular archive scanning and extraction library that
supports several popular compressed file formats. It gives a common
interface to the supported formats, allowing one version of user code.

Features:
* Simple C interface.
* Supports ZIP, GZIP, 7-Zip (7Z), and RAR[1] archive formats.
* Non-archive files act like archive of that one file, simplifying code.
* Modular design allows removal of support for unneeded archive formats.
* Optionally supports wide-character paths on Windows.
* Archive file type identification can be customized

[1] RAR support must be enabled before use, due to its special
licensing.

Author  : Shay Green <gblargg@gmail.com>
Website : http://code.google.com/p/file-extractor/
License : GNU LGPL 2.1 or later for all except unrar
Language: C interface, C++ implementation


Getting Started
---------------
Build the demo by typing "make" at the command-line. If that doesn't
work, manually build a program from demo.c and all *.c and *.cpp files
in fex/, 7z_C/, and zlib/. Run demo with test.zip in the same directory.

To enable RAR archive support, edit fex/blargg_config.h.

See fex.h for reference and fex.txt for documentation.


Files
-----
fex.txt                 Manual
license.txt             GNU LGPL 2.1 license

makefile                Builds libfex.a and demo

demo.c                  Basic usage
demo_read.c             Uses fex_read() to extract data
demo_rewind.c           Uses fex_rewind() to re-scan archive
demo_seek.c             Uses fex_seek_arc() to go back to files
demo_directory.c        Recursively scans directory for archives
demo.zip                Test archive used by demos

fex/
  blargg_config.h       Configuration (modify as needed)
  fex.h                 C interface (also usable from C++)
  (all other files)     Library sources

zlib/                   Zip/Gzip (can use your system's instead)
7z_C/                   7-Zip
unrar/                  RAR

-- 
Shay Green <gblargg@gmail.com>
