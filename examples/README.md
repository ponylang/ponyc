# Examples

Each subdirectory is a self-contained Pony program demonstrating a different part of the language and standard library. Examples are grouped by category.

## Getting Started

### [helloworld](getting-started/helloworld/)

Prints "Hello, world." to the console. Shows the minimal structure of a Pony program: a `Main` actor with a `create` constructor that receives an `Env`. Start here if you're new to Pony.

### [circle](getting-started/circle/)

Creates 100 circles with radii from 1 to 100, calculating and printing each circle's circumference and area. Demonstrates basic class definitions, constructors, instance methods, `F32` arithmetic, and iteration with `Range`.

## Language Features

### [commandline](language-features/commandline/)

An echo program that accepts words as arguments and optionally converts them to uppercase. Demonstrates command-line parsing with the `cli` package, including `CommandSpec.leaf()`, `OptionSpec.bool()`, and `ArgSpec.string_seq()`.

### [constrained_type](language-features/constrained_type/)

Validates a username against business rules (6-12 lowercase ASCII characters) using the `constrained_types` package. Demonstrates making illegal states unrepresentable by validating at the construction boundary with `Constrained`, `MakeConstrained`, and a custom `Validator` primitive.

### [ifdef](language-features/ifdef/)

Demonstrates compile-time conditional logic with `ifdef` for platform detection and build flags. Shows platform-specific FFI declarations, conditional compilation with `-D` flags, and `compile_error` to abort compilation when constraints aren't met.

### [lambda](language-features/lambda/)

Creates and applies lambda functions with the syntax `{(a: U32, b: U32): U32 => a + b}`. Demonstrates lambdas as first-class values, function types as parameters, and error handling in functional code.

### [printargs](language-features/printargs/)

Prints all command-line arguments and environment variables passed to the program. Demonstrates `Env.args` for argument access and `Env.vars` for environment variable iteration.

## Actors and Concurrency

### [counter](actors/counter/)

Sends multiple increments to a `Counter` actor and prints the accumulated result. Demonstrates actor-based mutable state, asynchronous message passing with behaviors, and the callback pattern where an actor sends results back to the caller.

### [mailbox](actors/mailbox/)

Stress-tests Pony's mailbox system by having multiple sender actors flood a single receiver with messages. Useful for observing how mailbox depth and memory usage scale under high message volume.

### [message-ubench](actors/message-ubench/)

Microbenchmark measuring message-passing throughput under sustained load. A `SyncLeader` coordinates multiple `Pinger` actors that exchange messages at random, reporting messages per second. Demonstrates `Timer`-based coordination, controlled in-flight message counts, and two-phase stop-then-report synchronization.

### [mixed](actors/mixed/)

Benchmarks multiple rings of actors, each passing messages around the ring while spawning `Worker` actors for side computation. Demonstrates ring topology, recursive actor creation, and parallel independent rings.

### [ring](actors/ring/)

Creates a ring of actors connected in a cycle and passes a message around the ring, printing the ID of the final recipient. Demonstrates circular actor linkage, recursive message passing, and scalable actor creation with loops.

### [spreader](actors/spreader/)

Builds a binary tree of actors to a specified depth, then counts the total nodes by aggregating results from leaves upward. Demonstrates tree-structured actor topologies, bottom-up result aggregation, and union type matching on `(Spreader | None)`.

### [timers](actors/timers/)

Schedules periodic timer callbacks, tracks invocation counts, and cancels a timer early. Demonstrates the `time` package's `Timers`, `Timer`, and `TimerNotify` interfaces, including timer creation with initial delay and repeat interval.

### [yield](actors/yield/)

Demonstrates converting infinite loops in behaviors to tail-recursive behavior calls so the runtime can interleave garbage collection and process other messages. Illustrates the "lonely pony problem" where an infinite loop starves the scheduler, and the solution of yielding between iterations.

## Networking

### [echo-server](networking/echo-server/)

