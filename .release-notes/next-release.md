## Change stack_depth_t to size_t on OpenBSD

The definition of stack_depth_t was changed from int to size_t to support compiling on OpenBSD 7.6.

ponyc may not be compilable on earlier versions of OpenBSD.

