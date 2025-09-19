@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ------------------------------------------------------------
rem Unified build/generate/pack driver for Windows
rem
rem Usage patterns (all parameters optional; missing ones will be prompted):
rem   Dont_Execute_This.bat all [arch] [compiler] [java] [node]
rem   Dont_Execute_This.bat build [arch] [compiler] [java] [node] [static_lib|dynamic_lib]
rem   Dont_Execute_This.bat gen-vsproj [arch] [msvc|clang] [java] [node] [static_lib|dynamic_lib]
rem   Dont_Execute_This.bat pack [arch]
rem
rem Legacy compatibility:
rem   Dont_Execute_This.bat dynamic_lib [arch] [compiler] [java] [node]
rem   Dont_Execute_This.bat static_lib  [arch] [compiler] [java] [node]
rem
rem Normalized values:
rem   arch      : ARM64 | x64
rem   compiler  : msvc | clang | mingw
rem   java,node : ON | OFF
rem   lib type  : static_lib | dynamic_lib
rem ------------------------------------------------------------

set "BUILD_JOBS=10"

set "ACTION=%~1"
set "ARG1=%~2"
set "ARG2=%~3"
set "ARG3=%~4"
set "ARG4=%~5"
set "ARG5=%~6"

if /I "%ACTION%"=="dynamic_lib" (
  set "BUILD_LIB_TYPE=dynamic_lib"
  set "ACTION=build"
) else if /I "%ACTION%"=="static_lib" (
  set "BUILD_LIB_TYPE=static_lib"
  set "ACTION=build"
)

if not defined ACTION set "ACTION=all"

rem Common params (may be used by multiple actions)
call :normalize_arch "%ARG1%" ARCH_PARAM
call :normalize_compiler "%ARG2%" COMPILER_TYPE
call :normalize_onoff "%ARG3%" JAVA_SUPPORT
call :normalize_onoff "%ARG4%" NODE_API_SUPPORT
call :normalize_build_lib_type "%ARG5%" BUILD_LIB_TYPE

if /I "%ACTION%"=="all" (
  call :ensure_common_params
  call :build_one dynamic_lib || exit /b 1
  call :build_one static_lib  || exit /b 1
  call :do_pack               || exit /b 1
  goto :success
) else if /I "%ACTION%"=="build" (
  call :ensure_common_params
  if not defined BUILD_LIB_TYPE call :ask_build_lib_type "Select library type" BUILD_LIB_TYPE
  call :build_one "%BUILD_LIB_TYPE%" || exit /b 1
  goto :success
) else if /I "%ACTION%"=="gen-vsproj" (
  call :ensure_common_params
  if not defined BUILD_LIB_TYPE call :ask_build_lib_type "Select library type" BUILD_LIB_TYPE
  call :gen_vsproj || exit /b 1
  goto :success
) else if /I "%ACTION%"=="pack" (
  if not defined ARCH_PARAM call :ensure_arch_only
  call :do_pack || exit /b 1
  goto :success
) else (
  echo Unknown action: %ACTION%
  echo Supported: all ^| build ^| gen-vsproj ^| pack ^| dynamic_lib ^| static_lib
  exit /b 2
)

:success
echo ---------
echo Finished!
echo ---------
exit /b 0


rem ================= helpers: prompts and normalization =================

:ensure_common_params
if not defined ARCH_PARAM call :ask_arch ARCH_PARAM
if not defined COMPILER_TYPE call :ask_compiler COMPILER_TYPE
if not defined JAVA_SUPPORT call :ask_yes_no "Enable Java/JNI support?" JAVA_SUPPORT
if not defined NODE_API_SUPPORT call :ask_yes_no "Enable Node-API (Node.js) support?" NODE_API_SUPPORT
goto :eof

:ensure_arch_only
call :detect_host_arch ARCH_PARAM
if not defined ARCH_PARAM call :ask_arch ARCH_PARAM
goto :eof

:normalize_arch
set "VAL=%~1"
set "OUTVAR=%~2"
set "%OUTVAR%="
if /I "%VAL%"=="arm64" set "%OUTVAR%=ARM64"
if /I "%VAL%"=="ARM64" set "%OUTVAR%=ARM64"
if /I "%VAL%"=="x64"   set "%OUTVAR%=x64"
goto :eof

:detect_host_arch
set "OUTVAR=%~1"
set "HOST_ARCH="
if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "HOST_ARCH=ARM64"
if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "HOST_ARCH=x64"
if not defined HOST_ARCH if /I "%PROCESSOR_ARCHITEW6432%"=="ARM64" set "HOST_ARCH=ARM64"
if not defined HOST_ARCH if /I "%PROCESSOR_ARCHITEW6432%"=="AMD64" set "HOST_ARCH=x64"
if defined HOST_ARCH set "%OUTVAR%=%HOST_ARCH%"
goto :eof

:ask_arch
set "OUTVAR=%~1"
echo.
echo Select architecture:
echo   [A] ARM64
echo   [X] x64
choice /C AX /N /M "Enter choice (A/X): "
if errorlevel 2 set "%OUTVAR%=x64"  & goto :eof
if errorlevel 1 set "%OUTVAR%=ARM64" & goto :eof
goto :ask_arch

