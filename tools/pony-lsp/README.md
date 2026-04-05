# pony-lsp - the Pony Language Server

`pony-lsp` implements the [Language Server Protocol] for Pony. It communicates with editors via stdout/stdin, and is built and distributed alongside `ponyc` via `ponyup`.

For user documentation — installation and editor configuration — see the [pony-lsp documentation] on the Ponylang website.

[Language Server Protocol]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/
[pony-lsp documentation]: https://www.ponylang.io/use/pony-lsp/

## Feature Support

| **Feature** | **Description** |
| ------- | ----------- |
| **Diagnostics** (push, pull, refresh and change notifications) | Ponyc errors and related information is reported as LSP diagnostics. |
| **Hover** | Additional information is shown for: entities, methods, fields, local variables and references. |
| **Go To Definition** | For most language constructs, you can go from a reference to its definition. |
| **Document Symbols** | pony-lsp provides a list of available symbols for each opened document. |
| **Document Highlight** | All occurrences of the symbol under the cursor are highlighted simultaneously across the document. |

New features are actively being added. Contributions are welcome — we are happy to provide help and guidance.

## Settings

`pony-lsp` supports settings via the [`workspace/configuration`] request and [`workspace/didChangeConfiguration`] notification.

[`workspace/configuration`]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#workspace_configuration
[`workspace/didChangeConfiguration`]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#workspace_didChangeConfiguration

| **Setting** | **Type** | **Example** | **Description** |
| ------- | ---- | ------- | ----------- |
| `defines` | `Array[String]` | `["FOO", "BAR"]` | Defines active during compilation, equivalent to the `-D` flag in `ponyc` |
| `ponypath` | `Array[String]` | `["/path/to/pony/package"]` | Additional package search paths |

Example settings in JSON:

```json
{
  "defines": ["FOO", "BAR"],
  "ponypath": ["/path/to/pony/package", "/another/path"]
}
```

## Building from source

First, follow the [ponyc build documentation] and make sure you have everything to build `ponyc`. Then building `pony-lsp` is as easy as:

```bash
make pony-lsp
```

[ponyc build documentation]: ../../BUILD.md

## Reporting Issues

If `pony-lsp` is not working as expected, please [open an issue]. Include a minimal Pony program that reproduces the problem, the `pony-lsp` version, and the LSP client/editor you are using.

[open an issue]: https://github.com/ponylang/ponyc/issues/new
