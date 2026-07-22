## 32-bit ARM Linux is now a best-effort target

32-bit ARM Linux is no longer tested in CI. It is now a best-effort target: we build and test it periodically on real hardware rather than continuously in CI. Pony still builds and runs on 32-bit ARM and we don't intend to break it, but a change can break the 32-bit build between those checks without CI catching it.
