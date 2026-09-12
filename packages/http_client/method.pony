interface val Method is (Equatable[Method] & Stringable)
  """
  An HTTP request method (RFC 7231).
  """

primitive GET is Method
  """
  HTTP GET method.
  """
  fun string(): String iso^ => "GET".clone()
  fun eq(that: Method): Bool => that is this

primitive HEAD is Method
  """
  HTTP HEAD method.
  """
  fun string(): String iso^ => "HEAD".clone()
  fun eq(that: Method): Bool => that is this

primitive POST is Method
  """
  HTTP POST method.
  """
  fun string(): String iso^ => "POST".clone()
  fun eq(that: Method): Bool => that is this

primitive PUT is Method
  """
  HTTP PUT method.
  """
  fun string(): String iso^ => "PUT".clone()
  fun eq(that: Method): Bool => that is this

primitive DELETE is Method
  """
  HTTP DELETE method.
  """
  fun string(): String iso^ => "DELETE".clone()
  fun eq(that: Method): Bool => that is this

primitive CONNECT is Method
  """
  HTTP CONNECT method.
  """
  fun string(): String iso^ => "CONNECT".clone()
  fun eq(that: Method): Bool => that is this

primitive OPTIONS is Method
  """
  HTTP OPTIONS method.
  """
  fun string(): String iso^ => "OPTIONS".clone()
  fun eq(that: Method): Bool => that is this

primitive TRACE is Method
  """
  HTTP TRACE method.
  """
  fun string(): String iso^ => "TRACE".clone()
  fun eq(that: Method): Bool => that is this

primitive PATCH is Method
  """
  HTTP PATCH method.
  """
  fun string(): String iso^ => "PATCH".clone()
  fun eq(that: Method): Bool => that is this

primitive Methods
  """
  Parse HTTP method strings and enumerate known methods.
  """

  fun parse(data: String): (Method | None) =>
    """
    Parse a string into an HTTP method, or None if not recognized.
    """
    match data
    | "GET" => GET
    | "HEAD" => HEAD
    | "POST" => POST
    | "PUT" => PUT
    | "DELETE" => DELETE
    | "CONNECT" => CONNECT
    | "OPTIONS" => OPTIONS
    | "TRACE" => TRACE
    | "PATCH" => PATCH
    else
      None
    end

  fun valid(): Array[Method] val =>
    """
    Return all standard HTTP methods.
    """
    [GET; HEAD; POST; PUT; DELETE; CONNECT; OPTIONS; TRACE; PATCH]