Minimal TCP echo server demonstrating the `net` package's core pattern. Shows the three building blocks: `TCPListenerActor` for accepting connections, `TCPConnectionActor` for event plumbing, and `ServerLifecycleEventReceiver` for application callbacks.

### [connection-timeout](networking/connection-timeout/)

Connects to a non-routable address (RFC 5737 TEST-NET-1) with a 3-second connection timeout. Demonstrates `MakeConnectionTimeout`, `ConnectionTimeout`, and exhaustive matching on `ConnectionFailureReason`.

### [framed-protocol](networking/framed-protocol/)

Length-prefixed message framing using `buffer_until()` and multi-buffer `send()`. A client and server exchange messages with 4-byte big-endian length headers, demonstrating how to switch between reading headers and variable-length payloads.

### [idle-timeout](networking/idle-timeout/)

Echo server that closes idle connections after 10 seconds. Demonstrates `idle_timeout()` for setting a per-connection inactivity timer and `_on_idle_timeout()` for handling expiration, using the connection's built-in ASIO timer.

### [infinite-ping-pong](networking/infinite-ping-pong/)

Client and server exchanging Ping/Pong messages in an endless loop. Shows both sides of a TCP conversation with `ServerLifecycleEventReceiver` and `ClientLifecycleEventReceiver`, using `buffer_until()` for fixed-size message delivery.

### [ip-version](networking/ip-version/)

IPv4-only echo server with a built-in client. Demonstrates the `ip_version` parameter on `TCPListener` and `TCPConnection.client` to restrict connections to `IP4`. The same approach works with `IP6` for IPv6-only connections.

### [read-buffer-size](networking/read-buffer-size/)

Configurable read buffer sizing with two phases: a small control phase (128 bytes) and a bulk transfer phase (8192 bytes). Demonstrates `set_read_buffer_minimum()` and `resize_read_buffer()` for dynamic buffer management.

### [send-completion](networking/send-completion/)

Per-send completion tracking with `SendToken`. A client sends five labeled messages and tracks each one through `_on_sent` and `_on_send_failed` callbacks, keyed by token id, to know which sends have been handed to the OS.

### [socket-options](networking/socket-options/)

Socket option tuning on a connected TCP connection. Configures `TCP_NODELAY` and OS buffer sizes using both dedicated convenience methods (`set_nodelay()`, `set_so_rcvbuf()`) and the general-purpose `getsockopt_u32()`/`setsockopt_u32()` interface.

### [timer](networking/timer/)

Query-timeout simulation using `set_timer()`. A client sends a query to a non-responding server and sets a 3-second timer. When it fires, `_on_timer()` logs the timeout and closes the connection. Unlike `idle_timeout()`, this fires unconditionally regardless of I/O activity.

### [udp-echo-server](networking/udp-echo-server/)

Minimal UDP echo server. A single actor binds a UDP socket and echoes every received datagram back to its sender. Shows `UDPSocketActor` for event plumbing and `UDPLifecycleEventReceiver` for application callbacks.

### [yield-read](networking/yield-read/)

Demonstrates returning `YieldReading` for cooperative scheduler fairness. A flood client sends 100 messages and the server yields every 10 messages, exiting the read loop to let other actors run. Unlike `mute()`/`unmute()`, `YieldReading` is a one-shot pause that resumes automatically.

### SSL

#### [echo-server](networking/ssl/echo-server/)

SSL version of the echo server. Adds `SSLContext` setup and uses `TCPConnection.ssl_server` for transparent SSL handshaking. Must be run from the project root for certificate paths to resolve.

#### [infinite-ping-pong](networking/ssl/infinite-ping-pong/)

SSL version of infinite ping-pong. Shows both `TCPConnection.ssl_server` and `TCPConnection.ssl_client` in the same program, with `buffer_until()` for fixed-size message delivery over TLS. Must be run from the project root.

#### [starttls-ping-pong](networking/ssl/starttls-ping-pong/)

STARTTLS upgrade from plaintext to TLS mid-connection. The client sends "STARTTLS", the server replies "OK", both sides call `start_tls()`, and then exchange Ping/Pong messages over the encrypted connection. Must be run from the project root.

