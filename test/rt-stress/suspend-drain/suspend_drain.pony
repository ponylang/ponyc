"""
Suspend-and-drain stress engine.

Drives the allocator's suspend-and-drain path with real
schedulers: memory allocated on every scheduler thread is freed
after those threads have suspended, and the freeing must bring the
physical memory back without activating any of them into
scheduling work.
"""
