## Drop Ubuntu 20.04 support

Ubuntu 20.04 has reached its end of life date. We've dropped it as a supported platform. That means, we no longer test against it when doing CI and we no longer create prebuilt binaries for installation via `ponyup` for Ubuntu 20.04.

We will maintain best effort to keep Ubuntu 20.04 continuing to work for anyone who wants to use it and builds `ponyc` from source.

## Update to LLVM 17.0.6

We've updated the LLVM version used to build Pony to 17.0.6.

