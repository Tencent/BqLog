cmd /c .\Dont_Execute_This.bat dynamic_lib msvc
if errorlevel 1 goto :fail
cmd /c .\Dont_Execute_This.bat static_lib msvc
if errorlevel 1 goto :fail

if exist "pack" rd /s /q "pack"
md pack
cd pack

cmake ..\..\..\..\pack -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
if errorlevel 1 goto :fail
cd ..