### Notifier

#### [echo-server](networking/notifier/echo-server/)

Minimal echo server using the notifier API. Shows the same behavior as `echo-server` but with `TCPListenNotify` and `ServerTCPConnectionNotify` traits instead of implementing actor-level delegation directly.

#### [ping-pong](networking/notifier/ping-pong/)

Client and server exchanging messages using the notifier API. Shows `TCPListenNotify`, `ClientTCPConnectionNotify`, and `ServerTCPConnectionNotify` working together with `buffer_until()` for fixed-size message framing.

#### [udp-echo-server](networking/notifier/udp-echo-server/)

UDP echo server using the notifier API. Shows the same behavior as `udp-echo-server` but with a `UDPSocketNotify` trait instead of implementing `UDPSocketActor` and `UDPLifecycleEventReceiver` directly.

### Backpressure

#### [fan-in](networking/backpressure/fan-in/)

A microbenchmark simulating thundering herd workloads with many senders, many analyzers, and a single receiver. Demonstrates how Pony's runtime backpressure system handles fan-in patterns, with `Timer`-based coordination and nanosecond-precision latency measurement.

#### [overload](networking/backpressure/overload/)

Floods a single `Receiver` actor with messages from multiple senders to demonstrate Pony's built-in backpressure. The runtime automatically throttles senders when the receiver's mailbox grows, preventing unbounded memory usage.

#### [backpressure](networking/backpressure/backpressure/)

A flood client sends 200 chunks of 64KB as fast as possible to a sink server, demonstrating the `net` package's backpressure handling. When the OS send buffer fills, `send()` returns `SendErrorNotWriteable` and `_on_throttled` fires. The client stops sending and resumes on `_on_unthrottled`, with `_on_sent` confirming each chunk reached the OS.

### HTTP Client

#### [basic](networking/http-client/basic/)

Connects to `example.com:80`, sends an HTTP GET for `/`, and prints the response status, headers, and body. Demonstrates the full `HTTPClientConnectionActor` lifecycle: `on_connected` to send the request, streaming body accumulation via `ResponseCollector`, exhaustive `ConnectionFailureReason` matching, and connection close after completion. Start here if you're new to the `http_client` package.

#### [query-params](networking/http-client/query-params/)

Connects to `httpbin.org` over HTTPS and sends a GET request with percent-encoded query parameters. Demonstrates the `Request` builder's `.query()` method for appending RFC 3986 encoded parameters to the request path.

#### [bearer-auth](networking/http-client/bearer-auth/)

Connects to `httpbin.org` over HTTPS and sends a GET request with a Bearer token in the Authorization header. Demonstrates the `Request` builder's `.bearer_auth()` method for token-based authentication.

#### [form-post](networking/http-client/form-post/)

Connects to `httpbin.org` over HTTPS and POSTs form-encoded data. Demonstrates `Request.post()` with `.form_body()` for `application/x-www-form-urlencoded` POST requests.

#### [multipart-upload](networking/http-client/multipart-upload/)

Connects to `httpbin.org` over HTTPS and POSTs a multipart form with a text field and a file attachment. Demonstrates `MultipartFormData` with `Request.post().multipart_body()` for `multipart/form-data` uploads.

#### [json-api](networking/http-client/json-api/)

Connects to `jsonplaceholder.typicode.com` over HTTPS, fetches a JSON todo item, and decodes it into a typed `Todo` object. Demonstrates `JSONDecoder` and `DecodeJSON` for typed JSON decoding, `ResponseCollector` for body accumulation, and `HTTPClientConnection.ssl()` for TLS connections.

#### [redirect](networking/http-client/redirect/)

Connects to `httpbin.org` over HTTPS and requests `/redirect/3`, which returns three chained 302 redirects before a final 200. The actor's callbacks see only the final response — `RedirectFollower` handles the hops internally. Demonstrates `RedirectFollower`, `RedirectFollowerNotify`, `RedirectConnectionFactory` for cross-origin hops, and `Origin.from_uri()` for extracting connection parameters from the redirect target.

