use uri = "uri"
use "pony_check"
use "pony_test"

primitive \nodoc\ _RedirectTestKit
  fun response(status: U16, location: (String | None) = None): Response val =>
    let headers =
      recover val
        let h = Headers
        match location
        | let l: String => h.set("location", l)
        end
        h
      end
    Response(HTTP11, status, "", headers)

  fun request(
    method: Method = GET,
    path: String = "/dl/file",
    headers: Headers val = recover val Headers end,
    body: (Array[U8] val | None) = None)
    : HTTPRequest val
  =>
    HTTPRequest(method, path, headers, body)

  fun github_origin(): Origin =>
    Origin(true, "github.com", "443")

  fun decide(
    sent: HTTPRequest val,
    resp: Response val,
    max_redirects: USize = 5)
    : (Redirect | RedirectError | _NotARedirect)
  =>
    _RedirectDecision(sent, github_origin(), resp, max_redirects)

  fun target_host(r: Redirect): String =>
    match r.target().authority
    | let a: uri.URIAuthority => a.host
    else ""
    end

class \nodoc\ iso _TestRedirectCrossOriginStripsCredentials is UnitTest
  """
  The GitHub download case: a 302 to a different host drops authorization,
  cookie, proxy-authorization, host, and referer from the replayed request.
  """
  fun name(): String => "redirect/cross_origin_strips_credentials"

  fun apply(h: TestHelper) =>
    let sent =
      _RedirectTestKit.request(
        GET,
        "/dl/file",
        recover val
          Headers
            .> set("authorization", "Bearer secret")
            .> set("cookie", "session=abc")
            .> set("proxy-authorization", "Basic xyz")
            .> set("host", "github.com")
            .> set("referer", "https://github.com/dl/file")
            .> set("user-agent", "ponyup")
        end)
    let response =
      _RedirectTestKit.response(
        302, "https://cdn.example.com/signed/file?sig=123")

    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      let hdrs = r.request().headers
      h.assert_true(
        hdrs.get("authorization") is None,
        "authorization must be stripped cross-origin")
      h.assert_true(hdrs.get("cookie") is None, "cookie must be stripped")
      h.assert_true(
        hdrs.get("proxy-authorization") is None,
        "proxy-authorization must be stripped")
      h.assert_true(hdrs.get("host") is None, "host must be stripped")
      h.assert_true(hdrs.get("referer") is None, "referer must be stripped")
      match \exhaustive\ hdrs.get("user-agent")
      | let v: String => h.assert_eq[String val]("ponyup", v)
      | None => h.fail("user-agent must survive")
      end
      h.assert_eq[String val](
        "cdn.example.com", _RedirectTestKit.target_host(r))
      h.assert_eq[String val](
        "/signed/file?sig=123", r.request().path)
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect, got _NotARedirect")
    end

class \nodoc\ iso _TestRedirectSameOriginKeepsCredentials is UnitTest
  """A same-origin hop keeps authorization — only cross-origin strips it."""
  fun name(): String => "redirect/same_origin_keeps_credentials"

  fun apply(h: TestHelper) =>
    let sent =
      _RedirectTestKit.request(
        GET,
        "/old",
        recover val Headers .> set("authorization", "Bearer secret") end)
    let response = _RedirectTestKit.response(302, "https://github.com/new")

    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      match \exhaustive\ r.request().headers.get("authorization")
      | let v: String => h.assert_eq[String val]("Bearer secret", v)
      | None => h.fail("authorization must survive a same-origin hop")
      end
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirectRefusesDowngrade is UnitTest
  """An https origin redirecting to http is refused."""
  fun name(): String => "redirect/refuses_downgrade"

  fun apply(h: TestHelper) =>
    let response = _RedirectTestKit.response(302, "http://github.com/new")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response)
    | InsecureRedirect => None
    | let r: Redirect => h.fail("must refuse https->http, got Redirect")
    | let _: RedirectError =>
      h.fail("expected InsecureRedirect, got a different RedirectError")
    | _NotARedirect => h.fail("expected InsecureRedirect")
    end

class \nodoc\ iso _TestRedirectMissingLocation is UnitTest
  """A redirect status with no Location header is an error."""
  fun name(): String => "redirect/missing_location"

  fun apply(h: TestHelper) =>
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), _RedirectTestKit.response(302))
    | MissingLocation => None
    | let r: Redirect => h.fail("expected MissingLocation, got Redirect")
    | let _: RedirectError =>
      h.fail("expected MissingLocation, got a different RedirectError")
    | _NotARedirect => h.fail("expected MissingLocation")
    end

