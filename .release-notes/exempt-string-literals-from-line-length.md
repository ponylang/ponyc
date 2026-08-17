## Exempt triple-quoted string literals from the 80-column lint rule

pony-lint's `style/line-length` rule now skips lines inside triple-quoted string literals that are not docstrings. Triple-quoted strings used as data — JSON templates, inline test fixtures, multi-line format strings — exist for readability, and forcing them to wrap at 80 columns defeats their purpose. Docstring prose is still checked.
