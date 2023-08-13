## Remove `stable` from Docker images

Previously, we were including our very old and very deprecated dependency management tool `stable` in our `ponyc` Docker images. We've stopped.

## Fix broken linking when using a sanitizer

The internal link command used when building a Pony program with a sanitizer in place wasn't correctly generated and linking the program would fail. This has been fixed.

