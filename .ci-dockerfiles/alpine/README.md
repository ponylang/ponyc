# Build image

```bash
docker build -t ponylang/ponyc-ci:alpine .
```

# Run image to test

Will get you a bash shell in the image to try cloning Pony into where you can test a build to make sure everything will work before pushing:

```bash
docker run --name ponyc-ci-alpine --user pony --rm -i -t ponylang/ponyc-ci:alpine /bin/sh
```

# Run CircleCI jobs locally

Use the [CircleCI CLI](https://circleci.com/docs/2.0/local-cli/) to run the CI jobs using this image
from the ponyc project root:

```bash
circleci build --job alpine-debug
circleci build --job alpine-release
```

# Push to dockerhub

You'll need credentials for the ponylang dockerhub account. Talk to @jemc or @seantallen for access

```bash
docker push ponylang/ponyc-ci:alpine
```
