# Pony-AST :horse: :deciduous_tree:

Pony library wrapping `libponyc` via C-FFI, so Pony programs can finally compile themselves. :horse: :point_right: :horse:

## Current Status

This software is:

* Untested
* Dangerous
* Hacky
* hopelessly broken

It will:

* Leak memory
* Eat all the memory on your machine
* Segfault
* Steal all of your bitcoins

**Use only if you know what you are doing!**

## Requirements

In order to make the current compiler work, and have it pick up the Pony standard library one needs to point the `PONYPATH` environment variable to the pony stdlib. If you have a pony installation you compiled yourself, it will most likely live in: `ponyc-repository/packages`. If you have a pre-compiled installation obtained via ponyup it might live in something like: `$HOME/.local/share/ponyup/ponyc-release-<RELEASE-NUMBER>-<ARCH>-<SYSTEM>/packages`.

## Usage

The main entrypoint is the Compiler, that is running the pony compiler passes until (including) the `expr` pass and is emitting the produced `ast_t` structure in the `Program` class. It can be used to access packages, modules and entities, rerieve AST nodes at certain positions, go to definitions etc. etc.

```pony
use ast = "ast"

actor Main
  new create(env: Env) =>
    try
      let path = FilePath(FileAuth(env.root), env.args(env.args.size() - 1)?)
      match ast.Compiler.compile(env, path)
      | let program: ast.Program =>
        // do something with the parsed program AST
        env.out.print("OK")
      | let errors: Array[ast.Error] =>
        // do something with the compilation errors
        for e in errors.values() do
          env.err.print(e.msg)
        end
      end
    end
```
