# Examples

Each subdirectory is a self-contained Pony program demonstrating a different part of the language and standard library. Examples are grouped by category.

## Getting Started

### [helloworld](helloworld/)

Prints "Hello, world." to the console. Shows the minimal structure of a Pony program: a `Main` actor with a `create` constructor that receives an `Env`. Start here if you're new to Pony.

### [circle](circle/)

Creates 100 circles with radii from 1 to 100, calculating and printing each circle's circumference and area. Demonstrates basic class definitions, constructors, instance methods, `F32` arithmetic, and iteration with `Range`.

## Language Features

### [commandline](commandline/)

An echo program that accepts words as arguments and optionally converts them to uppercase. Demonstrates command-line parsing with the `cli` package, including `CommandSpec.leaf()`, `OptionSpec.bool()`, and `ArgSpec.string_seq()`.

### [constrained_type](constrained_type/)

Validates a username against business rules (6-12 lowercase ASCII characters) using the `constrained_types` package. Demonstrates making illegal states unrepresentable by validating at the construction boundary with `Constrained`, `MakeConstrained`, and a custom `Validator` primitive.

### [ifdef](ifdef/)

Demonstrates compile-time conditional logic with `ifdef` for platform detection and build flags. Shows platform-specific FFI declarations, conditional compilation with `-D` flags, and `compile_error` to abort compilation when constraints aren't met.

### [lambda](lambda/)

Creates and applies lambda functions with the syntax `{(a: U32, b: U32): U32 => a + b}`. Demonstrates lambdas as first-class values, function types as parameters, and error handling in functional code.

### [printargs](printargs/)

Prints all command-line arguments and environment variables passed to the program. Demonstrates `Env.args` for argument access and `Env.vars` for environment variable iteration.

## Actors and Concurrency

### [counter](counter/)

Sends multiple increments to a `Counter` actor and prints the accumulated result. Demonstrates actor-based mutable state, asynchronous message passing with behaviors, and the callback pattern where an actor sends results back to the caller.

### [mailbox](mailbox/)

Stress-tests Pony's mailbox system by having multiple sender actors flood a single receiver with messages. Useful for observing how mailbox depth and memory usage scale under high message volume.

### [message-ubench](message-ubench/)

Microbenchmark measuring message-passing throughput under sustained load. A `SyncLeader` coordinates multiple `Pinger` actors that exchange messages at random, reporting messages per second. Demonstrates `Timer`-based coordination, controlled in-flight message counts, and two-phase stop-then-report synchronization.

### [mixed](mixed/)

Benchmarks multiple rings of actors, each passing messages around the ring while spawning `Worker` actors for side computation. Demonstrates ring topology, recursive actor creation, and parallel independent rings.

### [ring](ring/)

Creates a ring of actors connected in a cycle and passes a message around the ring, printing the ID of the final recipient. Demonstrates circular actor linkage, recursive message passing, and scalable actor creation with loops.

### [spreader](spreader/)

Builds a binary tree of actors to a specified depth, then counts the total nodes by aggregating results from leaves upward. Demonstrates tree-structured actor topologies, bottom-up result aggregation, and union type matching on `(Spreader | None)`.

### [timers](timers/)

Schedules periodic timer callbacks, tracks invocation counts, and cancels a timer early. Demonstrates the `time` package's `Timers`, `Timer`, and `TimerNotify` interfaces, including timer creation with initial delay and repeat interval.

### [yield](yield/)

Demonstrates converting infinite loops in behaviors to tail-recursive behavior calls so the runtime can interleave garbage collection and process other messages. Illustrates the "lonely pony problem" where an infinite loop starves the scheduler, and the solution of yielding between iterations.

## Networking

### [echo](echo/)

A TCP echo server that listens on a random port and echoes back any data received. Demonstrates the `net` package's `TCPListener`, `TCPListenNotify`, `TCPConnection`, and `TCPConnectionNotify` interfaces for event-driven I/O.

### [net](net/)

A ping-pong example demonstrating both TCP and UDP communication. Shows `TCPListener` and `TCPConnection` for TCP, `UDPSocket` for UDP, and `NetAddress` for IP/port handling across multiple actor roles.

## Backpressure

### [fan-in](fan-in/)

A microbenchmark simulating thundering herd workloads with many senders, many analyzers, and a single receiver. Demonstrates how Pony's runtime backpressure system handles fan-in patterns, with `Timer`-based coordination and nanosecond-precision latency measurement.

### [overload](overload/)

