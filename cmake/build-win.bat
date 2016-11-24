@ECHO OFF

pushd %~dp0

IF NOT EXIST build\Release (
   nuget install nupengl.core
   nuget install zlib
   mkdir build
   cd build
   mkdir Release
   mkdir Debug
   cd ..
)

IF NOT EXIST build\CMakeCache.txt (
   cd build
   cmake -G "Visual Studio 14 Win64" ..
   cd ..
)

IF NOT EXIST build\Release\zlib.dll (
   copy /y nuget-packages\zlib.v140.windesktop.msvcstl.dyn.rt-dyn.1.2.8.8\lib\native\v140\windesktop\msvcstl\dyn\rt-dyn\x64\Release\zlib.dll build\Release
   copy /y nuget-packages\zlib.v140.windesktop.msvcstl.dyn.rt-dyn.1.2.8.8\lib\native\v140\windesktop\msvcstl\dyn\rt-dyn\x64\Release\zlib.dll build\Debug
)

cmake --build build --config Release --target viewer -- -nologo -m -v:m

popd

