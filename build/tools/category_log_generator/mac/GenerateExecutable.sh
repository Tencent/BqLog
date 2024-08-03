rm -rf XCodeProj
mkdir XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac ../../../../../tools/category_log_generator/ -G "Xcode"
xcodebuild -project BqLog_CategoryLogGenerator.xcodeproj -scheme BqLog_CategoryLogGenerator -configuration Debug
xcodebuild -project BqLog_CategoryLogGenerator.xcodeproj -scheme BqLog_CategoryLogGenerator -configuration Release
xcodebuild -project BqLog_CategoryLogGenerator.xcodeproj -scheme BqLog_CategoryLogGenerator -configuration RelWithDebInfo
xcodebuild -project BqLog_CategoryLogGenerator.xcodeproj -scheme BqLog_CategoryLogGenerator -configuration MinSizeRel
cd .. 

