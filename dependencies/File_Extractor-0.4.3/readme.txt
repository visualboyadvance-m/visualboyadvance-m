File_Extractor 0.4.3
--------------------
File_Extractor provides uniform access to files in popular compressed
archive formats. It is meant for programs which want to transparently
read from compressed files. Wrappers are also included that make the
library act like the unzip and unrarlib libraries, allowing your code to
work with minimal changes if you already use one of these libraries.

Features:
* C and C++ interfaces
* Supports ZIP, GZIP, 7-Zip (7Z), and RAR(*) archive formats
* File data can be extracted in several ways
* Uncompressed files can be treated as archives, simplifying your code
* Modular design allows removal of support for unneeded archive formats

(*) RAR support must be enabled before use, due to its special
licensing.

Author  : Shay Green <gblargg@gmail.com>
Website : http://www.slack.net/~ant/
License : GNU Lesser General Public License (LGPL) for all except RAR
Language: C or C++


Getting Started
---------------
Build demo.exe by typing "make" at the command-line. If that doesn't
work, manually build a program from demo.c, fex/*.cpp, 7z_C/*.c, and
zlib/*.c. Be sure "test.zip" is in the same directory. Running the
program should list the files and content of the test.zip archive.

To enable RAR support, edit fex/blargg_config.h.

See fex.h for reference and fex.txt for documentation.


Files
-----
fex.txt                     Manual
changes.txt                 Change log
license.txt                 LGPL license

makefile                    Builds libfex.a and demo.exe

demo.c                      Basic library usage
test.zip                    Test archive used by demo

fex_mini.cpp                Minimal version (no other sources needed)

fex/
  blargg_config.h           Configuration (modify as needed)
  
  fex.h                     C interface (also usable from C++)
  File_Extractor.h          C++ interface

  unzip.h                   Makes library act like unzip library
  unzip.cpp
  
  unrarlib.h                Makes library act like unrarlib
  unrarlib.cpp
  
  Single_File_Extractor.h   Single file support
  Single_File_Extractor.cpp
  Gzip_Reader.h
  Gzip_Reader.cpp
  
  Zip_Extractor.h           Zip support
  Zip_Extractor.cpp
  
  Zip7_Extractor.h          7-Zip support
  Zip7_Extractor.cpp
  
  Rar_Extractor.h           RAR support
  Rar_Extractor.cpp
  
  blargg_common.h           Required
  blargg_endian.h
  blargg_source.h           
  abstract_file.h
  abstract_file.cpp
  Data_Reader.h
  Data_Reader.cpp
  File_Extractor.cpp
  Zlib_Inflater.cpp
  Zlib_Inflater.h
  fex.cpp

zlib/                       Gzip support (or use standard zlib)

7z_C/                       7-Zip support

unrar/                      RAR support

-- 
Shay Green <gblargg@gmail.com>
