#!/bin/sh
set -e

mkdir -p build && cd build
rm * -rf
meson .. \
    -Denable-gtk-doc=true \
    -Denable-stemmer=true $@
ninja -v
ninja test -v
DESTDIR=/tmp/install-ninja ninja install
cd ..
