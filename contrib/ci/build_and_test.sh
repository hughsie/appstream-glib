#!/bin/sh
set -e

mkdir -p build && cd build
rm * -rf
meson .. \
    -Dgtk-doc=true \
    -Dstemmer=true $@
ninja -v
ninja test -v
DESTDIR=/tmp/install-ninja ninja install
cd ..
