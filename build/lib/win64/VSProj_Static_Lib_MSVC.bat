md VSProj_Static
cd VSProj_Static

cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=static_lib
cd ..
pause