class \nodoc\ iso _TestRedirectLimitExhausted is UnitTest
  """A max of zero recognizes a redirect but refuses to follow it."""
  fun name(): String => "redirect/limit_exhausted"

  fun apply(h: TestHelper) =>
    let response = _RedirectTestKit.response(302, "https://cdn.example.com/x")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response, 0)
    | TooManyRedirects => None
    | let r: Redirect => h.fail("max 0 must refuse, got Redirect")
    | let _: RedirectError =>
      h.fail("expected TooManyRedirects, got a different RedirectError")
    | _NotARedirect => h.fail("expected TooManyRedirects")
    end

class \nodoc\ iso _TestRedirectRemainingDecrements is UnitTest
  """A followed hop carries a count one smaller than the one that made it."""
  fun name(): String => "redirect/remaining_decrements"

  fun apply(h: TestHelper) =>
    let response = _RedirectTestKit.response(302, "https://cdn.example.com/x")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response, 3)
    | let r: Redirect => h.assert_eq[USize](2, r.remaining())
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirect303PostBecomesGet is UnitTest
  """303 rewrites POST to GET and drops the body."""
  fun name(): String => "redirect/303_post_becomes_get"

  fun apply(h: TestHelper) =>
    let sent =
      _RedirectTestKit.request(
        POST,
        "/submit",
        recover val
          Headers
            .> set("content-type", "application/json")
            .> set("content-length", "2")
            .> set("content-encoding", "gzip")
            .> set("transfer-encoding", "chunked")
        end,
        recover val [as U8: '{'; '}'] end)
    let response = _RedirectTestKit.response(303, "https://github.com/result")
    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      h.assert_true(r.request().method is GET, "303 must rewrite to GET")
      h.assert_true(r.request().body is None, "303 must drop the body")
      h.assert_true(
        r.request().headers.get("content-type") is None,
        "content-type must go with the body")
      h.assert_true(
        r.request().headers.get("content-length") is None,
        "content-length must go with the body")
      h.assert_true(
        r.request().headers.get("content-encoding") is None,
        "content-encoding must go with the body")
      h.assert_true(
        r.request().headers.get("transfer-encoding") is None,
        "transfer-encoding must go with the body")
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirect307PreservesMethodAndBody is UnitTest
  """307 preserves both the method and the body."""
  fun name(): String => "redirect/307_preserves_method_and_body"

  fun apply(h: TestHelper) =>
    let body = recover val [as U8: 'h'; 'i'] end
    let sent =
      _RedirectTestKit.request(
        POST,
        "/submit",
        recover val Headers .> set("content-length", "2") end,
        body)
    let response = _RedirectTestKit.response(307, "https://github.com/again")
    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      h.assert_true(r.request().method is POST, "307 must preserve POST")
      match \exhaustive\ r.request().body
      | let b: Array[U8] val => h.assert_eq[USize](2, b.size())
      | None => h.fail("307 must preserve the body")
      end
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirect301PostBecomesGet is UnitTest
  """301 on POST becomes GET; 301 on GET stays GET."""
  fun name(): String => "redirect/301_post_becomes_get"

  fun apply(h: TestHelper) =>
    let response = _RedirectTestKit.response(301, "https://github.com/q")

    let post =
      _RedirectTestKit.request(
        POST, "/p", recover val Headers end, recover val [as U8: 'x'] end)
    match _RedirectTestKit.decide(post, response)
    | let r: Redirect =>
      h.assert_true(r.request().method is GET, "301 POST must become GET")
    else h.fail("expected Redirect for 301 POST")
    end

    match _RedirectTestKit.decide(_RedirectTestKit.request(GET, "/p"), response)
    | let r: Redirect =>
      h.assert_true(r.request().method is GET, "301 GET stays GET")
    else h.fail("expected Redirect for 301 GET")
    end

