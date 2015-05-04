FROM debian:sid
MAINTAINER Arnau Siches <asiches@gmail.com>

RUN apt-get update -qqy \
  && apt-get install -qqy \
    build-essential \
    git \
    libncurses-dev \
    libnuma-dev \
    llvm-3.6-dev \
    zlib1g-dev \
  && rm -rf /var/lib/apt/lists/*

COPY . /usr/local/ponyc
WORKDIR /usr/local/ponyc

RUN make config=release
ENV PATH=$PATH:/usr/local/ponyc/build/release

CMD ["make", "test"]
