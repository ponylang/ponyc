FROM sendence/ponyc-runner:0.0.4

COPY . /build

WORKDIR /build

ARG PONYC_CONFIG
ENV PONYC_CONFIG ${PONYC_CONFIG:-debug}

RUN LLVM_CONFIG=true make config=${PONYC_CONFIG} install
RUN if [ -e /build/build/arm-libponyrt.a ]; then cp /build/build/arm-libponyrt.a /usr/local/lib/libponyrt.a; fi

WORKDIR /build/build/pony-stable

RUN make install

ENTRYPOINT ["ponyc"]
