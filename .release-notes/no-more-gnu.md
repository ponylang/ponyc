## Stop building "x86-64-unknown-linux-gnu" packages

Previously we built a "generic gnu" ponyc package and made it available via Cloudsmith and ponyup. Unfortunately, there is no such thing as a "generic gnu" ponyc build that is universally usable. The ponyc package only worked if the library versions on your Glibc based Linux distribution matched those of our build environment.

We've stoped building the "generic gnu" aka "x86-64-unknown-linux-gnu" packages of ponyc as they had limited utility and a couple of different ways of creating a very bad user experience. Please see our [Linux installation instructions](https://github.com/ponylang/ponyc/blob/main/INSTALL.md#linux) for a list of Linux distributions we currently create ponyc packages for.
