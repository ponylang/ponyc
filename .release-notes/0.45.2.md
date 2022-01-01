## Clarify wording for some subtyping errors

The wording of a small number of errors relating
to the subtyping of capabilities were improved.
These were previously technically incorrect,
or otherwise unnecessarily obtuse.

## Fix inability to fully build pony on Raspberry PI 4's with 64-bit Raspbian

Building on 64-bit Raspbian needs the flag `-fPIC` instead of `-fpic` which is used by default on Unix builds.

We've added a Makefile flag `pic_flag` that you can set to either `-fpic` or `-fPIC` for both libs and configure steps, e.g. `make libs pic_flag=-fPIC` and `make configure pic_flag=-fPIC`. It will default to `-fpic` if not specified.

## Added build instructions for 64-bit Raspbian

We've gotten Pony building on 64-bit Raspbian running on Raspberry Pi 4 model B boards. The installation is a little different than other Linux distributions that you can build Pony on, so we've added instructions specific to 64-bit Raspbian.

