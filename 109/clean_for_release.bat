del /s *.lib *.exp *.pdb
mkdir bin_109
mkdir bin_109\x86
mkdir bin_109\x64
move Release\*     bin_109\x86
move x64\Release\* bin_109\x64
pause
