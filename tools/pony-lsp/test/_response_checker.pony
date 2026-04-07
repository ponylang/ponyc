use ".."
use "pony_test"

interface val _ResponseChecker
  """
  Encapsulates the LSP method name and response validation for a single
  pending request. Implement this to integrate a new LSP feature into
  the shared test server.
  """
  fun lsp_method(): String
  fun check(res: ResponseMessage val, h: TestHelper, action: String): Bool
