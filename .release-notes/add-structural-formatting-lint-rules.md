## Add structural formatting lint rules to pony-lint

Six new lint rules enforce structural formatting conventions from the Pony style guide:

- `style/blank-lines` — Checks blank line conventions within and between entities. No blank line before the first body content, no blank lines between consecutive fields, exactly one blank line before each method. Between entities, exactly one blank line is required. When both the preceding and current item are one-liners, zero blank lines are allowed.
- `style/match-no-single-line` — Match expressions must span multiple lines.
- `style/match-case-indent` — The `|` in match cases must align with the `match` keyword's column.
- `style/partial-spacing` — The `?` in partial method declarations must have a space before and after it (e.g., `fun foo() ? =>`).
- `style/partial-call-spacing` — The `?` at partial call sites must immediately follow the closing `)` with no space (e.g., `foo()?`).
- `style/dot-spacing` — No spaces around `.` for method calls; `.>` (method chaining) must be spaced as an infix operator with spaces on both sides.

All rules are enabled by default and can be individually suppressed with `// pony-lint: allow <rule-id>` comments or disabled via configuration.
