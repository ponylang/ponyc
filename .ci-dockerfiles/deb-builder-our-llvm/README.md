# Build image for different distros

```bash
# Ubuntu Xenial
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=xenial -t ponylang/ponyc-ci:xenial-deb-builder-our-llvm .
```

```bash
# Ubuntu Bionic
docker build --build-arg FROM_DISTRO=ubuntu --build-arg FROM_VERSION=bionic -t ponylang/ponyc-ci:bionic-deb-builder-our-llvm .
```

```bash
# Debian Stretch
docker build --build-arg FROM_DISTRO=debian --build-arg FROM_VERSION=stretch -t ponylang/ponyc-ci:stretch-deb-builder-our-llvm .
```

```bash
# Debian Buster
docker build --build-arg FROM_DISTRO=debian --build-arg FROM_VERSION=buster -t ponylang/ponyc-ci:buster-deb-builder-our-llvm .
```

# Run image to test

Will get you a bash shell in the image to try cloning Pony into where you can test a build to make sure everything will work before pushing:

```bash
docker run --name ponyc-ci-${DIST}-deb-builder-our-llvm --user pony --rm -i -t ponylang/ponyc-ci:${DIST}-deb-builder-our-llvm bash
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:${DIST}-deb-builder-our-llvm
```
