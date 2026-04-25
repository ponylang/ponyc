## Build pony-lint-ci once across the three lint-pony-* CI targets

The `lint-pony-lint`, `lint-pony-doc`, and `lint-pony-lsp` Makefile targets each rebuilt the `pony-lint-ci` binary from Pony source before running it against their respective directories. The Pony compilation took roughly 60 seconds, so the redundant work added about two minutes to every PR's lint job.

`pony-lint-ci` is now a real file target with the lint tool source as its only regular prereq (`tools/pony-lint/*.pony` plus the pony_compiler library). The three lint targets depend on the file rather than each producing it, so the second and third invocations in CI skip the rebuild and reuse the binary from the first. The order-only prereq on `all` keeps the existing requirement that `ponyc` itself must be built first, without forcing a rebuild every time `all` is touched.

The change affects only the Linux PR-tools job; macOS and Windows do not run lint steps in CI.
