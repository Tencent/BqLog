set "ARCH_PARAM=%~1"

if /I "%ARCH_PARAM%"=="arm64" (
  set "ARCH_PARAM=ARM64"
) else if /I "%ARCH_PARAM%"=="x64" (
  set "ARCH_PARAM=x64"
) else (
  set "ARCH_PARAM="
)

set "GEN_PLATFORM_ARG="
if defined ARCH_PARAM set "GEN_PLATFORM_ARG=-A %ARCH_PARAM%"

cmd /c .\Dont_Execute_This.bat dynamic_lib msvc %ARCH_PARAM%
if errorlevel 1 goto :fail
cmd /c .\Dont_Execute_This.bat static_lib msvc %ARCH_PARAM%
if errorlevel 1 goto :fail

if exist "pack" rd /s /q "pack"
md pack
cd pack

cmake ..\..\..\..\pack -DTARGET_PLATFORM:STRING=win64 %GEN_PLATFORM_ARG% -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
if errorlevel 1 goto :fail
cd ..

