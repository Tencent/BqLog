set -euo pipefail

rm -rf "../../../artifacts"
rm -rf "../../../install"

mkdir -p makeProj
cd makeProj

cmake ../../../../wrapper/typescript -G "Unix Makefiles"
cmake --build . --target install

cd ..

rm -rf pack
mkdir -p pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=all -DPACKAGE_NAME:STRING=bqlog-nodejs-npm
cmake --build . --target package
cd ..