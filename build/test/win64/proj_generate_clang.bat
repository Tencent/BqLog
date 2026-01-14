md VSProj
cd VSProj

cmake ..\..\..\..\test\cpp -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -T ClangCl
cd ..
pause

