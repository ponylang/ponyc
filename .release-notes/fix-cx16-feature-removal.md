## Fix link failure on x86-64 when passing `--features=-cx16`

The Pony runtime uses 128-bit atomic operations that need the x86-64 `cx16` CPU feature. ponyc turns that feature on for x86-64 targets, but passing `--features=-cx16` turned it off and ponyc didn't turn it back on. The compiled program then referenced `__atomic_compare_exchange_16`, and on platforms where nothing provides that function, such as macOS, linking failed with an undefined symbol. ponyc now keeps `cx16` on for every x86-64 target, even when `--features` turns it off.
