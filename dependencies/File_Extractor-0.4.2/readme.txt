File_Extractor 0.4.2
--------------------
File_Extractor provides uniform access to files in popular compressed
archive formats. It also includes wrappers to make it act like libunrar
and the unzip library, allowing minimal changes to your code if you
already use one of these libraries.

Features:
* C and C++ interfaces
* Supports ZIP, 7-ZIP, RAR(*), and GZIP archive formats
* File data can be extracted in several ways
* Uncompressed files can be treated as archives, simplifying your code
* Modular design allows removal of support for unused archive formats

(*) RAR support must be enabled before use, due to its special licensing

Author  : Shay Green <gblargg@gmail.com>
Website : http://www.slack.net/~ant/
License : GNU Lesser General Public License (LGPL) for all except unrar
Language: C or C++


Getting Started
---------------
Build a program consisting of demo.c and all source files in fex/,
7z_C/, and zlib/. Be sure "test.zip" is in the same directory. Running
the program should list the files and content of the test.zip archive.

To enable RAR support, edit fex/blargg_config.h.

See fex.h for reference and fex.txt for documentation.


Files
-----
fex.txt                     Manual
changes.txt                 Change log
license.txt                 LGPL license

demo.c                      Basic library usage
test.zip                    Test archive used by demo

fex/
  blargg_config.h           Configuration (modify as needed)
  
  fex.h                     C interface (also usable from C++)
  File_Extractor.h          C++ interface

  unzip.h                   Makes library act like unzip library
  unzip.cpp
  
  unrarlib.h                Makes library act like unrarlib
  unrarlib.cpp

  Single_File_Extractor.h   Single file handling
  Single_File_Extractor.cpp
  Gzip_Reader.h
  Gzip_Reader.cpp
  
  Zip_Extractor.h           Zip archive support
  Zip_Extractor.cpp
  
  Zip7_Extractor.h          7-Zip archive support
  Zip7_Extractor.cpp
  
  Rar_Extractor.h           RAR archive support (see unrar_license.txt)
  Rar_Extractor.cpp
  
  blargg_common.h           Common files needed by all formats
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

7z_C/                       7-zip support
unrar/                      RAR support (see unrar/readme.txt)
zlib/                       Gzip support (minimal zlib sources)

-- 
Shay Green <gblargg@gmail.com>
