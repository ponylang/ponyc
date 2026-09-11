use "net"
use uri = "uri"
use "pony_test"

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

class \nodoc\ _RecordingReceiver is RedirectFollowerNotify
  """
  Records which callbacks a RedirectFollower forwards so tests can verify
  routing decisions.
  """
  var connected: USize = 0
  var responses: USize = 0
  embed response_list: Array[Response val] = Array[Response val]
  var body_chunks: USize = 0
  var completed: USize = 0
  var closed: USize = 0
  var redirect_errors: USize = 0
  embed redirect_error_list: Array[RedirectError] = Array[RedirectError]
  var connection_failures: USize = 0
  var parse_errors: USize = 0
  var throttled: USize = 0
  var unthrottled: USize = 0
  var timer_failures: USize = 0

  fun ref on_connected() =>
    connected = connected + 1

  fun ref on_response(response: Response val) =>
    responses = responses + 1
    response_list.push(response)

  fun ref on_body_chunk(data: Array[U8] val) =>
    body_chunks = body_chunks + 1

  fun ref on_response_complete() =>
    completed = completed + 1

  fun ref on_closed() =>
    closed = closed + 1

  fun ref on_redirect_error(err: RedirectError) =>
    redirect_errors = redirect_errors + 1
    redirect_error_list.push(err)

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    connection_failures = connection_failures + 1

  fun ref on_parse_error(err: ParseError) =>
    parse_errors = parse_errors + 1

  fun ref on_throttled() =>
    throttled = throttled + 1

  fun ref on_unthrottled() =>
    unthrottled = unthrottled + 1

  fun ref on_timer_failure() =>
    timer_failures = timer_failures + 1

class \nodoc\ _CountingFactory is RedirectConnectionFactory
  """
  Counts how many times it was called and returns none() connections.
  """
  var calls: USize = 0

  fun ref apply(target: uri.URI val): HTTPClientConnection =>
    calls = calls + 1
    HTTPClientConnection.none()

class \nodoc\ iso _TestFollowerForwardsNonRedirect is UnitTest
  """
  A non-redirect response forwards to the receiver when _last_request is None.
  """
  fun name(): String => "redirect_follower/forwards_non_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443"))

    let response = _RedirectTestKit.response(200)
    follower.on_response(response)
    h.assert_eq[USize](1, receiver.responses, "200 must forward")
    h.assert_eq[USize](0, receiver.redirect_errors)

class \nodoc\ iso _TestFollowerForwardsNonRedirectWithLastRequest is UnitTest
  """
  A non-redirect response forwards even when _last_request is set.
  """
  fun name(): String => "redirect_follower/forwards_non_redirect_with_request"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443")
        where last_request' = _RedirectTestKit.request())

    let response = _RedirectTestKit.response(200)
    follower.on_response(response)
    h.assert_eq[USize](1, receiver.responses, "200 must forward")

class \nodoc\ iso _TestFollowerInterceptsRedirect is UnitTest
  """
  A redirect response is intercepted — the receiver does not get on_response.
  """
  fun name(): String => "redirect_follower/intercepts_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)
    h.assert_eq[USize](0, receiver.responses, "302 must not forward")
    h.assert_eq[USize](0, receiver.redirect_errors)

class \nodoc\ iso _TestFollowerRedirectErrorForwards is UnitTest
  """
  A redirect error (e.g. TooManyRedirects) calls on_redirect_error on the
  receiver.
  """
  fun name(): String => "redirect_follower/redirect_error_forwards"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        0, // budget exhausted
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)
    h.assert_eq[USize](
      0, receiver.responses, "error must not forward response")
    h.assert_eq[USize](
      1, receiver.redirect_errors, "error must forward")

class \nodoc\ iso _TestFollowerSuppressesBodyDuringRedirect is UnitTest
  """
  Body chunks arriving while a redirect is pending are silently consumed.
  """
  fun name(): String => "redirect_follower/suppresses_body_during_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    // Trigger a redirect to set _pending
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)

    // Body chunks during a redirect are consumed silently
    follower.on_body_chunk(recover val [as U8: 'x'] end)
    follower.on_body_chunk(recover val [as U8: 'y'] end)
    h.assert_eq[USize](0, receiver.body_chunks, "body must be suppressed")

class \nodoc\ iso _TestFollowerForwardsBodyWithoutRedirect is UnitTest
  """
  Body chunks forward normally when no redirect is pending.
  """
  fun name(): String => "redirect_follower/forwards_body_without_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443"))

    follower.on_body_chunk(recover val [as U8: 'x'] end)
    h.assert_eq[USize](1, receiver.body_chunks, "body must forward")

class \nodoc\ iso _TestFollowerSuppressesClosedDuringRedirect is UnitTest
  """
  on_closed during a redirect teardown is suppressed.
  """
  fun name(): String => "redirect_follower/suppresses_closed_during_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    // Trigger a redirect to set _pending
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)

    // on_closed while pending is suppressed
    follower.on_closed()
    h.assert_eq[USize](0, receiver.closed, "closed must be suppressed")

