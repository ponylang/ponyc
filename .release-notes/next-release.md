## Stop building versioned "-alpine" images

We used to release images for ponyc like `ponyc:0.38.0-alpine` that were based on Alpine Linux.However, we now just have those as `ponyc:0.38.0` as we don't have any glibc based images anymore.

## Stop Creating Generic musl Builds

We previously created releases for a generic "musl" build of ponyc. This was done to provide a build that would run on any Linux distribution that used musl as its C standard library. However, these could easily break if the OS they were built on changed.

We now provide specific Alpine version builds instead.

## Stop Creating "alpine" Docker Image

We previously were creating Docker images with a musl C standard library Ponyc under the tag `alpine`. We are no longer creating those images. You should switch to our new equivalent images tagged `nightly`.

## Add Multiplatform Versioned Release Docker Images

We've added multiplatform versioned release Docker images for Ponyc. These images are built for both `amd64` and `arm64` architectures and are tagged with the current Ponyc version. For example, `ponyc:0.60.3`.

Previously, the versioned release images were only available for the `amd64` architecture.

`nightly` and `release` tags were already avaialble as multiplatform images. This update extends that support to versioned images as well. With this change, we now provide consistent multiplatform support across all our Docker image tags.

