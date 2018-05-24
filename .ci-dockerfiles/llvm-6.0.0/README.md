# Build image

```bash
docker build -t ponylang/ponyc-ci:llvm-6.0.0 .
```

# Run image to test

Use the [CircleCI CLI](https://circleci.com/docs/2.0/local-cli/) to run the CI job using this image
from the ponyc project root:

```bash
circleci build --job llvm-600-debug
circleci build --job llvm-600-release
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:llvm-6.0.0
```
