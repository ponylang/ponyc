class _TestRecord
  """
  Internal state for a single test's result and log.
  """

  let name: String
  var _pass: Bool = false
  var _log: (Array[String] val | None) = None

  new create(name': String) =>
    name = name'

  fun ref _result(pass: Bool, log: Array[String] val) =>
    _pass = pass
    _log = log

  fun _to_result(): TestResult val =>
    TestResult(
      name,
      _pass,
      match _log
      | let l: Array[String] val => l
      else recover val Array[String] end
      end)
