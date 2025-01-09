## Change stack_depth_t to size_t on OpenBSD

The definition of stack_depth_t was changed from int to size_t to support compiling on OpenBSD 7.6.

ponyc may not be compilable on earlier versions of OpenBSD.

## Make sure scheduler threads don't ACK the quiescence protocol CNF messages if they have an actor waiting to be run

Prior to this, the pinned actor thread could cause early termination/quiescence of a pony program if there were only pinned actors active. This change fixes the issue to ensure that pony programs with only pinned actors active will no longer terminate too early.

