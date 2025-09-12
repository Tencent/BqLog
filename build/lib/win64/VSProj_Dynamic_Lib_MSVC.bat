md VSProj_Dynamic
cd VSProj_Dynamic

cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=dynamic_lib
cd ..
pause
