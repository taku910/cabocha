Set PATH=c:\Program Files\Microsoft Visual Studio 9.0\VC\bin;%PATH%
Set INCLUDE=c:\Program Files\Microsoft Visual Studio 9.0\VC\include;c:\Program Files\Microsoft Platform SDK\Include;%INCLUDE%
Set LIB=c:\Program Files\Microsoft Visual Studio 9.0\VC\lib;c:\Program Files\Microsoft Platform SDK\Lib;%LIB%
Set COMSPEC=cmd.exe
nmake -f Makefile.msvc clean
nmake -f Makefile.msvc


