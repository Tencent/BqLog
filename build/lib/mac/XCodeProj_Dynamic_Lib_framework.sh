mkdir XCodeProj_Dynamic_framework
cd XCodeProj_Dynamic_framework

cmake -DTARGET_PLATFORM:STRING=mac -DJAVA_SUPPORT=ON -DBUILD_TYPE:STRING=dynamic_lib -DAPPLE_LIB_FORMAT:STRING=framework ../../../../src -G "Xcode"
cd .. 