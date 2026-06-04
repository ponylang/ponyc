## Remove the unused serialisation subsystem

Pony's standard library no longer ships a `serialise` package; it was removed in a previous release. The compiler and runtime still carried all of the machinery that package fronted, even though no Pony code could reach it anymore. That machinery has now been removed.

This has a few user-visible consequences.

`libponyrt` no longer exports `pony_serialise`, `pony_deserialise`, or their offset, reserve, and block helpers. These were `PONY_API` functions, so out-of-tree C/FFI code that called them directly will no longer link. There is no in-tree replacement; serialisation is not a feature Pony provides.

Custom serialisation is no longer a recognised language feature. The `_serialise_space`, `_serialise`, and `_deserialise` methods are now ordinary methods with no special meaning. A type that defines them will compile, but the compiler no longer requires all three to be present together and no longer generates serialisation functions for them.

Compiled binaries no longer contain the `internal.signature` function. It existed only to stamp serialised data so the runtime could reject data produced by an incompatible build; with serialisation gone, there is nothing to stamp.
