cd ..\..\src\wx
..\..\dependencies\AStyle\bin\astyle --recursive -n --style=allman --indent=tab=4 --align-pointer=type --align-reference=name --break-blocks --pad-oper --pad-header --unpad-paren --delete-empty-lines --break-closing-brackets --keep-one-line-blocks --keep-one-line-statements --convert-tabs --remove-comment-prefix --mode=c *.cpp *.h
cmake -P copy-events.cmake
..\..\dependencies\wxrc xrc\*.xrc -o wxvbam.xrs
..\..\dependencies\bin2c wxvbam.xrs builtin-xrc.h builtin_xrs
..\..\dependencies\bin2c ../vba-over.ini builtin-over.h builtin_over
..\..\dependencies\SubWCRev\SubWCRev.exe ..\..\.. ..\..\..\dependencies\SubWCRev\svnrev_template.h ..\svnrev.h
exit 0
