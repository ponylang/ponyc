# pony_check

A program showing example tests using the PonyCheck property based testing package.

## How to compile

With a minimal Pony installation, in the same directory as this README file, run `ponyc`. You should see content building the necessary packages, which ends with:

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

Once `pony_check` has been compiled, in the same directory as this README file, run `./pony_check`. You should see a PonyTest runner output showing the tests run and their results; just like you would with PonyTest in general, except the tests in question are PonyCheck tests.

```console
1 test started, 0 complete: list/reverse/one started
...
---- Passed: list/reverse
---- Passed: list/reverse/one
---- Passed: list/properties
---- Passed: custom_class/flat_map
---- Passed: custom_class/map
---- Passed: custom_class/custom_generator
---- Passed: async/tcp_sender
---- Passed: collections/operation_on_random_collection_elements
---- Passed: health_check/narrow_filter
---- Passed: stateful/counter
---- Passed: stateful/async_counter
----
---- 11 tests ran.
---- Passed: 11
```

The `health_check/narrow_filter` test demonstrates health check configuration.
Run with `--verbose` to see the `WARNING:` lines it produces about filter
discard rate and slow samples.

The `stateful/counter` test verifies a simple in-memory counter with
`StatefulProperty`, checking that increments and decrements keep the
counter in sync with a model. The `stateful/async_counter` test does the
same against an actor-based counter, with the invariant querying the
actor's state asynchronously through the action mechanism.
