## Add timer methods to TCPBackend trait

`TCPBackend` has three new methods: `create_timer`, `set_timer`, and `unsubscribe_timer`. The idle timer now calls through the backend instead of calling `PonyAsio` directly, so fake backends can intercept timer operations for deterministic testing.

Custom `TCPBackend` implementations must add the three methods. A no-op stub is sufficient when timer behavior is not under test.

Before:

```pony
class MyBackend is TCPBackend
  // ... existing methods ...
```

After:

```pony
class MyBackend is TCPBackend
  // ... existing methods ...

  fun ref create_timer(the_actor: AsioEventNotify, nsec: U64): AsioEventID =>
    AsioEvent.none()

  fun ref set_timer(event: AsioEventID, nsec: U64) => None

  fun ref unsubscribe_timer(event: AsioEventID) => None
```
