actor _Tester
  let _queue: Array[(UnitTest iso, U64)]
  var _test_log: Array[String] iso
  var _ponytest: PonyTest tag
  var _in_test: Bool = false
  var _pass: Bool = false
  var _index: U64 = 0

  new create(t: PonyTest) =>
    _queue = Array[(UnitTest iso, U64)]
    _test_log = recover Array[String] end
    _ponytest = t

  be apply(subject: UnitTest iso, index: U64) =>
    if _in_test then
      // Already running a test, queue this one for later
      _queue.push((consume subject, index))
    else
      // Run this test now
      _test(consume subject, index)
    end

  fun ref _test(subject: UnitTest iso, index: U64) =>
    let helper = recover val TestHelper._create(this) end
    _in_test = true
    _pass = true
    _index = index
    var long_test = subject.long_test()

    try
      if not subject(helper) then
        // Failure by returning false
        _pass = false
      end
    else
      // Failure by thrown error
      _pass = false
    end

    if not long_test then
      _test_complete(true)
    end
    
  be _log(msg: String) =>
    _test_log.push(msg)

  be _assert_failed(msg: String) =>
    _test_log.push(msg)
    _pass = false

  be _test_complete(success: Bool) =>
    if not success then
      _pass = false
    end

    let log = _test_log = recover Array[String] end
    _ponytest._test_result(_index, _pass, consume log)
    _in_test = false
    _next_test()

  fun ref _next_test() =>
    if _queue.size() == 0 then
      // No more tests queued
      return
    end

    try
      (let t, let i) = _queue.pop()
      _test(consume t, i)
    end
