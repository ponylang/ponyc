# pony-lsp - the Pony Language Server

`pony-lsp` implements the [Language Server Protocol] for Pony. It communicates with editors via stdout/stdin, and is built and distributed alongside `ponyc` via `ponyup`.

For user documentation — installation and editor configuration — see the [pony-lsp documentation] on the Ponylang website.

[Language Server Protocol]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/
[pony-lsp documentation]: https://www.ponylang.io/use/pony-lsp/

## Feature Support

| **Feature** | **Description** |
| ------- | ----------- |
| **[Diagnostics][spec-diagnostics]** (push, pull, refresh and change notifications) | Ponyc errors and related information is reported as LSP diagnostics. |
| **[Hover][spec-hover]** | Additional information is shown for: entities, methods, fields, local variables and references. |
| **[Signature Help][spec-signature-help]** | Parameter hints are shown when the cursor is inside a call expression, with the active parameter highlighted. Includes the method's docstring when present. Requires the file to be saved — signature help is not available while the file has unsaved edits. |
| **[Go To Definition][spec-definition]** | For most language constructs, you can go from a reference to its definition. |
| **[Go To Declaration][spec-declaration]** | Navigate from a reference to the declaration site of a symbol. |
| **[Go To Type Definition][spec-type-definition]** | Navigate to the type definition of the symbol under the cursor. |
| **[Document Symbols][spec-document-symbols]** | pony-lsp provides a list of available symbols for each opened document. |
| **[Workspace Symbols][spec-workspace-symbols]** | Fuzzy search over all symbols across the entire workspace. |
| **[Document Highlight][spec-document-highlight]** | All occurrences of the symbol under the cursor are highlighted simultaneously across the document. |
| **[Inlay Hints][spec-inlay-hints]** | Three kinds of hints are shown inline. For `let`, `var`, and field declarations with no type annotation, the full inferred type appears after the variable name. For annotated types where the capability is omitted — in variable and field declarations, function parameter types, generic type arguments, and union and tuple members — the missing capability keyword appears after the type name. For `fun` declarations, the implicit receiver capability appears before the function name and the implicit return type appears after the closing parenthesis. |
| **[Find References][spec-references]** | All references to the symbol under the cursor are returned, with optional inclusion of the declaration site. |
| **[Rename][spec-rename]** | Rename a symbol and all its references across the workspace. |
| **[Folding Range][spec-folding-range]** | Code folding ranges are provided for blocks, methods, classes, and other structured constructs. |
| **[Selection Range][spec-selection-range]** | Smart expand/shrink selection based on the AST structure of the document. |

[spec-diagnostics]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#pull-diagnostics
[spec-hover]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#hover-request-leftwards_arrow_with_hook
[spec-signature-help]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_signatureHelp
[spec-definition]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#goto-definition-request-leftwards_arrow_with_hook
[spec-declaration]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#goto-declaration-request-leftwards_arrow_with_hook
[spec-type-definition]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#goto-type-definition-request-leftwards_arrow_with_hook
[spec-document-symbols]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#document-symbols-request-leftwards_arrow_with_hook
[spec-workspace-symbols]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#workspace-symbols-request-leftwards_arrow_with_hook
[spec-document-highlight]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#document-highlights-request-leftwards_arrow_with_hook
[spec-inlay-hints]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlay-hint-request-leftwards_arrow_with_hook
[spec-references]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#find-references-request-leftwards_arrow_with_hook
[spec-rename]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#rename-request-leftwards_arrow_with_hook
[spec-folding-range]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#folding-range-request-leftwards_arrow_with_hook
[spec-selection-range]: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#selection-range-request-leftwards_arrow_with_hook

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
