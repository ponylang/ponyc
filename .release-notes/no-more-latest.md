## Stop Creating "alpine" Docker Image

We previously were creating Docker images with a musl C standard library Ponyc under the tag `alpine`. We are no longer creating those images. You should switch to our new equivalent images tagged `nightly`.
