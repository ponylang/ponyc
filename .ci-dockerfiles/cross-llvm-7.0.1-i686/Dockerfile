FROM ponylang/ponyc-ci:llvm-7.0.1

ARG CROSS_TRIPLE=i686-unknown-linux-gnu
ARG CROSS_CC=gcc-6
ARG CROSS_CXX=g++-6
ARG CROSS_CFLAGS=-m32
ARG CROSS_CXXFLAGS=-m32
ARG CROSS_LDFLAGS=-m32
ARG CROSS_BITS=32
ARG CROSS_LINKER='gcc-6 -m32'
ARG CROSS_TUNE=generic

USER root

RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y \
  libc6:i386 \
  libc6-dev:i386 \
  linux-libc-dev:i386 \
  zlib1g:i386 \
  zlib1g-dev:i386 \
  g++-6 \
  g++-6-multilib \
  gcc-6-multilib \
# install pcre2
 && wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2 \
 && tar xvf pcre2-10.21.tar.bz2 \
 && cd pcre2-10.21 \
 && ./configure --prefix=/usr/cross --host="${CROSS_TRIPLE}" CC="${CROSS_CC}" CXX="${CROSS_CXX}" CFLAGS="${CROSS_CFLAGS}" LDFLAGS="${CROSS_LDFLAGS}" \
 && make \
 && make install \
 && cd .. \
 && rm -rf pcre2-10.21* \
# install libressl
 && wget "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.4.5.tar.gz" \
 && tar -xzvf libressl-2.4.5.tar.gz \
 && cd libressl-2.4.5 \
 && ./configure --prefix=/usr/cross --disable-asm --host="${CROSS_TRIPLE}" CC="${CROSS_CC}" CXX="${CROSS_CXX}" CFLAGS="${CROSS_CFLAGS}" LDFLAGS="${CROSS_LDFLAGS}" \
 && make \
 && make install \
 && cd .. \
 && rm -rf libressl-2.4.5* \
# cleanup
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean

USER pony
WORKDIR /home/pony
