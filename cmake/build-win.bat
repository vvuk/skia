@ECHO OFF

pushd %~dp0

IF NOT EXIST build\Release (
   nuget install nupengl.core
   nuget install zlib
   mkdir build
)

IF NOT EXIST build\CMakeCache.txt (
   pushd build
   cmake -G "Visual Studio 14 Win64" ..
   popd
)

FOR %%B IN (Debug Release RelWithDebInfo) DO (
   pushd build
   IF NOT EXIST %%B mkdir %%B
   IF NOT EXIST %%B\zlib.dll copy /y ..\nuget-packages\zlib.v140.windesktop.msvcstl.dyn.rt-dyn.1.2.8.8\lib\native\v140\windesktop\msvcstl\dyn\rt-dyn\x64\Release\zlib.dll %%B
   popd
)

cmake --build build --config RelWithDebInfo --target viewer -- -nologo -m -v:m

popd

