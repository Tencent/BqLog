#!/usr/bin/env bash
# Downloads the Log4j2 benchmark dependencies from Maven Central into lib/ .
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$DIR/lib"
for url in \
  https://repo1.maven.org/maven2/org/apache/logging/log4j/log4j-api/2.23.1/log4j-api-2.23.1.jar \
  https://repo1.maven.org/maven2/org/apache/logging/log4j/log4j-core/2.23.1/log4j-core-2.23.1.jar \
  https://repo1.maven.org/maven2/com/lmax/disruptor/3.4.2/disruptor-3.4.2.jar
do
  name=$(basename "$url")
  if [ ! -f "$DIR/lib/$name" ]; then
    echo "downloading $name"
    curl -fSL "$url" -o "$DIR/lib/$name"
  fi
done
echo done
