#!/bin/bash

mkdir /usr/src/qgis/build
cd /usr/src/qgis/build || exit -1

ccache -s

ln -s /../../bin/ccache /usr/lib64/ccache/clang
ln -s /../../bin/ccache /usr/lib64/ccache/clang++

cmake -GNinja \
 -DWITH_QUICK=OFF \
 -DWITH_3D=ON \
 -DWITH_STAGED_PLUGINS=ON \
 -DWITH_GRASS=OFF \
 -DSUPPRESS_QT_WARNINGS=ON \
 -DENABLE_MODELTEST=ON \
 -DENABLE_PGTEST=ON \
 -DENABLE_SAGA_TESTS=ON \
 -DENABLE_MSSQLTEST=ON \
 -DWITH_QSPATIALITE=OFF \
 -DWITH_QWTPOLAR=OFF \
 -DWITH_APIDOC=OFF \
 -DWITH_ASTYLE=OFF \
 -DWITH_DESKTOP=ON \
 -DWITH_BINDINGS=ON \
 -DWITH_SERVER=ON \
 -DWITH_ORACLE=OFF \
 -DDISABLE_DEPRECATED=ON \
 -DCXX_EXTRA_FLAGS="${CLANG_WARNINGS}" \
 -DCMAKE_C_COMPILER=/usr/lib64/ccache/clang \
 -DCMAKE_CXX_COMPILER=/usr/lib64/ccache/clang++ \
 -DADD_CLAZY_CHECKS=ON \
 -DWERROR=TRUE \
 ..

ninja

ccache -s
