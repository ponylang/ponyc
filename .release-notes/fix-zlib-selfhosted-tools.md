## Fix failure to build pony tools with Homebrew

When building ponyc from source with Homebrew, the self-hosted tools (pony-lint, pony-lsp, pony-doc) failed to link because the embedded LLD linker couldn't find zlib. Homebrew installs zlib outside the default system linker search paths, and while CMake resolved the correct location for linking ponyc itself, that path wasn't forwarded to the ponyc invocations that compile the tools.
