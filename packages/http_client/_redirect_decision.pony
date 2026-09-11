use uri = "uri"

primitive _NotARedirect
  """
  The response is not a followed redirect: a non-redirect status, or a
  redirect status not acted on (300, 304, 305). The response is delivered
  normally through `on_response`.
  """

primitive _RedirectDecision
  """
  Decide what to do with a response on a redirect-following connection:
  return a validated `Redirect` to follow, a `RedirectError` to refuse, or
  `_NotARedirect` to deliver the response normally.

  Pure: a function of the request that was sent, the origin it was sent to,
  the response, and how many hops may still be followed. It applies the whole
  redirect policy — which statuses redirect, `Location` resolution, the
  `https`-to-`http` downgrade refusal, cross-origin credential stripping, and
  method/body rewriting — with no socket, actor, or I/O.
  """
  fun apply(
    sent: HTTPRequest val,
    origin: Origin,
    response: Response val,
    max_redirects: USize)
    : (Redirect | RedirectError | _NotARedirect)
  =>
    if not _is_redirect_status(response.status) then
      return _NotARedirect
    end

    let location =
      match \exhaustive\ response.headers.get("location")
      | let l: String => l
      | None => return MissingLocation
      end

    let target =
      match \exhaustive\ _resolve_target(origin, sent.path, location)
      | let u: uri.URI val => u
      | let e: RedirectError => return e
      end

    let target_ssl =
      match target.scheme
      | let s: String => s.lower() == "https"
      else false
      end

    if origin.secure and (not target_ssl) then
      return InsecureRedirect
    end

    if max_redirects == 0 then
      return TooManyRedirects
    end

    let cross_origin = not (origin == Origin.from_uri(target))
    (let new_method, let drop_body) = _rewrite(sent.method, response.status)
    let new_headers = _rewrite_headers(sent.headers, cross_origin, drop_body)
    let new_body: (Array[U8] val | None) =
      if drop_body then None else sent.body end

    let path = if target.path == "" then "/" else target.path end
    let request_path: String val =
      match \exhaustive\ target.query
      | let q: String => path + "?" + q
      | None => path
      end

    Redirect._create(
      response.status,
      target,
      HTTPRequest(new_method, request_path, new_headers, new_body),
      max_redirects - 1)

  fun _is_redirect_status(status: U16): Bool =>
    (status == 301) or (status == 302) or (status == 303) or
      (status == 307) or (status == 308)

  fun _resolve_target(
    origin: Origin,
    request_target: String val,
    location: String val)
    : (uri.URI val | RedirectError)
  =>
    let base_url: String val =
      recover val
        String
          .> append(if origin.secure then "https" else "http" end)
          .> append("://")
          .> append(origin.host)
          .> append(":")
          .> append(origin.port)
          .> append(request_target)
      end
    let base =
      match \exhaustive\ uri.ParseURI(base_url)
      | let u: uri.URI => u
      | let _: uri.URIParseError => return InvalidLocation
      end
    let reference =
      match \exhaustive\ uri.ParseURI(location)
      | let u: uri.URI => u
      | let _: uri.URIParseError => return InvalidLocation
      end
    let resolved =
      match \exhaustive\ uri.ResolveURI(base, reference)
      | let u: uri.URI => u
      | let _: uri.ResolveURIError => return InvalidLocation
      end

    // Reject schemes that cannot be followed (data:, file:, ftp:, ...).
    match resolved.scheme
    | let s: String =>
      let lower = s.lower()
      if (lower != "http") and (lower != "https") then
        return InvalidLocation
      end
    else
      return InvalidLocation
    end

    // An HTTP URL must have an authority with a non-empty host and no
    // userinfo (RFC 7230 section 2.7.1).
    match resolved.authority
    | let a: uri.URIAuthority =>
      if a.host.size() == 0 then return InvalidLocation end
      if not (a.userinfo is None) then return InvalidLocation end
    else
      return InvalidLocation
    end

    resolved

  fun _rewrite(method: Method, status: U16): (Method, Bool) =>
    // 307/308 preserve method and body. 303 becomes GET (HEAD stays HEAD).
    // 301/302 on POST become GET, matching long-standing client behavior;
    // other methods are preserved.
    match status
    | 303 => if method is HEAD then (HEAD, false) else (GET, true) end
    | 301 | 302 => if method is POST then (GET, true) else (method, false) end
    else
      (method, false)
    end

  fun _rewrite_headers(
    headers: Headers val,
    cross_origin: Bool,
    drop_body: Bool)
    : Headers val
  =>
    if (not cross_origin) and (not drop_body) then return headers end
    recover val
      let out = Headers
      for (name, value) in headers.values() do
        // Headers stores names lowercased, so these compare directly.
        if cross_origin and _is_sensitive(name) then continue end
        if drop_body and _is_body_header(name) then continue end
        out.add(name, value)
      end
      out
    end

  fun _is_sensitive(name: String): Bool =>
    (name == "authorization") or (name == "cookie") or
      (name == "proxy-authorization") or (name == "host") or
      (name == "referer")

  fun _is_body_header(name: String): Bool =>
    (name == "content-length") or (name == "content-type") or
      (name == "content-encoding") or (name == "transfer-encoding")
