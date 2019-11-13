# Build image

```bash
docker build --pull -t ponylang/ponyc-ci:llvm-7.0.1 .
```

# Run image to test

Will get you a bash shell in the image to try cloning Pony into where you can test a build to make sure everything will work before pushing:

```bash
docker run --name ponyc-ci-llvm-701 --user pony --rm -i -t ponylang/ponyc-ci:llvm-7.0.1 bash
```

# Run CircleCI jobs locally

Use the [CircleCI CLI](https://circleci.com/docs/2.0/local-cli/) to run the CI job using this image
from the ponyc project root:

```bash
circleci build --job llvm-701-debug
circleci build --job llvm-701-release
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:llvm-7.0.1
```
