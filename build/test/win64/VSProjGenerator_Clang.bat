md VSProj
cd VSProj

cmake ..\..\..\..\test -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64  -DJAVA_SUPPORT=ON -T ClangCl
cd ..
pause

