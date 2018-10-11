ARG FROM_DISTRO=ubuntu
ARG FROM_VERSION=trusty
FROM ${FROM_DISTRO}:${FROM_VERSION}

ARG FROM_VERSION=trusty

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  ca-certificates \
  dirmngr \
  gnupg \
 && apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys "47E4F8DEE04F0923" \
 && echo deb https://dl.bintray.com/pony-language/ponylang-debian ${FROM_VERSION} main | tee -a /etc/apt/sources.list \
 && apt-get update \
 && apt-get install -y \
  devscripts \
  build-essential \
  lintian \
  debhelper \
  equivs

WORKDIR /home/pony

