## Fix systematic testing build failure under gcc

Building with `use=systematic_testing` under gcc failed to compile with a spurious compiler error. The build now compiles cleanly under gcc.

## Fix an intermittent systematic testing hang

Programs run under systematic testing could intermittently hang instead of running to completion. This has been fixed.

