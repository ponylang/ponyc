## Add Fedora 41 as a supported platform

We've added Fedora 41 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates in November 2025. At that point, we'll stop building releases for it.

## Drop Fedora 39 support

Fedora 39 has reached its end of life date. We've dropped it as a supported platform. That means, we no longer create prebuilt binaries for installation via `ponyup` for Fedora 39.

We will maintain best effort to keep Fedora 39 continuing to work for anyone who wants to use it and builds `ponyc` from source.

## Update Pony musl Docker images to Alpine 3.20

We've updated our `ponylang/ponyc:latest-alpine`, `ponylang/ponyc:release-alpine`, and `ponylang/ponyc:x.y.z-alpine` images to be based on Alpine 3.20. Previously, we were using Alpine 3.18 as the base.
## Fix rare termination logic failures that could result in early shutdown

There was a very rare edge case in the termination logic that could result in early shutdown resulting in a segfault.

The edge cases have been addressed and the shutdown/termination logic has been overhauled to make it simpler and more robust.

