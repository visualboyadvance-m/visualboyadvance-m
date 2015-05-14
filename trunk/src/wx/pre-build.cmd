cd ..\..\src\wx
cmake.exe -P copy-events.cmake
..\..\..\dependencies\wxrc xrc\*.xrc -o wxvbam.xrs
..\..\..\dependencies\bin2c wxvbam.xrs builtin-xrc.h builtin_xrs
..\..\..\dependencies\bin2c ../vba-over.ini builtin-over.h builtin_over
..\..\..\dependencies\SubWCRev\SubWCRev.exe ..\..\.. ..\..\..\dependencies\SubWCRev\svnrev_template.h ..\svnrev.h
exit 0
