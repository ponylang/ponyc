## Add Multiplatform Release Docker Images

We've added new release images with support multiple architectures. The images support both arm64 and amd64. The images will be available with the tag `release`. They will be available for GitHub Container Registry the same as our [other ponyc images](https://github.com/ponylang/ponyc/pkgs/container/ponyc).

The images are based on Alpine 3.21. Every few months, we will be updating the base image to a newer version of Alpine Linux. At the moment, we generally add support for new Alpine version about 2 times a year.

Note, at this time, the release images that are tagged with the release version like `0.60.2` are still only amd64. Multiplatform images tagged with the version will be coming soon.

