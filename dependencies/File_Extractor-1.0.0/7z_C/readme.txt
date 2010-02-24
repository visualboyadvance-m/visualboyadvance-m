Modified LZMA 4.65
------------------
This is just the ANSI C 7-zip extraction code from the LZMA 4.65 source
code release, with unnecessary files removed. I've made minor changes to
allow the code to compile with almost all warnings enabled in GCC.

* Made relevant functions extern "C" so that they can be called from
C++.

* Put all files in same directory and removed "../../" from #includes.

* Made private (unprototyped) functions static.

* #if'd out code that is never called.

* Removed a couple of Windows references.

-- 
Shay Green <gblargg@gmail.com>