Floods a single `Receiver` actor with messages from multiple senders to demonstrate Pony's built-in backpressure. The runtime automatically throttles senders when the receiver's mailbox grows, preventing unbounded memory usage.

### [under_pressure](under_pressure/)

Establishes a TCP connection and applies explicit backpressure when the send buffer fills up. Demonstrates the `backpressure` package's `Backpressure` primitive, `ApplyReleaseBackpressureAuth`, and throttled/unthrottled callbacks for detecting when the scheduler responds to pressure.

## File I/O and Terminal

### [files](files/)

Reads a file specified as a command-line argument and prints its path and contents line by line. Demonstrates capability-based file access with `FilePath`, `FileAuth`, and `FileCaps`, and resource cleanup with `with` blocks.

### [readline](readline/)

An interactive command-line prompt with tab completion and command history. Demonstrates the `term` package's `Readline` and `ReadlineNotify` interfaces, and `Promise`-based prompt control where rejecting the promise exits the loop.

## Data Formats

### [json](json/)

Demonstrates the `json` standard library package: building JSON documents with `JsonObject` and `JsonArray`, parsing JSON text with `JsonParser`, reading nested values with `JsonNav`, composable get/set/remove with `JsonLens`, and string-based queries with `JsonPath` including filters, slicing, and function extensions (`match`, `search`, `length`, `count`).

## C FFI

### [ffi-callbacks](ffi-callbacks/)

Passes Pony functions as callbacks to C code using three mechanisms: bare functions with `@` annotation, bare lambdas with `@{...}` syntax, and partial application with the `~` operator. Each mechanism requires compiling and linking a companion C library.

### [ffi-struct](ffi-struct/)

Passes Pony structs to C functions, showing both `embed` fields (inline like C nested structs) and `var` fields (pointer-based). Demonstrates that Pony `struct` types have identical binary layout to their C counterparts for zero-copy interop.

## Testing and Benchmarking

### [pony_bench](pony_bench/)

Runs microbenchmarks using the `pony_bench` package, reporting mean, median, and deviation. Demonstrates synchronous benchmarks with the `MicroBenchmark` trait, asynchronous benchmarks with `AsyncMicroBenchmark`, and `DoNotOptimise` to prevent dead code elimination.

### [pony_check](pony_check/)

Demonstrates property-based testing with the `pony_check` package. Shows `Property1UnitTest` for defining properties, built-in and custom generators for producing test data, generator composition with `flat_map`, and async property testing over TCP.

## Benchmarks and Simulations

### [gups_basic](gups_basic/)

Measures GUPS (Giga Updates Per Second), a random-access memory performance metric, using multiple `Streamer` and `Updater` actors that perform random XOR updates on a large array. Demonstrates actor-based work distribution with configurable parallelism and completion tracking via countdown patterns.

### [gups_opt](gups_opt/)

An optimized variant of `gups_basic` that measures GUPS with array recycling for memory reuse. Demonstrates distributed computation across `Updater` actors, each managing a portion of the update table, with barrier synchronization for completion detection.

### [mandelbrot](mandelbrot/)

Plots the Mandelbrot set using divide-and-conquer parallelization across `Worker` actors, outputting a PBM (Portable Bitmap) image. Demonstrates parallel computation with result coordination, file I/O with seeking, and bit manipulation for compact pixel packing.

### [n-body](n-body/)

Simulates gravitational interaction among five planetary bodies (Sun, Jupiter, Saturn, Uranus, Neptune) using Newtonian physics. Demonstrates classes with multiple factory constructors, `F64` floating-point arithmetic, and pairwise force calculations with velocity and position integration.

## Dynamic Tracing

### [dtrace](dtrace/)

Example DTrace scripts for tracing Pony runtime behavior on macOS/BSD, including GC events, actor scheduling, and message throughput. Demonstrates the Pony runtime's DTrace provider interface with probes for garbage collection, scheduling, and telemetry aggregation.

### [runtime_info](runtime_info/)

Queries and displays runtime statistics including actor heap memory, GC metrics, CPU time, and scheduler state. Demonstrates the `runtime_info` package's `ActorStats` and `SchedulerStats` with auth-gated access via `ActorStatsAuth` and `SchedulerStatsAuth`. Requires the compiler to be built with `use=runtimestats_messages`.

### [systemtap](systemtap/)

Example SystemTap scripts for tracing Pony runtime behavior on Linux, covering GC events, scheduling, and telemetry. Functionally equivalent to the `dtrace` examples but using SystemTap's probe syntax, requiring a Linux kernel with UPROBES support and the compiler built with `use=dtrace`.
