This library is based on UnRAR (unrarsrc-3.6.8.tar.gz) by Alexander L.
Roshal. The original sources have been heavily modified, trimmed down,
and purged of all OS-specific calls for file access and other
unnecessary operations. Refer to unrar_license.txt for full licensing
terms. The original unRAR source is available at http://www.rarlab.com/

It is important to provide 1 byte alignment for structures in model.hpp.
Now it contains '#pragma pack(1)' directive, but your compiler may
require something else. Though Unrar should work with other model.hpp
alignments, its memory requirements may increase significantly.
Alignment in other modules is not important.

Dmitry Shkarin wrote PPMII text compression. Dmitry Subbotin wrote the
carryless rangecoder.

Unrar source may be used in any software to handle RAR archives without
limitations free of charge, but cannot be used to re-create the RAR
compression algorithm, which is proprietary. Distribution of modified
Unrar source in separate form or as a part of other software is
permitted, provided that it is clearly stated in the documentation and
source comments that the code may not be used to develop a RAR (WinRAR)
compatible archiver.
-- 
Shay Green <gblargg@gmail.com>