class \nodoc\ iso _TestRedirect302PostBecomesGet is UnitTest
  """302 on POST becomes GET; 302 on GET stays GET."""
  fun name(): String => "redirect/302_post_becomes_get"

  fun apply(h: TestHelper) =>
    let response = _RedirectTestKit.response(302, "https://github.com/q")

    let post =
      _RedirectTestKit.request(
        POST, "/p", recover val Headers end, recover val [as U8: 'x'] end)
    match _RedirectTestKit.decide(post, response)
    | let r: Redirect =>
      h.assert_true(r.request().method is GET, "302 POST must become GET")
    else h.fail("expected Redirect for 302 POST")
    end

    match _RedirectTestKit.decide(_RedirectTestKit.request(GET, "/p"), response)
    | let r: Redirect =>
      h.assert_true(r.request().method is GET, "302 GET stays GET")
    else h.fail("expected Redirect for 302 GET")
    end

class \nodoc\ iso _TestRedirect308PreservesMethodAndBody is UnitTest
  """308 preserves both the method and the body."""
  fun name(): String => "redirect/308_preserves_method_and_body"

  fun apply(h: TestHelper) =>
    let body = recover val [as U8: 'h'; 'i'] end
    let sent =
      _RedirectTestKit.request(
        POST,
        "/submit",
        recover val Headers .> set("content-length", "2") end,
        body)
    let response = _RedirectTestKit.response(308, "https://github.com/again")
    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      h.assert_true(r.request().method is POST, "308 must preserve POST")
      match \exhaustive\ r.request().body
      | let b: Array[U8] val => h.assert_eq[USize](2, b.size())
      | None => h.fail("308 must preserve the body")
      end
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirectNonRedirectStatus is UnitTest
  """A 200 is not a redirect. 304 and 305 are not followed either."""
  fun name(): String => "redirect/non_redirect_status"

  fun apply(h: TestHelper) =>
    for status in [as U16: 200; 204; 304; 305].values() do
      let response =
        _RedirectTestKit.response(status, "https://github.com/x")
      match _RedirectTestKit.decide(_RedirectTestKit.request(), response)
      | _NotARedirect => None
      else h.fail("status " + status.string() + " must be _NotARedirect")
      end
    end

class \nodoc\ iso _TestRedirectRelativeLocation is UnitTest
  """A relative Location resolves against the request base."""
  fun name(): String => "redirect/relative_location"

  fun apply(h: TestHelper) =>
    let sent = _RedirectTestKit.request(GET, "/a/b/c")
    let response = _RedirectTestKit.response(302, "../x")
    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      h.assert_eq[String val](
        "github.com", _RedirectTestKit.target_host(r))
      h.assert_eq[String val]("/a/x", r.target().path)
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirectUnsupportedSchemeLocation is UnitTest
  """
  A Location carrying an unfollowable scheme is InvalidLocation, not
  a same-origin path — it must not be reinterpreted as relative.
  """
  fun name(): String => "redirect/unsupported_scheme_location"

  fun apply(h: TestHelper) =>
    let locations =
      [ "data:text/html,x"; "file:///etc/passwd"; "ftp://host/x"; "mailto:a@b" ]
    for loc in locations.values() do
      let response = _RedirectTestKit.response(302, loc)
      match \exhaustive\ _RedirectTestKit.decide(
        _RedirectTestKit.request(), response)
      | InvalidLocation => None
      | let r: Redirect =>
        h.fail(
          "must reject unfollowable scheme in " + loc +
            ", got target host " + _RedirectTestKit.target_host(r))
      | let _: RedirectError =>
        h.fail(
          "expected InvalidLocation for " + loc +
            ", got a different RedirectError")
      | _NotARedirect => h.fail("expected InvalidLocation for " + loc)
      end
    end

class \nodoc\ iso _TestRedirectUserinfoRejected is UnitTest
  """A Location with userinfo is InvalidLocation per RFC 7230 section 2.7.1."""
  fun name(): String => "redirect/userinfo_rejected"

  fun apply(h: TestHelper) =>
    let response =
      _RedirectTestKit.response(302, "https://user:pass@evil.com/path")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response)
    | InvalidLocation => None
    | let r: Redirect =>
      h.fail("must reject Location with userinfo, got Redirect")
    | let _: RedirectError =>
      h.fail("expected InvalidLocation, got a different RedirectError")
    | _NotARedirect => h.fail("expected InvalidLocation")
    end

