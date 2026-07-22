# Pony

Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language.

## Status

Pony is still pre-1.0 and as such, semi-regularly introduces breaking changes. These changes are usually fairly easy to adapt to. Applications written in Pony are currently used in production environments.

## Supported platforms

<!-- Keep this matrix in sync with the copy in the ponylang-website repo: docs/learn/installing-pony.md -->

| OS \ CPU | `amd64` | `arm64` | `arm32` | `riscv64` |
|---|---|---|---|---|
| **Linux** | ✅ Released | ✅ Released | 🔧 Best-effort | 🧪 Tested |
| **macOS** | ✅ Released | ✅ Released | — | — |
| **Windows** | ✅ Released | ✅ Released | — | — |
| **FreeBSD** | 🧪 Tested | ✗ Unsupported | — | — |
| **OpenBSD** | 🧪 Tested | ✗ Unsupported | — | — |
| **DragonFly BSD** | 🧪 Tested | — | — | — |

* ✅ **Released** — official prebuilt binaries; also tested in CI
* 🧪 **Tested** — tested in CI; build from source (no prebuilt binary)
* 🔧 **Best-effort** — not in CI; built from source and tested periodically on real hardware
* ✗ **Unsupported** — no support
* — **Not applicable** — combination doesn't exist

On Windows, the minimum supported version is **Windows 11 / Windows Server 2022**
(build 20348). Pony's networking uses an OS readiness API introduced in that
build; earlier versions, including Windows 10, are unsupported and a Pony binary
will not run on them.

On Linux, the minimum supported version is **kernel 5.3**. Pony's process
support detects a child's exit with `pidfd_open`, a system call added in that
release; on an older kernel, starting a process returns an error. Earlier
kernels are unsupported.

## More Information

* [Installation](INSTALL.md)
* [Building from source](BUILD.md)
* [Docker images](INSTALL_DOCKER.md)
* [Editor support](EDITORS.md)

## Resources

* [Learn more about Pony](https://www.ponylang.io/discover/)
* [Start learning Pony](https://www.ponylang.io/learn/)
  * [Getting help](https://www.ponylang.io/learn/#getting-help)
* [Try Pony online](https://playground.ponylang.io)
* [Frequently Asked Questions](https://www.ponylang.io/faq/)
* [Community](https://www.ponylang.io/community/)

## Contributing

We welcome contributions to Pony. Please read through [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to get started.

## License

Pony is distributed under the terms of the [2-Clause BSD License](https://opensource.org/licenses/BSD-2-Clause). See [LICENSE](LICENSE) for details.
