# pony_check

A program showing example tests using the PonyCheck property based testing package.

## How to compile

With a minimal Pony installation, in the same directory as this README file run `ponyc`. You should see content building the necessary packages, which ends with:

```console
...
Generating
 Reachability
 Selector painting
 Data prototypes
 Data types
 Function prototypes
 Functions
 Descriptors
Optimising
Writing ./pony_check.o
Linking ./pony_check
```

## How to Run

Once `pony_check` has been compiled, in the same directory as this README file run `./pony_check`. You should see a PonyTest runner output showing the tests run and their results; just like you would with PonyTest in general, except, the tests in question are PonyCheck tests.

```console
1 test started, 0 complete: list/reverse/one started
2 tests started, 0 complete: list/properties started
3 tests started, 0 complete: list/reverse started
4 tests started, 0 complete: custom_class/map started
5 tests started, 0 complete: custom_class/custom_generator started
6 tests started, 0 complete: async/tcp_sender started
7 tests started, 0 complete: collections/operation_on_random_collection_elements started
8 tests started, 0 complete: custom_class/flat_map started
8 tests started, 1 complete: list/properties complete
8 tests started, 2 complete: custom_class/flat_map complete
8 tests started, 3 complete: custom_class/custom_generator complete
8 tests started, 4 complete: custom_class/map complete
8 tests started, 5 complete: list/reverse/one complete
8 tests started, 6 complete: collections/operation_on_random_collection_elements complete
8 tests started, 7 complete: list/reverse complete
8 tests started, 8 complete: async/tcp_sender complete
---- Passed: list/reverse
---- Passed: list/reverse/one
---- Passed: list/properties
---- Passed: custom_class/flat_map
---- Passed: custom_class/map
---- Passed: custom_class/custom_generator
---- Passed: async/tcp_sender
---- Passed: collections/operation_on_random_collection_elements
----
---- 8 tests ran.
---- Passed: 8
```
