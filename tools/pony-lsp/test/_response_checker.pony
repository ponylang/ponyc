use ".."
use "json"
use "pony_test"

interface val _ResponseChecker
  """
  Encapsulates the LSP method name and response validation for a single
  pending request. Implement this to integrate a new LSP feature into
  the shared test server.
  """
  fun lsp_method(): String
  fun lsp_range(): (None | (I64, I64, I64, I64))
  fun lsp_context(): (None | JsonObject)
  fun check(res: ResponseMessage val, h: TestHelper): Bool
