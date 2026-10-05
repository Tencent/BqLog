#!/usr/bin/env bash
# Downloads the Log4j2 benchmark dependencies from Maven Central into lib/ .
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$DIR/lib"
for url in \
  https://repo1.maven.org/maven2/org/apache/logging/log4j/log4j-api/2.26.0/log4j-api-2.26.0.jar \
  https://repo1.maven.org/maven2/org/apache/logging/log4j/log4j-core/2.26.0/log4j-core-2.26.0.jar \
  https://repo1.maven.org/maven2/com/lmax/disruptor/4.0.0/disruptor-4.0.0.jar
do
  name=$(basename "$url")
  if [ ! -f "$DIR/lib/$name" ]; then
    echo "downloading $name"
    curl -fSL "$url" -o "$DIR/lib/$name"
  fi
done
echo done