class \nodoc\ iso _TestRedirectIPv6Origin is UnitTest
  """A redirect to an IPv6 host strips brackets for the origin."""
  fun name(): String => "redirect/ipv6_origin"

  fun apply(h: TestHelper) =>
    let origin = Origin(true, "github.com", "443")
    let sent = _RedirectTestKit.request()
    let response =
      _RedirectTestKit.response(302, "https://[::1]:8443/path")
    match \exhaustive\ _RedirectDecision(sent, origin, response, 5)
    | let r: Redirect =>
      let target_origin = Origin.from_uri(r.target())
      h.assert_eq[String val]("::1", target_origin.host)
      h.assert_eq[String val]("8443", target_origin.port)
      h.assert_true(target_origin.secure)
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirectEmptyPathDefaultsToSlash is UnitTest
  """A Location with no path defaults to "/" in the request target."""
  fun name(): String => "redirect/empty_path_defaults_to_slash"

  fun apply(h: TestHelper) =>
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response)
    | let r: Redirect =>
      h.assert_eq[String val]("/", r.request().path)
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirectEmptyPathWithQuery is UnitTest
  """A Location with no path but a query defaults to "/?query"."""
  fun name(): String => "redirect/empty_path_with_query"

  fun apply(h: TestHelper) =>
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com?foo=bar")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response)
    | let r: Redirect =>
      h.assert_eq[String val]("/?foo=bar", r.request().path)
    | let _: RedirectError => h.fail("expected Redirect, got a RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _TestRedirectEmptyHostRejected is UnitTest
  """A Location with an empty host is InvalidLocation."""
  fun name(): String => "redirect/empty_host_rejected"

  fun apply(h: TestHelper) =>
    let response = _RedirectTestKit.response(302, "https:///path")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response)
    | InvalidLocation => None
    | let r: Redirect =>
      h.fail("must reject Location with empty host, got Redirect")
    | let _: RedirectError =>
      h.fail("expected InvalidLocation, got a different RedirectError")
    | _NotARedirect => h.fail("expected InvalidLocation")
    end

class \nodoc\ iso _TestRedirectMixedCaseScheme is UnitTest
  """Scheme comparison is case-insensitive per RFC 3986."""
  fun name(): String => "redirect/mixed_case_scheme"

  fun apply(h: TestHelper) =>
    let response =
      _RedirectTestKit.response(302, "HTTPS://cdn.example.com/file")
    match \exhaustive\ _RedirectTestKit.decide(
      _RedirectTestKit.request(), response)
    | let r: Redirect =>
      h.assert_eq[String val](
        "cdn.example.com", _RedirectTestKit.target_host(r))
    | let _: RedirectError =>
      h.fail("HTTPS must be accepted case-insensitively, got RedirectError")
    | _NotARedirect => h.fail("expected Redirect")
    end

class \nodoc\ iso _PropertyRedirectStripsCredentials
  is Property1[(U16, String val)]
  """
  For every redirect status and any safe header, a cross-origin hop carrying
  authorization, cookie, proxy-authorization, host, and referer drops all
  five while keeping the safe header.
  """
  fun name(): String => "redirect/property_strips_credentials"

  fun gen(): Generator[(U16, String val)] =>
    Generators.zip2[U16, String val](
      Generators.one_of[U16]([as U16: 301; 302; 303; 307; 308]),
      Generators.ascii_letters(where from = 1, to = 12))

  fun ref property(arg1: (U16, String val), ph: PropertyHelper) =>
    (let status, let safe_name_raw) = arg1
    // Keep the generated safe header clear of sensitive and body names.
    let safe_name: String val = "x-" + safe_name_raw.lower()
    let sent =
      HTTPRequest(
        GET,
        "/dl",
        recover val
          Headers
            .> set("authorization", "Bearer secret")
            .> set("cookie", "s=1")
            .> set("proxy-authorization", "Basic z")
            .> set("host", "github.com")
            .> set("referer", "https://github.com/dl")
            .> set(safe_name, "keep")
        end)
    let response =
      _RedirectTestKit.response(status, "https://cdn.example.com/x")

    match \exhaustive\ _RedirectTestKit.decide(sent, response)
    | let r: Redirect =>
      let hdrs = r.request().headers
      ph.assert_true(hdrs.get("authorization") is None)
      ph.assert_true(hdrs.get("cookie") is None)
      ph.assert_true(hdrs.get("proxy-authorization") is None)
      ph.assert_true(hdrs.get("host") is None)
      ph.assert_true(hdrs.get("referer") is None)
      ph.assert_false(
        hdrs.get(safe_name) is None,
        "safe header " + safe_name + " must survive")
    | let _: RedirectError =>
      ph.fail("expected Redirect, got a RedirectError")
    | _NotARedirect =>
      ph.fail("expected Redirect for status " + status.string())
    end