:normalize_compiler
set "VAL=%~1"
set "OUTVAR=%~2"
set "%OUTVAR%="
if /I "%VAL%"=="msvc"  set "%OUTVAR%=msvc"
if /I "%VAL%"=="clang" set "%OUTVAR%=clang"
if /I "%VAL%"=="mingw" set "%OUTVAR%=mingw"
goto :eof

:ask_compiler
set "OUTVAR=%~1"
echo.
echo Select compiler:
echo   [M] MSVC
echo   [C] Clang (ClangCL)
echo   [G] MinGW (Clang targeting mingw)
choice /C MCG /N /M "Enter choice (M/C/G): "
if errorlevel 3 set "%OUTVAR%=mingw" & goto :eof
if errorlevel 2 set "%OUTVAR%=clang" & goto :eof
if errorlevel 1 set "%OUTVAR%=msvc"  & goto :eof
goto :ask_compiler

:normalize_onoff
set "VAL=%~1"
set "OUTVAR=%~2"
set "%OUTVAR%="
if /I "%VAL%"=="on"  set "%OUTVAR%=ON"
if /I "%VAL%"=="off" set "%OUTVAR%=OFF"
if /I "%VAL%"=="yes" set "%OUTVAR%=ON"
if /I "%VAL%"=="no"  set "%OUTVAR%=OFF"
if /I "%VAL%"=="1"   set "%OUTVAR%=ON"
if /I "%VAL%"=="0"   set "%OUTVAR%=OFF"
goto :eof

:normalize_build_lib_type
set "VAL=%~1"
set "OUTVAR=%~2"
set "%OUTVAR%="
if /I "%VAL%"=="static_lib"  set "%OUTVAR%=static_lib"
if /I "%VAL%"=="dynamic_lib" set "%OUTVAR%=dynamic_lib"
goto :eof

:ask_build_lib_type
set "OUTVAR=%~2"
echo.
echo Select library type:
echo   [S] static library
echo   [D] dynamic library
choice /C SD /N /M "Enter choice (S/D): "
if errorlevel 2 set "%OUTVAR%=dynamic_lib" & goto :eof
if errorlevel 1 set "%OUTVAR%=static_lib"  & goto :eof
goto :ask_build_lib_type

:ask_yes_no
set "PROMPT=%~1"
set "OUTVAR=%~2"
echo.
choice /C YN /N /M "%PROMPT% (Y/N): "
if errorlevel 2 set "%OUTVAR%=OFF" & goto :eof
if errorlevel 1 set "%OUTVAR%=ON"  & goto :eof
goto :ask_yes_no


rem ================= core actions =================

:build_one
set "BUILD_LIB_TYPE_ARG=%~1"
if /I not "%BUILD_LIB_TYPE_ARG%"=="static_lib" if /I not "%BUILD_LIB_TYPE_ARG%"=="dynamic_lib" (
  echo Invalid BUILD_LIB_TYPE: %BUILD_LIB_TYPE_ARG%
  exit /b 2
)

set "VS_GEN_PLATFORM_ARG="
if /I "%ARCH_PARAM%"=="ARM64" set "VS_GEN_PLATFORM_ARG=-A ARM64"
if /I "%ARCH_PARAM%"=="x64"   set "VS_GEN_PLATFORM_ARG=-A x64"

echo.
echo ===== Build Configuration =====
echo   ACTION              : build (%BUILD_LIB_TYPE_ARG%)
echo   ARCH                : %ARCH_PARAM%
echo   COMPILER            : %COMPILER_TYPE%
echo   JAVA_SUPPORT        : %JAVA_SUPPORT%
echo   NODE_API_SUPPORT    : %NODE_API_SUPPORT%
echo   CMake -A            : %VS_GEN_PLATFORM_ARG%
echo =================================
echo.

set "PROJ_DIR=VSProj_%BUILD_LIB_TYPE_ARG%"
if exist "%PROJ_DIR%" rd /s /q "%PROJ_DIR%" || exit /b 1
md "%PROJ_DIR%" || exit /b 1
cd "%PROJ_DIR%" || exit /b 1

set "SRC_DIR=..\..\..\..\src"

set "CONFIGS=Debug MinSizeRel RelWithDebInfo Release"