#### [response-timeout](networking/http-client/response-timeout/)

Connects to `httpbin.org` over HTTPS with a 3-second response deadline on a deliberately slow endpoint. The timer fires before the response arrives, demonstrating `HTTPClientConnection.set_timer()`, `HTTPClientConnection.cancel_timer()`, and `on_timer()` in a response deadline pattern.

### URI

#### [parsing](networking/uri/parsing/)

Parses a full URI and prints each component (scheme, authority, host, port, path, query, fragment). Demonstrates `URI.query_params()`, standalone authority parsing with `ParseURIAuthority`, path segment splitting, percent-encoding and decoding, reference resolution, normalization, and equivalence checking.

#### [building](networking/uri/building/)

Constructs a URI from scratch using `URIBuilder`'s fluent API, chaining `set_scheme()`, `set_host()`, `append_path_segment()`, `add_query_param()`, and `set_fragment()`. Also demonstrates modifying an existing URI with `URIBuilder.from()` and error handling for invalid schemes via `URIBuildError`. Start here if you're new to the `uri` package.

#### [iri](networking/uri/iri/)

Parses a URI containing non-ASCII characters and demonstrates IRI/URI round-trip conversion with `IRIToURI` and `URIToIRI`. Covers IRI-aware percent-encoding with `IRIPercentEncode`, IRI normalization with `NormalizeIRI`, and cross-form equivalence checking with `IRIEquivalent`.

#### [template](networking/uri/template/)

Expands URI templates (RFC 6570) by parsing a template string with `URITemplate` and binding variables with `URITemplateVariables`. Demonstrates all variable types (strings, lists, pairs), multiple operator styles (simple, reserved `+`, fragment `#`, path `/`, query `?`), the explode modifier `*`, and the prefix modifier. Also shows error handling via `URITemplateParse`.

#### [form-urlencoded](networking/uri/form-urlencoded/)

Parses `application/x-www-form-urlencoded` strings into key-value pairs using `ParseFormURLEncoded` and `FormURLEncoded`. Demonstrates single-value lookup with `get()`, multi-value lookup with `get_all()` for duplicate keys, presence checking with `contains()`, iterating all pairs, and decoding of plus-as-space and percent-encoding. Also shows the `URI.query_params()` convenience wrapper for extracting query parameters from a parsed URI.

#### [template-builder](networking/uri/template-builder/)

One-shot URI template expansion using `URITemplateBuilder`'s fluent API, which combines template parsing and variable binding into a single chain. Demonstrates all three variable types (`set()`, `set_list()`, `set_pairs()`) and error handling for invalid templates.

## File I/O, Terminal, and Signals

### [files](io/files/)

Reads a file specified as a command-line argument and prints its path and contents line by line. Demonstrates capability-based file access with `FilePath`, `FileAuth`, and `FileCaps`, and resource cleanup with `with` blocks.

### [readline](io/readline/)

An interactive command-line prompt with tab completion and command history. Demonstrates the `term` package's `Readline` and `ReadlineNotify` interfaces, and `Promise`-based prompt control where rejecting the promise exits the loop.

### [signals](io/signals/)

Registers two handlers for SIGINT, raises the signal programmatically, and unsubscribes by returning false from the notifies. Demonstrates the `signals` package's capability-secured API: `SignalAuth` for authorization, `MakeHandleableSignal` for signal validation via constrained types, `SignalHandler` for subscription, and `SignalNotify` for callbacks. Shows that multiple handlers can subscribe to the same signal, what the runtime does and does not guarantee about delivery ordering, and that a `wait = true` handler keeps the program alive until it is disposed.

## Data Formats

### [json](data-formats/json/)

Demonstrates the `json` standard library package: building JSON documents with `JsonObject` and `JsonArray`, serializing any value with `JsonPrinter`, parsing JSON text with `JsonParser`, reading nested values with `JsonNav`, composable get/set/remove with `JsonLens`, and string-based queries with `JsonPath` including filters, slicing, and function extensions (`match`, `search`, `length`, `count`).

