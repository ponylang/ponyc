## Stop building versioned "-alpine" images

We used to release images for ponyc like `ponyc:0.38.0-alpine` that were based on Alpine Linux.However, we now just have those as `ponyc:0.38.0` as we don't have any glibc based images anymore.

