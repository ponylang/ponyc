## Add LSP `textDocument/foldingRange` support

The Pony language server now handles `textDocument/foldingRange` requests, enabling editors to show fold regions for Pony source files.

A fold range is emitted for each top-level type entity (class, actor, struct, primitive, trait, interface) and for each multi-line member (fun, be, new). Within method bodies, fold ranges are also emitted for compound expressions: if (including ifdef, which is resolved to if by the compiler), while, for, repeat, match, try, and recover blocks. Single-line nodes are excluded since there is nothing to fold.

The server also sends `workspace/foldingRange/refresh` after each compilation when the editor advertises support for it, so that fold regions update automatically when files change.
