## Add Multiplatform Versioned Release Docker Images

We've added multiplatform versioned release Docker images for Ponyc. These images are built for both `amd64` and `arm64` architectures and are tagged with the current Ponyc version. For example, `ponyc:0.60.3`.

Previously, the versioned release images were only available for the `amd64` architecture.

`nightly` and `release` tags were already avaialble as multiplatform images. This update extends that support to versioned images as well. With this change, we now provide consistent multiplatform support across all our Docker image tags.
