md VSProj_Dynamic
cd VSProj_Dynamic

cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DCMAKE_GENERATOR_PLATFORM=x64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=dynamic_lib
cd ..
pause
