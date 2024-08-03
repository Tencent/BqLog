mkdir XCodeProj_Static_dylib
cd XCodeProj_Static_dylib

cmake -DTARGET_PLATFORM:STRING=mac -DJAVA_SUPPORT=ON -DBUILD_TYPE:STRING=static_lib -DAPPLE_LIB_FORMAT:STRING=dylib ../../../../src -G "Xcode"
cd .. 