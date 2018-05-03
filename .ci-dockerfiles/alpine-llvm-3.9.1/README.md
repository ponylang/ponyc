# Build image

```bash
docker build -t ponylang/ponyc-ci:alpine-llvm-3.9.1 .
```

# Run image to test

Use the [CircleCI CLI](https://circleci.com/docs/2.0/local-cli/) to run the CI jobs using this image
from the ponyc project root:

```bash
circleci build --job alpine-llvm-391-debug
circleci build --job alpine-llvm-391-release
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:alpine-llvm-3.9.1
```
