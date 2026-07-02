// Fixture for workspace/symbol integration tests. Reformatting this
// file (reindenting, moving members, renaming) invalidates the exact
// (line, col) tuples asserted by `_WsSymRangeTest` in
// `_workspace_symbol_integration_tests.pony`. Keep it stable.

actor _WsSymHost
  var _count: U32 = 0
  let _name: String = ""
  embed _inner: Array[U8] = Array[U8]

  new create() =>
    _count = 0

  fun ref increment(): U32 =>
    _count = _count + 1
    _count

  be ping() =>
    None

class _WsSymInner
