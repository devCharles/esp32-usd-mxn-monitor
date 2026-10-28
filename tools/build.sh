#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tools/configure.py
arduino-cli --config-file arduino-cli.yaml compile --fqbn 'esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB' --build-path build/firmware UsdMxnMonitor
