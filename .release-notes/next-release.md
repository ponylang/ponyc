## Add LSP `workspace/inlayHint/refresh` support

After each compilation, pony-lsp now sends a `workspace/inlayHint/refresh` request to the editor, asking it to re-request inlay hints for all open documents. Previously, inlay hints (such as inferred type annotations) would not update after a file was saved and recompiled. This only takes effect when the editor advertises support for `workspace/inlayHint/refresh` in its LSP capabilities (all major editors do).

