mkdir -p EclipseProj
cd EclipseProj

cmake ../../../../wrapper/java -G "Unix Makefiles"
make

cd ..
