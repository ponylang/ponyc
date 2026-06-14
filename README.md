# Pony

Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language.

## Status

Pony is still pre-1.0 and as such, semi-regularly introduces breaking changes. These changes are usually fairly easy to adapt to. Applications written in Pony are currently used in production environments.

## Supported platforms

### Operating Systems

* Linux
* macOS
* Windows 11

### CPUs

* Full support for 64-bit platforms
  * x86, ARM and RISC-V CPUs only

## Additionally tested platforms

These platforms are supported. We run CI on a regular schedule rather than on every pull request, putting less effort into CI coverage than for the platforms above.

* DragonFly BSD (x86-64)
* FreeBSD (x86-64)
* Linux arm (32-bit)
* Linux armhf (32-bit)
* OpenBSD (x86-64)

## Best effort platforms

Best-effort platforms are operating systems where Pony _should_ work, but with low confidence. We don't have automated testing or continuous integration for these platforms to ensure they remain functional. We won't intentionally break a best-effort platform. However, we make no effort to maintain compatibility with them. When building ponyc from source on a best-effort platform, you may encounter failures. We welcome thoughtful pull requests to restore Pony compatibility with the platform.

* Windows 10 (x86-64 only)

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
