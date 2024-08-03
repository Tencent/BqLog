rm -rf XCodeProj
mkdir XCodeProj
cd XCodeProj

cmake -DTARGET_PLATFORM:STRING=mac ../../../../../tools/log_decoder/ -G "Xcode"
xcodebuild -project BqLog_LogDecoder.xcodeproj -scheme BqLog_LogDecoder -configuration RelWithDebInfo
xcodebuild -project BqLog_LogDecoder.xcodeproj -scheme BqLog_LogDecoder -configuration MinSizeRel
cd .. 

