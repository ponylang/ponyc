"""
Demonstrates iftype specialization: multiple definitions of the same method
distinguished by iftype guards on type parameters. The matching body is
selected at reification time — zero runtime cost.

When `A` is `U32` (which implements `Stringable`), the specialization is
used. When `A` is `MyVal` (which does not), the default is used.
"""
