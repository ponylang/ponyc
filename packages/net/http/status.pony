
trait val Status
  fun apply(): U16
  fun string(): String

primitive StatusContinue is Status
  fun apply(): U16 => 100
  fun string(): String => "100 Continue"
primitive StatusSwitchingProtocols is Status
  fun apply(): U16 => 101
  fun string(): String => "101 Switching Protocols"

primitive StatusOK is Status
  fun apply(): U16 => 200
  fun string(): String => "200 OK"
primitive StatusCreated is Status
  fun apply(): U16 => 201
  fun string(): String => "201 Created"
primitive StatusAccepted is Status
  fun apply(): U16 => 202
  fun string(): String => "202 Accepted"
primitive StatusNonAuthoritativeInfo is Status
  fun apply(): U16 => 203
  fun string(): String => "203 Non-Authoritative Information"
primitive StatusNoContent is Status
  fun apply(): U16 => 204
  fun string(): String => "204 No Content"
primitive StatusResetContent is Status
  fun apply(): U16 => 205
  fun string(): String => "205 Reset Content"
primitive StatusPartialContent is Status
  fun apply(): U16 => 206
  fun string(): String => "206 Partial Content"

primitive StatusMultipleChoices is Status
  fun apply(): U16 => 300
  fun string(): String => "300 Multiple Choices"
primitive StatusMovedPermanently is Status
  fun apply(): U16 => 301
  fun string(): String => "301 Moved Permanently"
primitive StatusFound is Status
  fun apply(): U16 => 302
  fun string(): String => "302 Found"
primitive StatusSeeOther is Status
  fun apply(): U16 => 303
  fun string(): String => "303 See Other"
primitive StatusNotModified is Status
  fun apply(): U16 => 304
  fun string(): String => "304 Not Modified"
primitive StatusUseProxy is Status
  fun apply(): U16 => 305
  fun string(): String => "305 Use Proxy"
primitive StatusTemporaryRedirect is Status
  fun apply(): U16 => 307
  fun string(): String => "307 Temporary Redirect"

primitive StatusBadRequest is Status
  fun apply(): U16 => 400
  fun string(): String => "400 Bad Request"
primitive StatusUnauthorized is Status
  fun apply(): U16 => 401
  fun string(): String => "401 Unauthorized"
primitive StatusPaymentRequired is Status
  fun apply(): U16 => 402
  fun string(): String => "402 Payment Required"
primitive StatusForbidden is Status
  fun apply(): U16 => 403
  fun string(): String => "403 Forbidden"
primitive StatusNotFound is Status
  fun apply(): U16 => 404
  fun string(): String => "404 Not Found"
primitive StatusMethodNotAllowed is Status
  fun apply(): U16 => 405
  fun string(): String => "405 Method Not Allowed"
primitive StatusNotAcceptable is Status
  fun apply(): U16 => 406
  fun string(): String => "406 Not Acceptable"
primitive StatusProxyAuthRequired is Status
  fun apply(): U16 => 407
  fun string(): String => "407 Proxy Authentication Required"
primitive StatusRequestTimeout is Status
  fun apply(): U16 => 408
  fun string(): String => "408 Request Timeout"
primitive StatusConflict is Status
  fun apply(): U16 => 409
  fun string(): String => "409 Conflict"
primitive StatusGone is Status
  fun apply(): U16 => 410
  fun string(): String => "410 Gone"
primitive StatusLengthRequired is Status
  fun apply(): U16 => 411
  fun string(): String => "411 Length Required"
primitive StatusPreconditionFailed is Status
  fun apply(): U16 => 412
  fun string(): String => "412 Precondition Failed"
primitive StatusRequestEntityTooLarge is Status
  fun apply(): U16 => 413
  fun string(): String => "413 Request Entity Too Large"
primitive StatusRequestURITooLong is Status
  fun apply(): U16 => 414
  fun string(): String => "414 Request URI Too Long"
primitive StatusUnsupportedMediaType is Status
  fun apply(): U16 => 415
  fun string(): String => "415 Unsupported Media Type"
primitive StatusRequestedRangeNotSatisfiable is Status
  fun apply(): U16 => 416
  fun string(): String => "416 Requested Range Not Satisfiable"
primitive StatusExpectationFailed is Status
  fun apply(): U16 => 417
  fun string(): String => "417 Expectation Failed"
primitive StatusTeapot is Status
  fun apply(): U16 => 418
  fun string(): String => "418 I'm a teapot"
primitive StatusPreconditionRequired is Status
  fun apply(): U16 => 428
  fun string(): String => "428 Precondition Required"
primitive StatusTooManyRequests is Status
  fun apply(): U16 => 429
  fun string(): String => "429 Too Many Requests"
primitive StatusRequestHeaderFieldsTooLarge is Status
  fun apply(): U16 => 431
  fun string(): String => "431 Request Header Fields Too Large"
primitive StatusUnavailableForLegalReasons is Status
  fun apply(): U16 => 451
  fun string(): String => "451 Unavailable For Legal Reasons"

primitive StatusInternalServerError is Status
  fun apply(): U16 => 500
  fun string(): String => "500 Internal Server Error"
primitive StatusNotImplemented is Status
  fun apply(): U16 => 501
  fun string(): String => "501 Not Implemented"
primitive StatusBadGateway is Status
  fun apply(): U16 => 502
  fun string(): String => "502 Bad Gateway"
primitive StatusServiceUnavailable is Status
  fun apply(): U16 => 503
  fun string(): String => "503 Service Unavailable"
primitive StatusGatewayTimeout is Status
  fun apply(): U16 => 504
  fun string(): String => "504 Gateway Timeout"
primitive StatusHTTPVersionNotSupported is Status
  fun apply(): U16 => 505
  fun string(): String => "505 HTTP Version Not Supported"
primitive StatusNetworkAuthenticationRequired is Status
  fun apply(): U16 => 511
  fun string(): String => "511 Network Authentication Required"
