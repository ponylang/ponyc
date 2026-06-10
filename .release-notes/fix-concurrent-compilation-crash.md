## Fix intermittent crashes when compiling on multiple threads at once

The compiler could crash intermittently when more than one compilation ran at the same time within a single process, as the Pony language server does while you edit. Because the failure depended on thread timing, it appeared as occasional, hard-to-reproduce crashes rather than a consistent error.

Concurrent compilations no longer share state that simultaneous access could corrupt, so these crashes no longer happen.
