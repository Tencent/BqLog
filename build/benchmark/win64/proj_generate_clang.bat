md VSProj
cd VSProj

cmake ..\..\..\..\benchmark\cpp -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -T ClangCl
cd ..
pause