if /I "%COMPILER_TYPE%"=="mingw" (
  for %%c in (%CONFIGS%) do (
    set "CUR_CFG=%%c"
    set "MINGW_TARGET_ARGS="
    echo   BUILD FOR CONFIG             %BUILD_LIB_TYPE_ARG%   : !CUR_CFG!
    if /I "%ARCH_PARAM%"=="ARM64" (
      set "MINGW_TARGET_ARGS=-DCMAKE_C_COMPILER_TARGET=aarch64-w64-windows-gnu -DCMAKE_CXX_COMPILER_TARGET=aarch64-w64-windows-gnu -DCMAKE_SYSTEM_PROCESSOR=aarch64"
    ) else (
      set "MINGW_TARGET_ARGS=-DCMAKE_C_COMPILER_TARGET=x86_64-w64-windows-gnu -DCMAKE_CXX_COMPILER_TARGET=x86_64-w64-windows-gnu -DCMAKE_SYSTEM_PROCESSOR=x86_64"
    )

    cmake "!SRC_DIR!" -G "Ninja" ^
      -DTARGET_PLATFORM:STRING=win64 ^
      -DBUILD_TYPE=%BUILD_LIB_TYPE_ARG% ^
      -DJAVA_SUPPORT:BOOL=%JAVA_SUPPORT% ^
      -DNODE_API_SUPPORT:BOOL=%NODE_API_SUPPORT% ^
      -DCMAKE_BUILD_TYPE=!CUR_CFG! ^
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_RC_COMPILER=llvm-rc ^
      -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld ^
      !MINGW_TARGET_ARGS! || exit /b 1

    if not exist build.ninja exit /b 1
    cmake --build . --parallel %BUILD_JOBS% || exit /b 1
    cmake --install . || exit /b 1
  )
) else (
  if /I "%COMPILER_TYPE%"=="clang" (
    cmake "!SRC_DIR!" -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DBUILD_LIB_TYPE=%BUILD_LIB_TYPE_ARG% -DJAVA_SUPPORT:BOOL=%JAVA_SUPPORT% -DNODE_API_SUPPORT:BOOL=%NODE_API_SUPPORT% -T ClangCl || exit /b 1
  ) else (
    cmake "!SRC_DIR!" -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DBUILD_LIB_TYPE=%BUILD_LIB_TYPE_ARG% -DJAVA_SUPPORT:BOOL=%JAVA_SUPPORT% -DNODE_API_SUPPORT:BOOL=%NODE_API_SUPPORT% || exit /b 1
  )

  set "CONFIGS=Debug MinSizeRel RelWithDebInfo Release"
  for %%c in (%CONFIGS%) do (
    set "CUR_CFG=%%c"
    echo   BUILD FOR CONFIG             %BUILD_LIB_TYPE_ARG%   : !CUR_CFG!
    cmake --build . --config !CUR_CFG! --parallel %BUILD_JOBS% || exit /b 1
    cmake --install . --config !CUR_CFG! || exit /b 1
  )
)

cd ..
goto :eof


:gen_vsproj
set "VS_GEN_PLATFORM_ARG="
if /I "%ARCH_PARAM%"=="ARM64" set "VS_GEN_PLATFORM_ARG=-A ARM64"
if /I "%ARCH_PARAM%"=="x64"   set "VS_GEN_PLATFORM_ARG=-A x64"

if /I "%COMPILER_TYPE%"=="mingw" (
  echo gen-vsproj requires MSVC or ClangCL. MinGW is not supported for Visual Studio solutions.
  exit /b 2
)

echo.
echo ===== VS Project Generation =====
echo   ARCH                : %ARCH_PARAM%
echo   COMPILER            : %COMPILER_TYPE%
echo   JAVA_SUPPORT        : %JAVA_SUPPORT%
echo   NODE_API_SUPPORT    : %NODE_API_SUPPORT%
echo   BUILD_LIB_TYPE      : %BUILD_LIB_TYPE%
echo   CMake -A            : %VS_GEN_PLATFORM_ARG%
echo =================================
echo.

if not exist "VSProj" md "VSProj"
cd "VSProj" || exit /b 1

set "SRC_DIR=..\..\..\..\src"
if /I "%COMPILER_TYPE%"=="clang" (
  cmake "!SRC_DIR!" -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DBUILD_TYPE=%BUILD_LIB_TYPE% -DJAVA_SUPPORT:BOOL=%JAVA_SUPPORT% -DNODE_API_SUPPORT:BOOL=%NODE_API_SUPPORT% -T ClangCl || exit /b 1
) else (
  cmake "!SRC_DIR!" -DTARGET_PLATFORM:STRING=win64 %VS_GEN_PLATFORM_ARG% -DBUILD_TYPE=%BUILD_LIB_TYPE% -DJAVA_SUPPORT:BOOL=%JAVA_SUPPORT% -DNODE_API_SUPPORT:BOOL=%NODE_API_SUPPORT% || exit /b 1
)

cd ..
goto :eof


:do_pack
set "GEN_PLATFORM_ARG="
if /I "%ARCH_PARAM%"=="ARM64" set "GEN_PLATFORM_ARG=-A ARM64"
if /I "%ARCH_PARAM%"=="x64"   set "GEN_PLATFORM_ARG=-A x64"

echo.
echo ===== Packaging =====
echo   ARCH                : %ARCH_PARAM%
echo   CMake -A            : %GEN_PLATFORM_ARG%
echo =================================
echo.

if exist "pack" rd /s /q "pack"
md pack || exit /b 1
cd pack || exit /b 1

set "PACK_DIR=..\..\..\..\pack"

cmake "!PACK_DIR!" ^
  -DTARGET_PLATFORM:STRING=win64 ^
  %GEN_PLATFORM_ARG% ^
  -DPACKAGE_NAME:STRING=bqlog-lib || exit /b 1

cmake --build . --target package || exit /b 1

cd ..
goto :eof