#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "$#" -ne 1 ]; then
  echo 'Uso: sh tools/upload.sh /dev/cu.usbmodem101'
  exit 1
fi
sh tools/build.sh
arduino-cli --config-file arduino-cli.yaml upload --port "$1" --fqbn 'esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB' --input-dir build/firmware UsdMxnMonitor
