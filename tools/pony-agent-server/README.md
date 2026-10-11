# pony-agent-server

A query server for AI coding agents that need Pony type information while editing code. It compiles a package and answers questions about types, definitions, capabilities, and structure.

The query interface is experimental and subject to change.

## Usage

```text
pony-agent-server <package-directory> [-D define]...
```

The server compiles the package on startup, then reads one JSON query per line from stdin and writes one JSON response per line to stdout. Diagnostic messages go to stderr.

Pass `-D` flags the same way you would pass them to `ponyc`. For example, `-D openssl_3.0.x` for OpenSSL 3.x.

Run `--queries` to print a summary of all queries and their input format.

## Protocol

Every query is a JSON object with a `"query"` field. An optional `"id"` field (string or integer) is echoed back in the response.

On success, the response contains the query-specific fields described below. On error, the response contains an `"error"` field with a message.

```json
{"query":"status", "id":1}
{"id":1, "status":"ready"}
```

## Queries

### inspect

Type and definition of the AST node at a source position.

```json
{"query":"inspect", "file":"/path/to/file.pony", "line":10, "column":5}
```

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | The node's type as the compiler resolved it |
| `node_kind` | string | AST node kind (e.g. `"letref"`, `"call"`) |
| `line`, `column` | integer | Position of the node |
| `definition` | object | Where the symbol is defined: `file`, `line`, `column`, `kind`, `declared_type` |
| `enclosing` | object | Enclosing context: `method`, `receiver_cap`, `type` |
| `in_recover` | boolean | Whether the position is inside a `recover` block |
| `sendable` | boolean | Whether the type is sendable (iso, val, or tag) |

### scope

Every name visible at a source position, with recover-block accessibility.

```json
{"query":"scope", "file":"/path/to/file.pony", "line":10, "column":5}
```

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `in_recover` | boolean | Whether the position is inside a `recover` block |
| `entries` | array | Each entry has `name`, `type`, `kind`, `accessible`, and optionally `reason` |

When `accessible` is false, `reason` explains why (typically because a non-sendable reference is blocked by a recover expression).

### exports

Public types exported by a package.

```json
{"query":"exports", "package":"collections"}
```

The package can be a qualified name like `"collections"` or an absolute path.

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `package_path` | string | Resolved path to the package |
| `entries` | array | Each entry has `name`, `kind` (class, actor, primitive, etc.), and optionally `docstring` |

### type_api

Methods callable on a type at a given capability. There are two ways to query: by type name with an explicit capability, or by source position (the type and capability are read from the compiler's AST).

By name:

```json
{"query":"type_api", "type":"Array[String]", "cap":"ref"}
```

By position:

```json
{"query":"type_api", "file":"/path/to/file.pony", "line":10, "column":5}
```

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | The type name or expression |
| `kind` | string | Entity kind: `"class"`, `"actor"`, `"primitive"`, `"trait"`, `"interface"`, `"struct"`, or `"union"` |
| `callable` | array | Methods callable at this capability |
| `not_callable` | array | Methods not callable, each with a `method` and `reason` |
| `constructors` | array | Available constructors |
| `traits` | array | Traits and interfaces the type provides |

Each method object has: `name`, `receiver` (capability), `params` (array of `{name, type, has_default}`), `return`, `partial` (boolean), `kind` (`"fun"`, `"be"`, or `"new"`), and `source_type` (the type that defined the method).

### check

Subtype check between two types using the compiler's subtype checker.

```json
{"query":"check", "source":"Collector ref", "target":"Describable ref"}
```

Types are a name optionally followed by a capability. Without a capability, the type's default is used (ref for classes, val for primitives, tag for actors).

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `compatible` | boolean | Whether the source is a subtype of the target |
| `source_type` | string | Source type with resolved capability |
| `target_type` | string | Target type with resolved capability |
| `reason` | string | Explanation of the result |
| `suggestions` | array | When incompatible, suggestions for making it work |

### implementors

Types that implement a given trait or interface.

```json
{"query":"implementors", "type":"Describable"}
```

The target must be a trait or interface.

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | The queried trait or interface name |
| `nominal` | array | Types that explicitly declare the trait/interface in their provides list |
| `structural` | array | Types that match the interface's method signatures without declaring it (interfaces only; traits require nominal declaration) |

### errors

Compilation errors from the last compile or reload.

```json
{"query":"errors"}
```

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `errors` | array | Each error has `message`, and optionally `file`, `line`, `column` |

### reload

Recompile the package. The server recompiles from the same directory it was started with and replaces the in-memory program on success. Queries issued after a failed reload get the previous successful compilation.

```json
{"query":"reload"}
```

The response is the same as the initial compile: `{"status":"ready"}` on success, or `{"status":"error", "errors":[...]}` on failure.

### status

Whether the server has a compiled program ready for queries.

```json
{"query":"status"}
```

Response fields:

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | `"ready"` or `"not_compiled"` |
