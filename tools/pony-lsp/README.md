# pony-lsp - the Pony Language Server

This language server

This language server is communicating with LSP-clients (editors) via stdout/stdin.

## Feature Support

| **Feature** | **Description** |
| ------- | ----------- |
| **Diagnostics** (push, pull, refresh and change notifications) | Ponyc errors and related information is reported as LSP diagnostics. |
| **Hover** | Additional information is shown for: entities, methods, fields, local variables and references. |
| **Go To Definition** | For most language constructs, you can go from a reference to its definition. |
| **Document Symbols** | pony-lsp provides a list of available symbols for each opened document. |

We are constantly working on adding new features, but also welcome all contributions and offer help and guidance for implementing any feature.

## Getting pony-lsp

`pony-lsp` is distributed via `ponyup` alongside ponyc, so installing a recent `ponyc` will also give you `pony-lsp`.

Checkout the [Ponyc installation instructions](/INSTALL.md) on how to install `ponyc` (and `pony-lsp` at the same time).

## Building from source

First checkout [the ponyc build documentation](/BUILD.md) and make sure you have
everything necessary available to build `ponyc`. Then building `pony-lsp` should be as simple as:

```
make pony-lsp
```

## Reporting Issues

The best support and feedback you can give us, is when the pony-lsp is not working as expected.
Please provide a minimal Pony program to reproduce this issue, document the `pony-lsp` version you are using and the lsp-client/editor you are using.
