# pony-lsp - the Pony Language Server

This language server is communicating with LSP-clients (editors) via stdout/stdin.

## Feature Support

| **Feature** | **Description** |
| ------- | ----------- |
| **Diagnostics** (push, pull, refresh and change notifications) | Ponyc errors and related information is reported as LSP diagnostics. |
| **Hover** | Additional information is shown for: entities, methods, fields, local variables and references. |
| **Go To Definition** | For most language constructs, you can go from a reference to its definition. |
| **Document Symbols** | pony-lsp provides a list of available symbols for each opened document. |

We are constantly working on adding new features, but also welcome all contributions and offer help and guidance for implementing any feature.

## Settings

`pony-lsp` supports settings via `workspace/configuration` request and `workspace/didChangeConfiguration`.
It expects the following optional settings:

| **Name** | **Type** | **Example** | **Description** |
| ---- | ---- | ------- | ----------- |
| **defines** | `Array[String]` | `["FOO", "BAR"]` | Defines active during compilation. These are usually set when using `ponyc` using `-D` |
| **ponypath** | `Array[String]` | `["/path/to/pony/package", "/another/path"]` | An array of paths which are added to the package search paths of `pony-lsp` |

Example settings in JSON:

```json
{
  "defines": ["FOO", "BAR"],
  "ponypath": ["/path/to/pony/package", "/another/path"]
}
```

## Getting pony-lsp

`pony-lsp` is distributed via `ponyup` alongside ponyc, so installing a recent `ponyc` will also give you `pony-lsp`.

Checkout the [Ponyc installation instructions](/INSTALL.md) on how to install `ponyc` (and `pony-lsp` at the same time).

## Building from source

First checkout [the ponyc build documentation](/BUILD.md) and make sure you have
everything necessary available to build `ponyc`. Then building `pony-lsp` should be as simple as:

```bash
make pony-lsp
```

## Reporting Issues

The best support and feedback you can give us, is when the pony-lsp is not working as expected.
Please provide a minimal Pony program to reproduce this issue, document the `pony-lsp` version you are using and the lsp-client/editor you are using.
