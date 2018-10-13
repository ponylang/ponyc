FROM ponylang/ponyc-ci:llvm-3.9.1

USER root

# Get FreeBSD libs/headers, extract and fix broken links
RUN cd /tmp && wget http://ftp.plusline.de/FreeBSD/releases/amd64/11.1-RELEASE/base.txz \
    && mkdir -p /opt/cross-freebsd-11 \
    && cd /opt/cross-freebsd-11 \
    && tar -xf /tmp/base.txz ./lib/ ./usr/lib/ ./usr/include/ \
    && cd /opt/cross-freebsd-11/usr/lib \
    && find . -xtype l|xargs ls -l|grep ' /lib/' \
        | awk '{print "ln -sf /opt/cross-freebsd-11"$11 " " $9}' \
        | /bin/sh \
    && rm -f /tmp/base.txz \
    && cd /opt/cross-freebsd-11 \
    && wget https://pkg.freebsd.org/FreeBSD:11:amd64/release_1/All/llvm39-3.9.1_6.txz \
    && wget https://pkg.freebsd.org/FreeBSD:11:amd64/release_1/All/libressl-devel-2.5.4.txz \
    && wget https://pkg.freebsd.org/FreeBSD:11:amd64/release_1/All/pcre2-10.21_1.txz \
    && wget https://pkg.freebsd.org/FreeBSD:11:amd64/release_1/All/libunwind-20170113_1.txz \
    && tar -xf libunwind-20170113_1.txz \
    && tar -xf pcre2-10.21_1.txz \
    && tar -xf libressl-devel-2.5.4.txz \
    && tar -xf llvm39-3.9.1_6.txz \
    && rm -f *.txz

USER pony
WORKDIR /home/pony
