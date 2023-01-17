## Enable building of libponyc-standalone on MacOS

We now ship a "standalone" version of libponyc for MacOS. libponyc-standalone allows applications to use Pony compiler functionality as a library. The standalone version contains "all dependencies" needed in a single library. On MacOS, sadly "all dependencies" means "all that can be statically linked", so unlike Linux, dynamically linking to C++ standard library is required on MacOS.

An example pony program linking against it would need to look like this:

```pony
use "lib:ponyc-standalone" if posix or osx
use "lib:c++" if osx

actor Main
  new create(env: Env) =>
    None
```

