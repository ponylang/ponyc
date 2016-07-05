
trait val Status
  fun apply(): U16
  fun text(): String

primitive StatusContinue is Status
  fun apply(): U16 => 100
  fun text(): String => "Continue"
primitive StatusSwitchingProtocols is Status
  fun apply(): U16 => 101
  fun text(): String => "Switching Protocols"

primitive StatusOK is Status
  fun apply(): U16 => 200
  fun text(): String => "OK"
primitive StatusCreated is Status
  fun apply(): U16 => 201
  fun text(): String => "Created"
primitive StatusAccepted is Status
  fun apply(): U16 => 202
  fun text(): String => "Accepted"
primitive StatusNonAuthoritativeInfo is Status
  fun apply(): U16 => 203
  fun text(): String => "Non-Authoritative Information"
primitive StatusNoContent is Status
  fun apply(): U16 => 204
  fun text(): String => "No Content"
primitive StatusResetContent is Status
  fun apply(): U16 => 205
  fun text(): String => "Reset Content"
primitive StatusPartialContent is Status
  fun apply(): U16 => 206
  fun text(): String => "Partial Content"

primitive StatusMultipleChoices is Status
  fun apply(): U16 => 300
  fun text(): String => "Multiple Choices"
primitive StatusMovedPermanently is Status
  fun apply(): U16 => 301
  fun text(): String => "Moved Permanently"
primitive StatusFound is Status
  fun apply(): U16 => 302
  fun text(): String => "Found"
primitive StatusSeeOther is Status
  fun apply(): U16 => 303
  fun text(): String => "See Other"
primitive StatusNotModified is Status
  fun apply(): U16 => 304
  fun text(): String => "Not Modified"
primitive StatusUseProxy is Status
  fun apply(): U16 => 305
  fun text(): String => "Use Proxy"
primitive StatusTemporaryRedirect is Status
  fun apply(): U16 => 307
  fun text(): String => "Temporary Redirect"

primitive StatusBadRequest is Status
  fun apply(): U16 => 400
  fun text(): String => "Bad Request"
primitive StatusUnauthorized is Status
  fun apply(): U16 => 401
  fun text(): String => "Unauthorized"
primitive StatusPaymentRequired is Status
  fun apply(): U16 => 402
  fun text(): String => "Payment Required"
primitive StatusForbidden is Status
  fun apply(): U16 => 403
  fun text(): String => "Forbidden"
primitive StatusNotFound is Status
  fun apply(): U16 => 404
  fun text(): String => "Not Found"
primitive StatusMethodNotAllowed is Status
  fun apply(): U16 => 405
  fun text(): String => "Method Not Allowed"
primitive StatusNotAcceptable is Status
  fun apply(): U16 => 406
  fun text(): String => "Not Acceptable"
primitive StatusProxyAuthRequired is Status
  fun apply(): U16 => 407
  fun text(): String => "Proxy Authentication Required"
primitive StatusRequestTimeout is Status
  fun apply(): U16 => 408
  fun text(): String => "Request Timeout"
primitive StatusConflict is Status
  fun apply(): U16 => 409
  fun text(): String => "Conflict"
primitive StatusGone is Status
  fun apply(): U16 => 410
  fun text(): String => "Gone"
primitive StatusLengthRequired is Status
  fun apply(): U16 => 411
  fun text(): String => "Length Required"
primitive StatusPreconditionFailed is Status
  fun apply(): U16 => 412
  fun text(): String => "Precondition Failed"
primitive StatusRequestEntityTooLarge is Status
  fun apply(): U16 => 413
  fun text(): String => "Request Entity Too Large"
primitive StatusRequestURITooLong is Status
  fun apply(): U16 => 414
  fun text(): String => "Request URI Too Long"
primitive StatusUnsupportedMediaType is Status
  fun apply(): U16 => 415
  fun text(): String => "Unsupported Media Type"
primitive StatusRequestedRangeNotSatisfiable is Status
  fun apply(): U16 => 416
  fun text(): String => "Requested Range Not Satisfiable"
primitive StatusExpectationFailed is Status
  fun apply(): U16 => 417
  fun text(): String => "Expectation Failed"
primitive StatusTeapot is Status
  fun apply(): U16 => 418
  fun text(): String => "I'm a teapot"
primitive StatusPreconditionRequired is Status
  fun apply(): U16 => 428
  fun text(): String => "Precondition Required"
primitive StatusTooManyRequests is Status
  fun apply(): U16 => 429
  fun text(): String => "Too Many Requests"
primitive StatusRequestHeaderFieldsTooLarge is Status
  fun apply(): U16 => 431
  fun text(): String => "Request Header Fields Too Large"
primitive StatusUnavailableForLegalReasons is Status
  fun apply(): U16 => 451
  fun text(): String => "Unavailable For Legal Reasons"

primitive StatusInternalServerError is Status
  fun apply(): U16 => 500
  fun text(): String => "Internal Server Error"
primitive StatusNotImplemented is Status
  fun apply(): U16 => 501
  fun text(): String => "Not Implemented"
primitive StatusBadGateway is Status
  fun apply(): U16 => 502
  fun text(): String => "Bad Gateway"
primitive StatusServiceUnavailable is Status
  fun apply(): U16 => 503
  fun text(): String => "Service Unavailable"
primitive StatusGatewayTimeout is Status
  fun apply(): U16 => 504
  fun text(): String => "Gateway Timeout"
primitive StatusHTTPVersionNotSupported is Status
  fun apply(): U16 => 505
  fun text(): String => "HTTP Version Not Supported"
primitive StatusNetworkAuthenticationRequired is Status
  fun apply(): U16 => 511
  fun text(): String => "Network Authentication Required"
