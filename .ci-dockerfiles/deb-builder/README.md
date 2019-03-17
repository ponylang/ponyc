# Build image for different distros

```bash
# Ubuntu Xenial
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=xenial -t ponylang/ponyc-ci:xenial-deb-builder .
```

```bash
# Ubuntu Bionic
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=bionic -t ponylang/ponyc-ci:bionic-deb-builder .
```

```bash
# Debian Stretch
docker build --build-arg FROM_DISTRO=debian --build-arg FROM_VERSION=stretch -t ponylang/ponyc-ci:stretch-deb-builder .
```

```bash
# Debian Buster
docker build --build-arg FROM_DISTRO=debian --build-arg FROM_VERSION=buster -t ponylang/ponyc-ci:buster-deb-builder .
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:${DIST}-deb-builder
```
