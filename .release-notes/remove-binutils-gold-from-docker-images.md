## Remove binutils-gold from nightly and release Docker images

The `binutils-gold` package has been removed from the `ponylang/ponyc` nightly and release Docker images. Nothing in ponyc uses the gold linker, and gold is upstream-deprecated and being phased out of distro repositories.

If your build inside one of these images relies on `binutils-gold` being present, you will need to install it explicitly with `apk add binutils-gold`.
