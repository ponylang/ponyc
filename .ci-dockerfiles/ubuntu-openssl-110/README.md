# Build image

```bash
docker build -t ponylang/ponyc-ci:ubuntu-openssl-110 .
```

# Run image to test

Will get you a bash shell in the image to try cloning Pony into where you can test a build to make sure everything will work before pushing:

```bash
docker run --name ponyc-ci-ubuntu-openssl-110 --user pony --rm -i -t ponylang/ponyc-ci:ubuntu-openssl-110 bash
```

# Run CircleCI jobs locally

Use the [CircleCI CLI](https://circleci.com/docs/2.0/local-cli/) to run the CI job using this image
from the ponyc project root:

```bash
circleci build --job ubuntu-openssl-110-debug
circleci build --job ubuntu-openssl-110-release
```
Note: when building you might want to set CPUs environment
variable to speed up the build:
```bash
circleci build -e CPUs=10 --job ubuntu-openssl-110-debug
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:ubuntu-openssl-110
```
