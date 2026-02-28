## Add `style/method-declaration-format` and `style/type-parameter-format` lint rules

pony-lint now checks formatting of multiline method declarations and type parameter lists.

`style/method-declaration-format` checks that multiline method declarations have each parameter on its own line, the return type `:` indented one level from the method keyword, and the `=>` aligned with the method keyword.

`style/type-parameter-format` checks that multiline type parameter lists have the opening `[` on the same line as the name, each type parameter on its own line, and the `is` keyword (for entity provides clauses) indented one level from the entity keyword.
