# json

Demonstrates the standard library JSON package: building JSON documents,
parsing JSON text, reading values with `JsonNav`, composable paths with
`JsonLens`, string-based queries with `JsonPath` (including filters and
function extensions), and serialization.

## How to Compile

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
Writing ./json.o
Linking ./json
```

## How to Run

Once `json` has been compiled, in the same directory as this README file run `./json`. You should see output demonstrating each JSON feature.
