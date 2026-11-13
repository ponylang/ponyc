## Fix C shims that include ponyassert.h

C shims that include `ponyassert.h` now compile with an installed compiler. Compilation previously failed because a required header was missing from the installation.

## Apply runtime tracing category changes to ASIO

Runtime tracing category changes now apply to the ASIO thread as well as worker and pinned schedulers. Previously, ASIO retained its startup categories, so enabling a category at runtime could omit ASIO events and disabling or replacing a category could leave ASIO events enabled.

