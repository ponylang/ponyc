## Add pony-lint to the ponyc distribution

pony-lint is a text-based linter for Pony source files that checks for style guide violations. It was previously a standalone project and is now distributed with ponyc. This means the linter will track changes in ponyc and will always be up-to-date with the compiler's version of the standard library. pony-lint currently checks for line length, trailing whitespace, hard tabs, and comment spacing violations.
