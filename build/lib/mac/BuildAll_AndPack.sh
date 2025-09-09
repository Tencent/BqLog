#!/bin/zsh
set -euo pipefail
./Dont_Execute_This.sh dynamic_lib dylib
./Dont_Execute_This.sh dynamic_lib framework

./Dont_Execute_This.sh static_lib a
./Dont_Execute_This.sh static_lib framework

rm -rf pack
mkdir pack
pushd "pack" >/dev/null

cmake ../../../../pack -DTARGET_PLATFORM:STRING=mac -DPACKAGE_NAME:STRING=bqlog-lib
cmake --build . --target package
popd >/dev/null
