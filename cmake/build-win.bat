@ECHO OFF

IF NOT EXIST build (
   nuget install nupengl.core
   nuget install zlib
   mkdir build
   cd build
   mkdir Release
   mkdir Debug
   copy /y ..\nuget-packages\nupengl.core.redist.0.1.0.1\build\native\bin\x64\freeglut.dll Release
   copy /y ..\nuget-packages\nupengl.core.redist.0.1.0.1\build\native\bin\x64\freeglut.dll Debug
   copy /y ..\nuget-packages\zlib.v140.windesktop.msvcstl.dyn.rt-dyn.1.2.8.8\lib\native\v140\windesktop\msvcstl\dyn\rt-dyn\x64\Release\zlib.dll Release
   copy /y ..\nuget-packages\zlib.v140.windesktop.msvcstl.dyn.rt-dyn.1.2.8.8\lib\native\v140\windesktop\msvcstl\dyn\rt-dyn\x64\Release\zlib.dll Debug

   cmake -G "Visual Studio 14 Win64" ..
   cd ..
)

cmake --build build --config Release --target viewer
