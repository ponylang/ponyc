## Fix crash when tracing actor behaviors without forced actor tracing

A runtime built with `runtime_tracing` enabled would crash when the `actor_behavior` tracing category was active without `--ponytracingforceactortracing` also being set:

```
program --ponytracingcategories actor_behavior
```

The crash occurred whenever a message was scheduled from a thread that was not running an actor, such as the ASIO thread delivering a network event or the cycle detector. Programs that perform any I/O or run long enough to trigger cycle detection would reliably crash. Setting `--ponytracingforceactortracing all` avoided the crash.

This has been fixed. Actor behavior tracing now works without forcing actor tracing on.
