2025-11-25 Harald Oehlmann

Here is my recipe compiling with MS-VS2022 in 32 bit mode:
- start "x86 Native Tools Command Prompt for VS 2022"

- build zxing-cpp:

cmake -S zxing-cpp-wrapper-tcl -B zxing-cpp.release -DCMAKE_BUILD_TYPE=Release -DZXING_WRITERS=OFF -DZXING_C_API=ON -DZXING_EXAMPLES=OFF -DZXING_EXPERIMENTAL_API=ON -A Win32
cmake --build zxing-cpp.release -j8 --config Release

Note 1:
The "-A Win32" makes a 32 bt build. cmake does not respect the environment and builds for 64 bit if no option given.

Note 2:
The define "-DZXING_EXPERIMENTAL_API=ON" is optional. The "TryDenoise" option is available if given.

The target path above is hard coded in the make file.
If it is changed, the following line must be changed in wrappers\tcl\win\makefile.vc
PRJ_LIBS = "..\..\..\..\zxing-cpp.release\core\Release\ZXing.lib"

Go to the tcl wrapper win folder:
cd wrappers\tcl\win

Remove define in "Makefile.vc", if experimental features are not compiled in.

Compile:
nmake -f Makefile.vc TCLDIR=C:\myprograms\tcl8.6 TKDIR=C:\myprograms\tcl8.6

Install:
nmake -f Makefile.vc install TCLDIR=C:\myprograms\tcl8.6 TKDIR=C:\myprograms\tcl8.6 INSTALLDIR=..\..\..\..\installlib

The wrapper is tested with TCL/Tk 8.6.17 and 9.0.3 in 32 and 64 bit compile.
The build system for MacOS, Unix and Android is not fully set-up.

ChangeLog:

2025-11-25:
* Incorporate all upstream changes.
* Support option "TryDenoise" for builds with experimental features.
