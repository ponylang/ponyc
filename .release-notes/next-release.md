## Remove simplebuiltin compiler option

The ponyc option `simplebuiltin` was never useful to users but had been exposed for testing purposes for Pony developers. We've removed the need for the "simplebuiltin" package for testing and have remove both it and the compiler option.

Technically, this is a breaking change, but no end-users should be impacted.

