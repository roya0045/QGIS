FROM buildpack-deps:stretch

RUN chown root:root /tmp && chmod ugo+rwXt /tmp
RUN apt-get update
RUN  make protobuf
RUN apt-get install -y --no-install-recommends autopoint bison flex gperf libtool ruby scons unzip p7zip-full intltool libtool libtool-bin nsis lzip zip
WORKDIR /mxe
RUN export PATH=/mxe/usr/bin:/mxe/usr/i686-w64-mingw32.static/bin:/mxe/usr/i686-w64-mingw32.static/qt5/bin:/mxe/usr/bin:/mxe/usr/x86_64-pc-linux-gnu/bin:$PATH
RUN export PKG_CONFIG=/mxe/usr/bin/i686-w64-mingw32.static-pkg-config

RUN git clone https://github.com/mxe/mxe . || git pull origin master
RUN make MXE_TARGETS=x86_64-w64-mingw32.shared.posix -j 16 \
    qca \
    qtlocation  \
    qscintilla2  \
    qwt  \
    gdal  \
    qtkeychain  \
    qtserialport  \
    qtwebkit \
    qtwinextras \
    libzip \
    gsl \
    libspatialindex \
    exiv2

RUN chmod -R a+rw /mxe/usr/x86_64-w64-mingw32.shared.posix

RUN echo "hdgdgdhgdghdhgdhdg"

