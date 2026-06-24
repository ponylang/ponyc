# Pony Compiler (ponyc) Project Overview

Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language. This repository contains the source code for the Pony compiler (`ponyc`) and the Pony runtime (`libponyrt`).

## Project Architecture

The project is primarily written in **C11** and uses **LLVM** as its backend. The source code is organized as follows:

- **`src/libponyc/`**: Core compiler logic.
  - `ast/`: Abstract Syntax Tree implementation.
  - `type/`: Type system and type checking.
  - `codegen/`: LLVM code generation.
  - `pass/`: Compiler passes.
- **`src/libponyrt/`**: Pony runtime library.
  - `actor/`: Actor model implementation.
  - `gc/`: Garbage collection (ORCA).
  - `sched/`: Work-stealing scheduler.
  - `asio/`: Asynchronous I/O.
- **`src/ponyc/`**: CLI entry point for the compiler.
- **`packages/`**: The Pony standard library, written in Pony.
- **`lib/`**: Vendored dependencies (LLVM, blake2).
- **`test/`**: Comprehensive test suite including unit tests for the compiler and runtime, as well as full program integration tests.

## Building and Running

The project uses a **CMake**-based build system with a **Makefile** wrapper for convenience (and `make.ps1` for Windows).

### Prerequisites
- C11 compliant compiler (Clang >= 3.4, GCC >= 4.7, or MSVC >= 2017)
- CMake >= 3.21
- Python 3 (for LLVM build)
- zlib development headers

### Key Commands

- **Build vendored libraries (LLVM):**
  ```bash
  make libs
  ```
  *(Only needed once or when LLVM version changes)*

- **Configure build:**
  ```bash
  make configure                # Release build (default)
  make configure config=debug    # Debug build
  ```

- **Build ponyc:**
  ```bash
  make build
  ```
  *(Output goes to `build/release` or `build/debug`)*

- **Run tests:**
  ```bash
  make test
  ```

- **Install:**
  ```bash
  sudo make install
  ```

## Development Conventions

### Coding Style (from STYLE_GUIDE.md)
- **Indentation:** 2 spaces, no tabs.
- **Line Length:** Maximum 80 columns.
- **Naming:**
  - Types: `CamelCase` (e.g., `TCPConnection`).
  - Functions/Variables/Fields: `snake_case` (e.g., `send_data`).
- **File Naming:** Snake case version of the principal type (e.g., `contents_log.pony` for `ContentsLog`).

### Contribution Workflow
- **Features:** Major features require an **RFC** (Request for Comments).
- **Pull Requests:** PRs are squashed upon merge. Ensure the initial comment is a well-formatted commit message.
- **Changelog:** Behavioural changes should be noted. Labels on GitHub PRs automate `CHANGELOG.md` updates.

### Testing Practices
- **Compiler/Runtime tests:** Located in `test/libponyc` and `test/libponyrt`.
- **Integration tests:** Located in `test/full-program-tests`.
- **Standard Library tests:** Usually contained within the package directory or `builtin_test`.

## Key Files
- `README.md`: General project info.
- `BUILD.md`: Detailed build instructions for various platforms.
- `STYLE_GUIDE.md`: Comprehensive coding standards for Pony code.
- `CONTRIBUTING.md`: Workflow for contributing to the project.
- `Makefile`: Main entry point for build tasks on Unix.
- `CMakeLists.txt`: Root CMake configuration.
- `VERSION`: Current version of the compiler.
