set -euo pipefail
mkdir -p EclipseProj
cd EclipseProj

cmake ../../../../wrapper/java -G "Unix Makefiles"
cmake --build . --target install

cd ..

rm -rf pack
mkdir pack
cd pack

cmake ../../../../pack -DTARGET_PLATFORM:STRING=all -DPACKAGE_NAME:STRING=bqlog-java-wrapper
cmake --build . --target package
cd ..