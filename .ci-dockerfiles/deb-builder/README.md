# Build image for different distros

```bash
# Ubuntu Trusty
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=trusty -t ponylang/ponyc-ci:trusty-deb-builder .
```

```bash
# Ubuntu Xenial
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=xenial -t ponylang/ponyc-ci:xenial-deb-builder .
```

```bash
# Ubuntu Artful
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=artful -t ponylang/ponyc-ci:artful-deb-builder .
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

```bash
# Debian Jessie
docker build --build-arg FROM_DISTRO=debian --build-arg FROM_VERSION=jessie -t ponylang/ponyc-ci:jessie-deb-builder --file Dockerfile.jessie .
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:${DIST}-deb-builder
```
