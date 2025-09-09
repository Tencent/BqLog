cmd /c .\Dont_Execute_This.bat dynamic_lib clang
cmd /c .\Dont_Execute_This.bat static_lib clang

if exist "pack" rd /s /q "pack"
md pack
cd pack

cmake ..\..\..\..\pack -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
cd ..

pause

