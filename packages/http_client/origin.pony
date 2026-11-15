use uri = "uri"

class val Origin is (Equatable[Origin] & Stringable)
  """
  The origin of a URL: whether it is secure (https), the host, and the port.

  Two requests share an origin when all three match. Host comparison is
  case-insensitive; the host is lowercased and IPv6 brackets are stripped on
  construction. Redirect header stripping uses origin equality — credentials
  are removed when a hop crosses to a different origin.
  """
  let secure: Bool
  let host: String
  let port: String

  new val create(secure': Bool, host': String, port': String) =>
    secure = secure'
    host =
      if try host'(0)? == '[' else false end then
        host'.substring(1, (host'.size() - 1).isize()).lower()
      else
        host'.lower()
      end
    port = port'

  new val from_uri(url: uri.URI val) =>
    """
    Extract the origin from a parsed URI. The host is lowercased and IPv6
    brackets are stripped; the port defaults to 443 (https) or 80 (http)
    when the URI omits it. When the URI has no authority, host is empty.
    """
    secure =
      match url.scheme
      | let s: String => s.lower() == "https"
      else false
      end
    match url.authority
    | let a: uri.URIAuthority =>
      let h = a.host
      host =
        if try h(0)? == '[' else false end then
          h.substring(1, (h.size() - 1).isize()).lower()
        else
          h.lower()
        end
      port =
        match \exhaustive\ a.port
        | let p: U16 => p.string()
        | None => if secure then "443" else "80" end
        end
    else
      host = ""
      port = if secure then "443" else "80" end
    end

  fun eq(that: Origin box): Bool =>
    (secure == that.secure) and (host == that.host) and (port == that.port)

  fun _url_host(): String =>
    """
    The host formatted for use in a URI or Host header: IPv6 addresses are
    wrapped in brackets per RFC 3986 section 3.2.2.
    """
    if host.contains(":") then "[" + host + "]" else host end

  fun string(): String =>
    recover val
      String
        .> append(if secure then "https" else "http" end)
        .> append("://")
        .> append(_url_host())
        .> append(":")
        .> append(port)
    end
