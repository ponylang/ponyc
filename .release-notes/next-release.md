## Fix "iftype" expressions not being usable in lambdas or object literals

This release fixes an issue where the compiler would hit an assertion error when compiling programs that placed `iftype` expressions inside lambdas or object literals.

