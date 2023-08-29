## Update Pony musl Docker images to Alpine 3.18

We've updated our `ponylang/ponyc:latest-alpine`, `ponylang/ponyc:release-alpine`, and `ponylang/ponyc:x.y.z-alpine` images to be based on Alpine 3.18. Previously, we were using Alpine 3.16 as the base.

## Drop support for Alpine versions prior to 3.17

We've dropped support for Alpine versions prior to 3.17. The ponyc compiler that we supply will not work on earlier Alpine versions. If you need to use on 3.16 or earlier, you'll need to patch the compiler and build from source.

The change is relatively straightforward and can be reverse engineered from the [pull request that dropped 3.16 support](https://github.com/ponylang/ponyc/pull/4407).

## FreeBSD is no longer a supported platform

We no longer have access to hardware that we can test FreeBSD on as part of our regular Continous Integration and Release cycles. As such, we are removing FreeBSD as a supported platform. We will make best effort attempts to not break FreeBSD, however, it is likely to fall behind and need maintenance from interested community members.

## Add macOS on Intel support

We've re-added macOS on Intel as a fully supported platform. Nightly and release builds will are available via `ponyup`.

We plan on maintaining macOS on Intel support for as long as we are doing our builds on GitHub Actions and GitHub provides macOS Intel runners.

## Drop macOS on Apple Silicon as a fully supported platform

We've dropped macOS on Apple Silicon as a fully supported platform.

At the moment, we don't have access to a CI environment with macOS on Apple Silicon. Until we have one, we can no longer maintain macOS as a fully supported platform.

We expect that we can start offering full macOS support on Apple Silicon in Q4 of 2023. This assumes that GitHub rolls out their delayed macOS on Apple Silicon hosted CI runners in Q4.

As of Pony 0.56.0, we are no longer providing prebuilt Apple Silicon builds of ponyc. You can [still build from source](https://github.com/ponylang/ponyc/blob/main/BUILD.md#macos). And, existing prebuilt release and nightly macOS binaries will remain available via `ponyup` and Cloudsmith.

Our plan is to maintain `corral` and `ponyup` on Apple Silicon as we don't expect any breaking changes to `ponyc` before GitHub offers Apple Silicon runners.

We will be doing our best to not break macOS on Apple Silicon during the transition. Hopefully, our macOS on Intel and Linux on aarch64 CI coverage will keep any accidental breakage from impacting on macOS on Apple Silicon. We can not guarantee that, so, community support in the form of PRs to keep macOS on Apple Silicon working are greatly appreciated.

Due to a logical flaw in the `pony_check` package regarding how the `_PropertyRunner` handles `expect_action(...)` and `complete_action(...)`/`fail_action(...)` calls, it was possible for lingering executions of previous runs to call into the `_PropertyRunner` instance that was already handling another run, completing or failing some actions of the current run. This has resulted in occasional hangs in CI.

With this change the hangs could not be reproduces locally or in CI.

