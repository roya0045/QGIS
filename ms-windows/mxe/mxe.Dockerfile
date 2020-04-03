FROM buildpack-deps:stretch

RUN chown root:root /tmp && chmod ugo+rwXt /tmp
RUN apt-get update
RUN apt-get install -y --no-install-recommends autopoint bison flex gperf libtool ruby scons unzip p7zip-full intltool libtool libtool-bin nsis lzip zip libprotobuf protobuf-dev protobuf-compiler

WORKDIR /mxe

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

