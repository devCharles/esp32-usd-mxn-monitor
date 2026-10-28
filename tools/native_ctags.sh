#!/bin/sh
# Arduino's bundled ctags 5.8 is Intel-only on macOS; build the same release for ARM.
set -eu
cd "$(dirname "$0")/.."
target=.arduino/data/packages/builtin/tools/ctags/5.8-arduino11/ctags
if "$target" --version >/dev/null 2>&1; then exit 0; fi
mkdir -p build/ctags-source
curl -fsSL https://codeload.github.com/arduino/ctags/tar.gz/refs/tags/5.8-arduino11 -o build/ctags.tar.gz
echo '86ca843c62aecb9bc531a73d9b6cef33179bd885b1bf8df9e0c7e9761aa9794d  build/ctags.tar.gz' | shasum -a 256 -c -
tar -xzf build/ctags.tar.gz -C build/ctags-source --strip-components=1
python3 - <<'PY'
from pathlib import Path
# The old macro collides with current macOS SDK headers; rename only its identifier.
for p in Path('build/ctags-source').glob('*'):
    if p.suffix in ('.h','.c'):
        text=p.read_text(errors='surrogateescape')
        p.write_text(text.replace('__unused__','CTAGS_UNUSED'),errors='surrogateescape')
PY
(cd build/ctags-source && ./configure && make -j4)
cp build/ctags-source/ctags "$target"
"$target" --version