class \nodoc\ iso _TestFollowerForwardsClosedWithoutRedirect is UnitTest
  """
  on_closed forwards when no redirect is pending.
  """
  fun name(): String => "redirect_follower/forwards_closed_without_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443"))

    follower.on_closed()
    h.assert_eq[USize](1, receiver.closed, "closed must forward")

class \nodoc\ iso _TestFollowerForwardsPassthroughCallbacks is UnitTest
  """
  Callbacks that always forward regardless of redirect state: connection
  failure, parse error, throttled, unthrottled.
  """
  fun name(): String => "redirect_follower/forwards_passthrough_callbacks"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443"))

    follower.on_connection_failure(ConnectionFailedTCP)
    h.assert_eq[USize](1, receiver.connection_failures)

    follower.on_parse_error(TooLarge)
    h.assert_eq[USize](1, receiver.parse_errors)

    follower.on_throttled()
    h.assert_eq[USize](1, receiver.throttled)

    follower.on_unthrottled()
    h.assert_eq[USize](1, receiver.unthrottled)

    follower.on_timer_failure()
    h.assert_eq[USize](1, receiver.timer_failures)

class \nodoc\ iso _TestFollowerConnectedForwardsWithoutPending is UnitTest
  """
  on_connected without a pending redirect forwards to the receiver.
  """
  fun name(): String => "redirect_follower/connected_forwards_without_pending"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443"))

    follower.on_connected()
    h.assert_eq[USize](1, receiver.connected, "connected must forward")

class \nodoc\ iso _TestFollowerPendingRedirectSuppressesConnectedForward
  is UnitTest
  """
  When a redirect is pending, on_response does not forward to the receiver.
  The connected-with-pending path (which would send the redirect request)
  requires a live connection and is covered by integration tests.
  """
  fun name(): String =>
    "redirect_follower/pending_redirect_suppresses_connected_forward"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    // Trigger a cross-origin redirect — this sets _pending
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)
    h.assert_eq[USize](
      0, receiver.responses, "redirect must not forward response")

class \nodoc\ iso _TestFollowerSameOriginRedirectDoesNotUseFactory is UnitTest
  """
  A same-origin redirect does not invoke the factory — the existing
  connection is reused.
  """
  fun name(): String =>
    "redirect_follower/same_origin_redirect_no_factory"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    // Same-origin redirect sets _pending
    let response =
      _RedirectTestKit.response(302, "https://github.com/other")
    follower.on_response(response)
    h.assert_eq[USize](
      0, receiver.responses, "redirect must be intercepted")
    h.assert_eq[USize](
      0, factory.calls, "on_response must not invoke factory")

class \nodoc\ iso _TestFollowerCrossOriginRedirectSetsUpFactory is UnitTest
  """
  A cross-origin redirect is intercepted and the factory is not called
  until on_response_complete.
  """
  fun name(): String =>
    "redirect_follower/cross_origin_redirect_sets_pending"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    // Cross-origin redirect is intercepted
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)
    h.assert_eq[USize](
      0, receiver.responses, "redirect must be intercepted")
    h.assert_eq[USize](
      0, factory.calls, "factory is not called until response completes")

class \nodoc\ iso _TestFollowerCompleteForwardsWithoutRedirect is UnitTest
  """
  on_response_complete without a pending redirect forwards to the receiver.
  """
  fun name(): String =>
    "redirect_follower/complete_forwards_without_redirect"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        5,
        Origin(true, "example.com", "443"))

    follower.on_response_complete()
    h.assert_eq[USize](
      1, receiver.completed, "complete must forward")

class \nodoc\ iso _TestFollowerErrorSuppressesBodyAndComplete is UnitTest
  """
  After a redirect error, body chunks and on_response_complete from the
  error response are suppressed — the receiver gets on_redirect_error
  only.
  """
  fun name(): String =>
    "redirect_follower/error_suppresses_body_and_complete"

  fun apply(h: TestHelper) =>
    let receiver: _RecordingReceiver ref = _RecordingReceiver
    let factory: _CountingFactory ref = _CountingFactory
    let follower =
      RedirectFollower._for_test(
        HTTPClientConnection.none(),
        receiver,
        factory,
        0, // budget exhausted
        Origin(true, "github.com", "443")
        where last_request' = _RedirectTestKit.request())

    // Trigger a redirect error (budget exhausted)
    let response =
      _RedirectTestKit.response(302, "https://cdn.example.com/file")
    follower.on_response(response)
    h.assert_eq[USize](
      1, receiver.redirect_errors, "error must be delivered")

    // Body chunks from the error response must be suppressed
    follower.on_body_chunk(recover val [as U8: '<'; 'h'] end)
    h.assert_eq[USize](
      0, receiver.body_chunks, "body after error must be suppressed")

    // on_response_complete from the error response must be suppressed
    follower.on_response_complete()
    h.assert_eq[USize](
      0, receiver.completed, "complete after error must be suppressed")
