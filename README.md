[![Backers on Open Collective](https://opencollective.com/ponyc/backers/badge.svg)](#backers)
 [![Sponsors on Open Collective](https://opencollective.com/ponyc/sponsors/badge.svg)](#sponsors)

# Contributors

This project exists thanks to all the people who contribute. [[Contribute](CONTRIBUTING.md)].
<a href="https://github.com/ponylang/ponyc/graphs/contributors"><img src="https://opencollective.com/ponyc/contributors.svg?width=890&button=false" /></a>


# Backers

Thank you to all our backers! 🙏 [[Become a backer](https://opencollective.com/ponyc#backer)]

<a href="https://opencollective.com/ponyc#backers" target="_blank"><img src="https://opencollective.com/ponyc/backers.svg?width=890"></a>


# Sponsors

Support this project by becoming a sponsor. Your logo will show up here with a link to your website. [[Become a sponsor](https://opencollective.com/ponyc#sponsor)]

<a href="https://opencollective.com/ponyc/sponsor/0/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/0/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/1/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/1/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/2/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/2/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/3/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/3/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/4/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/4/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/5/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/5/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/6/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/6/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/7/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/7/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/8/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/8/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/9/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/9/avatar.svg"></a>

# Getting help

Need help? Not to worry, we have you covered.

We have a couple resources designed to help you learn, we suggest starting with the tutorial and from there, moving on to the Pony Patterns book. Additionally, standard library documentation is available online.

* [Tutorial](http://tutorial.ponylang.io).
* [Pony Patterns](http://patterns.ponylang.io) cookbook is in progress
* [Standard library docs](http://stdlib.ponylang.io/).
* [Build Problems, see FAQ Compiling](https://www.ponylang.io/faq/#compiling).

If you are looking for an answer "right now", we suggest you give our [Zulip](https://ponylang.zulipchat.com/#narrow/stream/189985-beginner-help) community a try. Whatever your question is, it isn't dumb, and we won't get annoyed.

Think you've found a bug? Check your understanding first by writing the mailing list. Once you know it's a bug, open an issue.
* [Open an issue](https://github.com/ponylang/ponyc/issues)

# Trying it online

If you want a quick way to test or run code, checkout the [Playground](https://playground.ponylang.io/).

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

# Installation

Pony supports LLVM 7.0.

Pony's prerequisites for CPU platforms are:

- Full support for 64-bit platforms
  - x86 and ARM CPUs only
  - See platforms listed in the Circle-CI build list at
    https://circleci.com/gh/ponylang/ponyc
- Partial support for 32-bit platforms
  - The `arm` and `armhf` architectures are tested via CI (Continuous
    Integration testing)
  - See platforms listed in the Circle-CI build list at
    https://circleci.com/gh/ponylang/ponyc
  - See also: GitHub issues
    [#2836](https://github.com/ponylang/ponyc/issues/2836)
    and
    [#1576](https://github.com/ponylang/ponyc/issues/1576)
    for more information.

## Using Docker

Want to use the latest revision of Pony source, but don't want to build from source yourself? You can run the `ponylang/ponyc` Docker container, which is created from an automated build at each commit to master.

You'll need to install Docker using [the instructions here](https://docs.docker.com/engine/installation/). Then you can pull the latest `ponylang/ponyc` image using this command:

```bash
docker pull ponylang/ponyc:latest
```

Then you'll be able to run `ponyc` to compile a Pony program in a given directory, running a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc
```

If you're unfamiliar with Docker, remember to ensure that whatever path you provide for `/path/to/my-code` is a full path name and not a relative path, and also note the lack of a closing slash, `/`, at the *end* of the path name.

Note that if your host doesn't match the docker container, you'll probably have to run the resulting program inside the docker container as well, using a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc ./main
```

If you're using `docker-machine` instead of native docker, make sure you aren't using [an incompatible version of Virtualbox](#virtualbox).

### Docker for Windows

Pull the latest image as above.

```bash
docker pull ponylang/ponyc:latest
```

Share a local drive (volume), such as `c:`, with Docker for Windows, so that they are available to your containers. (Refer to [shared drives](https://docs.docker.com/docker-for-windows/#shared-drives) in the Docker for Windows documentation for details.)

Then you'll be able to run `ponyc` to compile a Pony program in a given directory, running a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc
```

Note the inserted drive letter. Replace with your drive letter as appropriate.

To run a program, run a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc ./main
```

To compile and run in one step run a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc sh -c "ponyc && ./main"
```


## Building ponyc using LLVM sources:


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
