# Core AST / token definitions

Changes to token definitions (`token.h`), AST node types, the lexer, parser, AST manipulation, sugar/desugaring passes, or subtype checking can break the tools and pony-compiler tests. Adding or reordering token enum values renumbers every subsequent token and silently breaks any code matching on specific `TK_*` types. Run: `make test-pony-compiler`, `make test-pony-lint`, `make test-pony-lsp`, `make test-pony-doc`.
