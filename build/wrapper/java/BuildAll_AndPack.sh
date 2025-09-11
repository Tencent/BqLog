set -euo pipefail
mkdir -p EclipseProj
cd EclipseProj

cmake ../../../../wrapper/java -G "Unix Makefiles"
cmake --build . --target install

cd ..
