#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
c++ -std=c++17 -Wall -Wextra -IUsdMxnMonitor -I.arduino/user/libraries/ArduinoJson/src tests/test_monitor.cpp -o build/test_monitor
build/test_monitor tests/fixtures/yahoo_5d.json
c++ -std=c++17 -Wall -Wextra -IUsdMxnMonitor tests/test_rotation.cpp -o build/test_rotation
build/test_rotation
c++ -std=c++17 -Wall -Wextra -IUsdMxnMonitor -I.arduino/user/libraries/ArduinoJson/src tools/preview.cpp -o build/preview
for state in preview busy offline empty; do
  build/preview tests/fixtures/yahoo_5d.json "build/$state.ppm" "$state"
done
build/preview tests/fixtures/yahoo_5d.json build/landscape.ppm preview --landscape
