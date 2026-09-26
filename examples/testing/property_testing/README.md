# property_testing

A program showing example property-based tests using the PonyTest package.

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
Writing ./property_testing.o
Linking ./property_testing
```

## How to Run

Once `property_testing` has been compiled, in the same directory as this README file, run `./property_testing`. You should see a PonyTest runner output showing the tests run and their results.

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
----
---- 9 tests ran.
---- Passed: 9
```

Run `health_check/narrow_filter` with `--verbose` to see health check
warnings about filter discard rate and slow samples.
