# Overview

This docker image is used as a base for most of the other images for testing different llvm versions in CI.

# Build image

```bash
docker build --pull -t ponylang/ponyc-ci:ubuntu-16.04-base .
```

# Run image to test

Will get you a bash shell in the image to try cloning Pony into where you can test a build to make sure everything will work before pushing:

```bash
docker run --name ponyc-ci-ubuntu-16.04-base --user pony --rm -i -t ponylang/ponyc-ci:ubuntu-16.04-base bash
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:ubuntu-16.04-base
```
