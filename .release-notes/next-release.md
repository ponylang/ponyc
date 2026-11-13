## Fix C shims that include ponyassert.h

C shims that include `ponyassert.h` now compile with an installed compiler. Compilation previously failed because a required header was missing from the installation.

