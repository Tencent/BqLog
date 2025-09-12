md VSProj_Static
cd VSProj_Static

cmake ..\..\..\..\src -DTARGET_PLATFORM:STRING=win64 -DJAVA_SUPPORT=ON -DBUILD_TYPE=static_lib
cd ..
pause
