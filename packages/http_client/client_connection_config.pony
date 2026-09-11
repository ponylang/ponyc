use "net"

primitive _DefaultIdleTimeout
  """
  60-second idle timeout, the default for HTTP client connections.
  """
  fun apply(): (IdleTimeout | None) =>
    match MakeIdleTimeout(60_000)
    | let t: IdleTimeout => t
    else
      _Unreachable()
      None
    end

class val ClientConnectionConfig
  """
  Configuration for an HTTP client connection.

  Parser limits control the maximum size of response components. Idle timeout
  controls how long the connection can sit without I/O activity before the
  library closes it. Connection timeout bounds how long the initial connection
  handshake is allowed to take. Read buffer size bounds how much data the
  connection reads per scheduler turn. `from` specifies the local bind address
  (empty string means any interface).

  ```pony
  // All defaults (60-second idle timeout, 10 MB max body, 16 KB read buffer)
  ClientConnectionConfig

  // Custom idle timeout via MakeIdleTimeout (milliseconds)
  let timeout = match MakeIdleTimeout(30_000)
  | let t: IdleTimeout => t
  end
  ClientConnectionConfig(where
    max_body_size' = 52_428_800,  // 50 MB
    idle_timeout' = timeout)

  // Disable idle timeout
  ClientConnectionConfig(where idle_timeout' = None)

  // Set a 5-second connection timeout
  let ct = match MakeConnectionTimeout(5_000)
  | let t: ConnectionTimeout => t
  end
  ClientConnectionConfig(where connection_timeout' = ct)

  // Smaller read buffer to limit work per turn
  let rbs = match MakeReadBufferSize(4096)
  | let r: ReadBufferSize => r
  end
  ClientConnectionConfig(where read_buffer_size' = rbs)
  ```
  """
  let max_status_line_size: USize
  let max_header_size: USize
  let max_chunk_header_size: USize
  let max_body_size: USize
  let idle_timeout: (IdleTimeout | None)
  let connection_timeout: (ConnectionTimeout | None)
  let read_buffer_size: ReadBufferSize
  let from: String

  new val create(
    max_status_line_size': USize = 8192,
    max_header_size': USize = 8192,
    max_chunk_header_size': USize = 128,
    max_body_size': USize = 10_485_760,
    idle_timeout': (IdleTimeout | None) = _DefaultIdleTimeout(),
    connection_timeout': (ConnectionTimeout | None) = None,
    read_buffer_size': ReadBufferSize = DefaultReadBufferSize(),
    from': String = "")
  =>
    """
    Create client connection configuration.

    Parser limits default to sensible values. `idle_timeout'` is an
    `IdleTimeout` (milliseconds) or `None` to disable idle timeout. Defaults
    to 60 seconds. `connection_timeout'` is a `ConnectionTimeout`
    (milliseconds) or `None` to disable connection timeout. Defaults to
    `None`. `read_buffer_size'` bounds how much data the connection reads per
    scheduler turn — a smaller buffer means less work per turn at the cost of
    more turns to deliver a large response. Defaults to 16 KB (net's
    default). `from'` specifies the local bind address (empty string means any
    interface).
    """
    max_status_line_size = max_status_line_size'
    max_header_size = max_header_size'
    max_chunk_header_size = max_chunk_header_size'
    max_body_size = max_body_size'
    idle_timeout = idle_timeout'
    connection_timeout = connection_timeout'
    read_buffer_size = read_buffer_size'
    from = from'

  fun _parser_config(): _ParserConfig val =>
    """
    Create a parser config from the parser limit fields.
    """
    _ParserConfig(
      max_status_line_size,
      max_header_size,
      max_chunk_header_size,
      max_body_size)
