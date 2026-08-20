## Fix pony-lint false positive on partial operators

pony-lint's operator-spacing rule flagged partial arithmetic operators (`*?`, `+?`, etc.) as missing a space after the base operator. The `?` that makes the operation partial shares the same AST token as the non-partial form, so the rule saw `?` where it expected a space.
