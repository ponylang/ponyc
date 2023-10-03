## Fix broken DTrace support

Quite a while back, we broke the support in our Makefile for building the Pony runtime with support for DTrace. We've fixed that and added tests to assure it builds.

## Fix compilation error when building with pool_memalign in release mode

When attempting to build the Pony runtime with the `pool_memalign` option, users would encounter a compilation error if building a `release` rather than `debug` version of the runtime. We've fixed the compilation error and added CI testing to verify we don't get a regression.

