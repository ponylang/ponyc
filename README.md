[![GitHub license](https://img.shields.io/github/license/ponylang/ponyc?color=2f4f4f)](https://github.com/ponylang/ponyc/blob/master/LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/ponylang/ponyc)](https://github.com/ponylang/ponyc/releases)
[![GitHub contributors](https://img.shields.io/github/contributors/ponylang/ponyc)](https://github.com/ponylang/ponyc/graphs/contributors)
[![PR's Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)](https://github.com/ponylang/ponyc/pull/new)

[![GitHub issues open](https://img.shields.io/github/issues/ponylang/ponyc)](https://github.com/ponylang/ponyc/issues?q=is%3Aissue+label%3A%22help+wanted%22+is%3Aopen)
[![GitHub issues help wanted](https://img.shields.io/github/labels/ponylang/ponyc/help%20wanted)](https://github.com/ponylang/ponyc/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22)
[![GitHub issues triggers release](https://img.shields.io/github/issues/ponylang/ponyc/triggers%20release?color=008b8b)](https://github.com/ponylang/ponyc/issues?q=is%3Aissue+is%3Aopen+label%3A%22triggers+release%22)
[![GitHub issues good first](https://img.shields.io/github/issues/ponylang/ponyc/good%20first%20issue?color=228b22)](https://github.com/ponylang/ponyc/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)

# Pony

Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language.

## Status

Pony is still pre-1.0 and as such, semi-regularly introduces breaking changes. These changes are usually fairly easy to adapt to. Applications written in Pony are currently used in production environments.

## Installation

See [INSTALL.md](INSTALL.md).

## Building from source

See [BUILD.md](BUILD.md).

## Docker images

See [INSTALL_DOCKER.md](INSTALL_DOCKER.md)

## Resources

- [Learn more about Pony](https://www.ponylang.io/discover/)
- [Start learning Pony](https://www.ponylang.io/learn/)
  - [Getting help](https://www.ponylang.io/learn/#getting-help)
- [Try Pony online](https://playground.ponylang.io)
- [Frequently Asked Questions](https://www.ponylang.io/faq/)
- [Community](https://www.ponylang.io/community/)

## Supported platforms

### Operating Systems

- FreeBSD
- Linux
- macOS
- Windows 10

### CPUs

- Full support for 64-bit platforms
  - x86 and ARM CPUs only
- Partial support for 32-bit platforms
  - The `arm` and `armhf` architectures are tested via CI (Continuous
    Integration testing)

## Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)
* Atom: [language-pony](https://atom.io/packages/language-pony)
* Visual Studio: [VS-pony](https://github.com/ponylang/VS-pony)
* Visual Studio Code: [vscode-pony](https://marketplace.visualstudio.com/items?itemName=npruehs.pony)
* Vim:
    - [vim-pony](https://github.com/jakwings/vim-pony)
    - [pony.vim](https://github.com/dleonard0/pony-vim-syntax)
    - [currycomb: Syntastic support](https://github.com/killerswan/pony-currycomb.vim)
    - [SpaceVim](http://spacevim.org), available as layer for Vim and [Neovim](https://neovim.io). Just follow [installation instructions](https://github.com/SpaceVim/SpaceVim) then load `lang#pony` layer inside configuration file (*$HOME/.SpaceVim.d/init.toml*)
* Emacs:
    - [ponylang-mode](https://github.com/ponylang/ponylang-mode)
    - [flycheck-pony](https://github.com/ponylang/flycheck-pony)
    - [pony-snippets](https://github.com/ponylang/pony-snippets)
* BBEdit: [bbedit-pony](https://github.com/TheMue/bbedit-pony)
* Micro: [micro-pony-plugin](https://github.com/Theodus/micro-pony-plugin)
* Nano: [pony.nanorc file](https://github.com/serialhex/nano-highlight/blob/master/pony.nanorc)
* Kate: update syntax definition file: Settings -> Configure Kate -> Open/Save -> Modes & Filetypes -> Download Highlighting Files
* CudaText: lexer in Addon Manager

## Contributing

We welcome contributions to Pony. Please read through [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to get started.

## License

Pony is distributed under the terms of the [2-Clause BSD License](https://opensource.org/licenses/BSD-2-Clause). See [LICENSE](LICENSE) for details.
