mkdir XCodeProj
cd XCodeProj

cmake ../../../../test -DTARGET_PLATFORM:STRING=mac -G "Xcode"
xcodebuild -project BqLogUnitTest.xcodeproj -scheme BqLogUnitTest -configuration RelWithDebInfo
./RelWithDebInfo/BqLogUnitTest
cd .. 

