use ".."
use "json"
use "pony_test"

interface val _RoundTripCheck
  """
  Defines one LSP request/response pair for the integration test harness.
  Implement this to add a new LSP feature to the shared test server.
  The harness supplies textDocument.uri; everything else goes in lsp_params().
  """
  // LSP method string sent in the request (e.g. "textDocument/hover").
  fun lsp_method(): String

  // Optional request parameters merged into the params object beyond
  // textDocument.uri. Return a JsonObject whose key-value pairs are merged
  // directly, or None to add nothing. Include "position" for point-based
  // methods, "range" for span-based methods, and any other LSP-defined keys
  // as needed.
  fun lsp_params(): (None | JsonObject)

  // Validate the server response. Return true if all assertions pass, false
  // if any fail. Implementations call h.assert_*/h.fail directly so failures
  // are attributed to the correct test action.
  fun check(res: ResponseMessage val, h: TestHelper): Bool
