# Overview

This docker image is used as a base for most of the other images for testing different llvm versions in CI.

# Build image

```bash
docker build -t ponylang/ponyc-ci:ubuntu-14.04-base .
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:ubuntu-14.04-base
```
