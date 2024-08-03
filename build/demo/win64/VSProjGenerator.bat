md VSProj
cd VSProj

cmake ..\..\..\..\demo\cpp -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON
cd ..
pause

