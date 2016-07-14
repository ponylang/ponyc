FROM sendence/ponyc-runner:0.0.5

COPY . /build

WORKDIR /build

ARG PONYC_CONFIG
ENV PONYC_CONFIG ${PONYC_CONFIG:-debug}

RUN LLVM_CONFIG=true make config=${PONYC_CONFIG} install
RUN cp /build/build/arm-libponyrt.a /usr/arm-linux-gnueabihf/lib/libponyrt.a

WORKDIR /build/build/pony-stable

RUN make install

WORKDIR /tmp

RUN rm -rf /build

ADD armhf-*.tar.gz /usr/arm-linux-gnueabihf/
ADD amd64-*.tar.gz /usr/local/

ENTRYPOINT ["ponyc"]
