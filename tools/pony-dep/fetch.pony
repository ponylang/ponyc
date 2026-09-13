use "files"
use "net"
use "time"
use http = "http_client"
use uri_pkg = "uri"

actor Fetch is (http.HTTPClientConnectionActor & http.RedirectFollowerNotify)
  """
  Downloads a `.par` archive from a URL and extracts it to a target directory.

  Supports both HTTP and HTTPS with automatic redirect following. The archive
  is buffered in memory, decoded, and extracted to the output directory.
  Reports success or failure through a `FetchNotify`. Fails if no data
  arrives for 60 seconds during an active download.
  """
  let _env: Env
  let _notify: FetchNotify
  let _output_dir: String
  let _timers: Timers = Timers
  var _redirect: http.RedirectFollower = http.RedirectFollower.none()
  var _collector: http.ResponseCollector = http.ResponseCollector
  var _request_path: String val = "/"
  var _timeout_timer: (Timer tag | None) = None
  var _timed_out: Bool = false

  new create(env: Env, notify: FetchNotify, url: String, output_dir: String) =>
    _env = env
    _notify = notify
    _output_dir = output_dir

    let parsed =
      match \exhaustive\ uri_pkg.ParseURI(url)
      | let u: uri_pkg.URI val => u
      | let _: uri_pkg.URIParseError val =>
        _notify.fetch_failed(
          FetchError(FetchInvalidURL, "invalid URL: " + url))
        return
      end

    let scheme: String val =
      match \exhaustive\ parsed.scheme
      | let s: String => s.lower()
      | None =>
        _notify.fetch_failed(
          FetchError(FetchInvalidURL, "URL has no scheme: " + url))
        return
      end

    if (scheme != "http") and (scheme != "https") then
      _notify.fetch_failed(FetchError(
        FetchInvalidURL,
        "unsupported URL scheme '" + scheme + "'"))
      return
    end

    let authority =
      match \exhaustive\ parsed.authority
      | let a: uri_pkg.URIAuthority => a
      | None =>
        _notify.fetch_failed(
          FetchError(FetchInvalidURL, "URL has no host: " + url))
        return
      end

    let host = authority.host
    if host.size() == 0 then
      _notify.fetch_failed(
        FetchError(FetchInvalidURL, "URL has empty host: " + url))
      return
    end

    let secure = scheme == "https"
    let port =
      match \exhaustive\ authority.port
      | let p: U16 => p.string()
      | None => if secure then "443" else "80" end
      end

    _request_path =
      if parsed.path.size() == 0 then "/" else parsed.path end
    match parsed.query
    | let q: String => _request_path = _request_path + "?" + q
    end

    let tcp_auth = TCPConnectAuth(env.root)
    let config = http.ClientConnectionConfig

    let conn =
      if secure then
        try
          let ssl_ctx =
            recover val
              SSLContext .> set_default_verify_paths()?
            end
          http.HTTPClientConnection.ssl(
            tcp_auth, ssl_ctx, host, port, this, config)
        else
          _notify.fetch_failed(FetchError(
            FetchConnectionFailed, "SSL initialization failed"))
          return
        end
      else
        http.HTTPClientConnection(tcp_auth, host, port, this, config)
      end

    let factory =
      {ref(target: uri_pkg.URI val)
        (tcp_auth, config, client = this)
        : http.HTTPClientConnection
      =>
        let o = http.Origin.from_uri(target)
        if o.host.size() == 0 then
          return http.HTTPClientConnection.none()
        end
        if o.secure then
          try
            let ssl_ctx =
              recover val
                SSLContext .> set_default_verify_paths()?
              end
            http.HTTPClientConnection.ssl(
              tcp_auth, ssl_ctx, o.host, o.port, client, config)
          else
            http.HTTPClientConnection.none()
          end
        else
          http.HTTPClientConnection(
            tcp_auth, o.host, o.port, client, config)
        end
      }

    _redirect =
      http.RedirectFollower(
        conn, this, factory, 5, http.Origin(secure, host, port))

  fun ref _http_client_connection(): http.HTTPClientConnection =>
    _redirect.connection()

  fun ref _start_idle_timer() =>
    _cancel_idle_timer()
    let timer =
      Timer(_FetchTimeoutNotify(this), 60_000_000_000, 0)
    _timeout_timer = timer
    _timers(consume timer)

  fun ref _cancel_idle_timer() =>
    match _timeout_timer
    | let t: Timer tag => _timers.cancel(t)
    end
    _timeout_timer = None

  be _idle_timeout() =>
    if _timed_out then return end
    _timed_out = true
    _cancel_idle_timer()
    _notify.fetch_failed(FetchError(
      FetchIdleTimeout, "no data received for 60 seconds"))
    _redirect.connection().close()
    _timers.dispose()

  fun ref on_connected() =>
    let request = http.Request.get(_request_path).build()
    match \exhaustive\ _redirect.send_request(request)
    | http.SendRequestOK =>
      _start_idle_timer()
    | http.ConnectionClosed =>
      _notify.fetch_failed(FetchError(
        FetchConnectionFailed,
        "connection closed before request sent"))
    | http.ResponsePending =>
      _Unreachable()
    end

  fun ref on_response(response: http.Response val) =>
    _start_idle_timer()
    _collector = http.ResponseCollector
    _collector.set_response(response)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _start_idle_timer()
    _collector.add_chunk(data)

  fun ref on_response_complete() =>
    _cancel_idle_timer()
    _timers.dispose()

    try
      let response = _collector.build()?

      if (response.status < 200) or (response.status >= 300) then
        _notify.fetch_failed(FetchError(
          FetchHTTPError,
          "HTTP " + response.status.string() + " " + response.reason))
        _redirect.connection().close()
        return
      end

      let file_auth = FileAuth(_env.root)
      let dir_path = FilePath(file_auth, _output_dir)
      if not dir_path.mkdir() then
        try
          if not FileInfo(dir_path)?.directory then
            _notify.fetch_failed(FetchError(
              FetchExtractionFailed,
              "output path is not a directory: " + _output_dir))
            _redirect.connection().close()
            return
          end
        else
          _notify.fetch_failed(FetchError(
            FetchExtractionFailed,
            "cannot create output directory: " + _output_dir))
          _redirect.connection().close()
          return
        end
      end

      let dir =
        try
          Directory(dir_path)?
        else
          _notify.fetch_failed(FetchError(
            FetchExtractionFailed,
            "cannot open output directory: " + _output_dir))
          _redirect.connection().close()
          return
        end

      try
        ArchiveDecoder.from_bytes(response.body, dir)?
      else
        _notify.fetch_failed(
          FetchError(FetchExtractionFailed, "archive extraction failed"))
        _redirect.connection().close()
        return
      end

      _notify.fetch_succeeded()
      _redirect.connection().close()
    else
      _Unreachable()
    end

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    """
    Maps the connection failure reason to a human-readable message and
    reports it through the notify.
    """
    _cancel_idle_timer()
    _timers.dispose()
    let msg =
      match \exhaustive\ reason
      | ConnectionFailedDNS => "DNS resolution failed"
      | ConnectionFailedTCP => "TCP connection failed"
      | ConnectionFailedSSL => "SSL handshake failed"
      | ConnectionFailedTimeout => "connection timed out"
      | ConnectionFailedTimerError => "connection timer error"
      end
    _notify.fetch_failed(FetchError(FetchConnectionFailed, msg))

  fun ref on_redirect_error(err: http.RedirectError) =>
    """
    Maps the redirect error to a human-readable message and reports it
    through the notify.
    """
    _cancel_idle_timer()
    _timers.dispose()
    let msg =
      match \exhaustive\ err
      | http.TooManyRedirects => "too many redirects"
      | http.MissingLocation => "redirect missing Location header"
      | http.InvalidLocation => "redirect has invalid Location"
      | http.InsecureRedirect => "HTTPS to HTTP redirect refused"
      end
    _notify.fetch_failed(FetchError(FetchConnectionFailed, msg))
    _redirect.connection().close()

  fun ref on_parse_error(err: http.ParseError) =>
    _cancel_idle_timer()
    _timers.dispose()
    _notify.fetch_failed(
      FetchError(FetchConnectionFailed, "HTTP parse error"))

class iso _FetchTimeoutNotify is TimerNotify
  let _fetch: Fetch

  new iso create(fetch: Fetch) =>
    _fetch = fetch

  fun ref apply(timer: Timer, count: U64): Bool =>
    _fetch._idle_timeout()
    false

  fun ref cancel(timer: Timer) =>
    None
