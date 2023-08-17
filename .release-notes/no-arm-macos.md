## Drop macOS on Apple Silicon as a fully supported platform

We've dropped macOS on Apple Silicon as a fully supported platform.

At the moment, we don't have access to a CI environment with macOS on Apple Silicon. Until we have one, we can no longer maintain macOS as a fully supported platform.

We expect that we can start offering full macOS support on Apple Silicon in Q4 of 2023. This assumes that GitHub rolls out their delayed macOS on Apple Silicon hosted CI runners in Q4.

As of Pony 0.56.0, we are no longer providing prebuilt Apple Silicon builds of ponyc. You can [still build from source](https://github.com/ponylang/ponyc/blob/main/BUILD.md#macos). And, existing prebuilt release and nightly macOS binaries will remain available via `ponyup` and Cloudsmith.

Our plan is to maintain `corral` and `ponyup` on Apple Silicon as we don't expect any breaking changes to `ponyc` before GitHub offers Apple Silicon runners.

We will be doing our best to not break macOS on Apple Silicon during the transition. Hopefully, our macOS on Intel and Linux on aarch64 CI coverage will keep any accidental breakage from impacting on macOS on Apple Silicon. We can not guarantee that, so, community support in the form of PRs to keep macOS on Apple Silicon working are greatly appreciated.
