FROM dipinhora/ponyc-runner:0.0.1

COPY . /build

WORKDIR /build

ARG PONYC_CONFIG
ENV PONYC_CONFIG ${PONYC_CONFIG:-debug}

RUN LLVM_CONFIG=true make config=${PONYC_CONFIG} install
RUN if [ -e /build/build/arm-libponyrt.a ]; then cp /build/build/arm-libponyrt.a /usr/local/lib/pony/lib/libponyrt.a; fi

ENTRYPOINT ["ponyc"]

