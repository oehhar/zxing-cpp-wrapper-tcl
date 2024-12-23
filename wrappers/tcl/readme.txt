2024-12-23 Harald Oehlmann
This is work in progress to find out which feature is suitable.
Keys in the return dict and options may be removed, if not helpful for TCL.

Currently, I only compile with MS-VS2022 in 32 bit mode.

Here is my recipe:
- start "x86 Native Tools Command Prompt for VS 2022"

- build zxing-cpp:

cmake -S zxing-cpp-wrapper-tcl -B zxing-cpp.release -DCMAKE_BUILD_TYPE=Release -DZXING_WRITERS=OFF -DZXING_C_API=ON -DZXING_EXAMPLES=OFF -A Win32
cmake --build zxing-cpp.release -j8 --config Release

Note 1:
The "-A Win32" makes a 32 bt build. cmake does not respect the environment and builds for 64 bit if no option given.

The target path above is hard coded in the make file.
If it is changed, the following line must be changed in wrappers\tcl\win\makefile.vc
PRJ_LIBS = "..\..\..\..\zxing-cpp.release\core\Release\ZXing.lib"

Go to the tcl wrapper win folder:
cd wrappers\tcl\win

Compile:
nmake -f Makefile.vc TCLDIR=C:\myprograms\tcl8.6 TKDIR=C:\myprograms\tcl8.6

Install:
nmake -f Makefile.vc install TCLDIR=C:\myprograms\tcl8.6 TKDIR=C:\myprograms\tcl8.6 INSTALLDIR=..\..\..\..\installlib

The wrapper is tested with TCL/Tk 8.6.13 and 9.0.1 in 32 and 64 bit compile.
The build system for MacOS, Unix and Android is not fully set-up.
