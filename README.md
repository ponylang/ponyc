# Pony

Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language.

## Resources

- [Learn more about Pony](https://www.ponylang.io/discover/)
- [Start learning Pony](https://www.ponylang.io/learn/)
- [Beginner Help](https://ponylang.zulipchat.com/#narrow/stream/189985-beginner-help)
- [Try Pony online](https://playground.ponylang.io)
- [Frequently Asked Questions](https://www.ponylang.io/faq/)
- [Community](https://www.ponylang.io/community/)
- [Install Pony](INSTALL.md)
- [Build Pony from source](BUILD.md)
- [Docker images](DOCKER.md)

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

# Editor support

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




## Building Pony on Non-x86 platforms

On ARM platforms, the default gcc architecture specification used in the Makefile of _native_ does not work correctly, and can even result in the gcc compiler crashing.  You will have to override the compiler architecture specification on the _make_ command line.  For example, on a RaspberryPi2 you would say:
```bash
make arch=armv7
```

To get a complete list of acceptable architecture names, use the gcc command:

```bash
gcc -march=none
```

This will result in an error message plus a listing of all architecture types acceptable on your platform.
