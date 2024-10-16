# Pony

Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language.

test test

t

## Status

Pony is still pre-1.0 and as such, semi-regularly introduces breaking changes. These changes are usually fairly easy to adapt to. Applications written in Pony are currently used in production environments.

## Supported platforms

### Operating Systems

* Linux
* macOS
* Windows 10

### CPUs

* Full support for 64-bit platforms
  * x86, ARM and RISC-V CPUs only
* Partial support for 32-bit platforms
  * The `arm` and `armhf` architectures are tested via CI (Continuous
    Integration testing)

## Best effort platforms

Best effort platforms mean that there is support for the platform in the codebase but, we don't have any testing for the platform. We won't intentionally break a best-effort platform or remove support for it from the codebase, at the same time, we do make no effort to maintain it. When you go build a "best effort platform" from source, you might find it doesn't build. We welcome thoughtful pull requests to bring the platform up-to-date.

* DragonFlyBSD (x86 only)
* FreeBSD (x86 only)

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