## C FFI

### [cshim](c-ffi/cshim/)

Calls C code that `ponyc` compiles itself: a `.c` shim placed next to the `.pony` files is discovered, compiled with the embedded clang, and linked into the program — no separate C build step or `use "lib:..."` directive. Demonstrates the `cdefine:` and `cincludedir:` use schemes for setting C preprocessor macros and include search paths per package. Start here if you're new to calling C from Pony.

### [ffi-callbacks](c-ffi/ffi-callbacks/)

Passes Pony functions as callbacks to C code using three mechanisms: bare functions with `@` annotation, bare lambdas with `@{...}` syntax, and partial application with the `~` operator. Each mechanism requires compiling and linking a companion C library.

### [ffi-struct](c-ffi/ffi-struct/)

Passes Pony structs to C functions, showing both `embed` fields (inline like C nested structs) and `var` fields (pointer-based). Demonstrates that Pony `struct` types have identical binary layout to their C counterparts for zero-copy interop.

## Testing and Benchmarking

### [pony_bench](testing/pony_bench/)

Runs microbenchmarks using the `pony_bench` package, reporting mean, median, and deviation. Demonstrates synchronous benchmarks with the `MicroBenchmark` trait, asynchronous benchmarks with `AsyncMicroBenchmark`, and `DoNotOptimise` to prevent dead code elimination.

### [pony_check](testing/pony_check/)

Demonstrates property-based testing with the `pony_check` package. Shows `Property1UnitTest` for defining properties, built-in and custom generators for producing test data, generator composition with `flat_map`, and async property testing over TCP using the `net/notifier` API.

## Benchmarks and Simulations

### [gups_basic](benchmarks/gups_basic/)

Measures GUPS (Giga Updates Per Second), a random-access memory performance metric, using multiple `Streamer` and `Updater` actors that perform random XOR updates on a large array. Demonstrates actor-based work distribution with configurable parallelism and completion tracking via countdown patterns.

### [gups_opt](benchmarks/gups_opt/)

An optimized variant of `gups_basic` that measures GUPS with array recycling for memory reuse. Demonstrates distributed computation across `Updater` actors, each managing a portion of the update table, with barrier synchronization for completion detection.

### [mandelbrot](benchmarks/mandelbrot/)

Plots the Mandelbrot set using divide-and-conquer parallelization across `Worker` actors, outputting a PBM (Portable Bitmap) image. Demonstrates parallel computation with result coordination, file I/O with seeking, and bit manipulation for compact pixel packing.

### [n-body](benchmarks/n-body/)

Simulates gravitational interaction among five planetary bodies (Sun, Jupiter, Saturn, Uranus, Neptune) using Newtonian physics. Demonstrates classes with multiple factory constructors, `F64` floating-point arithmetic, and pairwise force calculations with velocity and position integration.

## Dynamic Tracing

### [dtrace](tracing/dtrace/)

Example DTrace scripts for tracing Pony runtime behavior on macOS and FreeBSD, including GC events, actor scheduling, and message throughput. Demonstrates the Pony runtime's DTrace provider interface with probes for garbage collection, scheduling, and telemetry aggregation.

### [runtime_info](tracing/runtime_info/)

Queries and displays runtime statistics including actor heap memory, GC metrics, CPU time, and scheduler state. Demonstrates the `runtime_info` package's `ActorStats` and `SchedulerStats` with auth-gated access via `ActorStatsAuth` and `SchedulerStatsAuth`. Requires the compiler to be built with `use=runtimestats_messages`.

### [systemtap](tracing/systemtap/)

Example SystemTap scripts for tracing Pony runtime behavior on Linux, covering GC events, scheduling, and telemetry. Functionally equivalent to the `dtrace` examples but using SystemTap's probe syntax, requiring a Linux kernel with UPROBES support and the compiler built with `use=dtrace`.
