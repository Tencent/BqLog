cmd /c .\Dont_Execute_This.bat dynamic_lib clang
IF %exitcode% NEQ 0 (
    echo BqLog failed with exit code %exitcode%
    exit /b %exitcode%
)
cmd /c .\Dont_Execute_This.bat static_lib clang
IF %exitcode% NEQ 0 (
    echo BqLog failed with exit code %exitcode%
    exit /b %exitcode%
)

if exist "pack" rd /s /q "pack"
md pack
cd pack

cmake ..\..\..\..\pack -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
IF %exitcode% NEQ 0 (
    echo BqLog failed with exit code %exitcode%
    exit /b %exitcode%
)
cd ..

pause

