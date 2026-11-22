## Speed up sending objects between actors

When an actor sends a message, receives one, or collects its own garbage, the runtime looks up which actor owns each object it traces. Except on macOS, each thread now remembers the last region of memory it looked up, so lookups for objects in the same region skip most of that work. In most of our Linux benchmarks that pass arrays of objects back and forth between actors, programs ran 2–6% faster.
