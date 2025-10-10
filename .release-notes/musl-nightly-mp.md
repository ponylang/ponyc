## Add Multiplatform Nightly Docker Images

We've added new nightly images with support multiple architectures. The images support both arm64 and amd64. The images will be available with the tag `nightly`. They will be available for GitHub Container Registry the same as our [other ponyc images](https://github.com/ponylang/ponyc/pkgs/container/ponyc).

The images are based on Alpine 3.21. Every few months, we will be updating the base image to a newer version of Alpine Linux. At the moment, we generally add support for new Alpine version about 2 times a year.

We will be dropping the `alpine` tag in favor of the new `nightly` tag. However, that isn't happening now but will in the not so distant future. You should switch any usage you have of those tags to `nightly` now to avoid disruption later.
