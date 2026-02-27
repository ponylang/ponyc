## Add operator-spacing and lambda-spacing lint rules to pony-lint

pony-lint now checks two additional whitespace rules from the style guide:

- **`style/operator-spacing`**: Binary operators require surrounding spaces, `not` requires a space after, and unary minus must not have a space after. Continuation lines where the operator starts the line are exempt from the "before" check.

- **`style/lambda-spacing`**: No space after `{` in lambda expressions; space before `}` only on single-line lambdas. Lambda types require no space on either side. Bare lambdas (`@{`) follow the same rules.

Both rules are on by default and can be disabled individually or via the `style` category.
