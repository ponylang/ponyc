## Fix unnecessary reallocations when building small Strings and Arrays

Building a small `String` or `Array` incrementally — appending or pushing through its first several elements — triggered repeated reallocations and copies, even though every one of those sizes fit within the single block the runtime allocator had already handed out. The collections now record the capacity of the block they were given, so a small `String` or `Array` allocates once and does not reallocate again until it genuinely outgrows that block.

A side effect is that `space()` may report a larger capacity than before for a small collection; the extra capacity is memory the allocator had already reserved, so a program's memory use is unchanged.
