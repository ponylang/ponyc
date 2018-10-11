use "ponytest"
use "itertools"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestALPNProtocolListEncoding)
    test(_TestALPNProtocolListDecode)
    test(_TestALPNStandardProtocolResolver)

class iso _TestALPNProtocolListEncoding is UnitTest
  """
  [Protocol Lists]() are correctly encoded and errors are raised when trying to encode invalid identifiers
  """
  fun name(): String => "net/ssl/_ALPNProtocolList.from_array"

  fun apply(h: TestHelper) =>
    let valid_h2http11 = String.from_array([as U8: 2; 'h'; '2'; 8; 'h'; 't'; 't'; 'p'; '/'; '1'; '.'; '1'])

    h.assert_error({()? => _ALPNProtocolList.from_array([""])? }, "raise error on empty protocol identifier")
    h.assert_error({()? => _ALPNProtocolList.from_array(["dummy"; ""])? }, "raise error when encoding an protocol identifier")
    h.assert_error({()? => _ALPNProtocolList.from_array([])? }, "raise error when encoding an empty array")

    let id256chars = recover val String(256) .> concat(Iter[U8].repeat_value('A'), 0, 256) end
    h.assert_eq[USize](id256chars.size(), USize(256))
    h.assert_error({()? => _ALPNProtocolList.from_array([id256chars])? }, "raise error on identifier longer than 256 bytes.")
    h.assert_error({()? => _ALPNProtocolList.from_array([id256chars; "dummy"])? }, "raise error on identifier longer than 256 bytes.")

    try
      h.assert_eq[String](_ALPNProtocolList.from_array(["h2"; "http/1.1"])?, valid_h2http11)
    else
      h.fail("failed to encode an array of valid identifiers")
    end

class iso _TestALPNProtocolListDecode is UnitTest
  fun name(): String => "net/ssl/_ALPNProtocolList.to_array"

  fun apply(h: TestHelper) =>
    let valid_h2http11 = String.from_array([as U8: 2; 'h'; '2'; 8; 'h'; 't'; 't'; 'p'; '/'; '1'; '.'; '1'])
    try
      let decoded = _ALPNProtocolList.to_array(valid_h2http11)?
      h.assert_eq[USize](decoded.size(), USize(2))
      h.assert_eq[ALPNProtocolName](decoded.apply(0)?, "h2")
      h.assert_eq[ALPNProtocolName](decoded.apply(1)?, "http/1.1")
    else
      h.fail("failed to decode a valid protocol list")
    end

    h.assert_error({()? => _ALPNProtocolList.to_array("")? }, "raise error when decoding an empty protocol list")
    h.assert_error({()? => _ALPNProtocolList.to_array(String.from_array([as U8: 3; 'h'; '2']))? }, "raise error on malformed data")
    h.assert_error({()? => _ALPNProtocolList.to_array(String.from_array([as U8: 0]))? }, "raise error on malformed data")
    h.assert_error({()? => _ALPNProtocolList.to_array(String.from_array([as U8: 1; 'A'; 0]))? }, "raise error on malformed data")
    h.assert_error({()? => _ALPNProtocolList.to_array(String.from_array([as U8: 1; 'A'; 1]))? }, "raise error on malformed data")

class iso _TestALPNStandardProtocolResolver is UnitTest
  fun name(): String => "net/ssl/StandardALPNProtocolResolver"

  fun apply(h: TestHelper) =>
    fallback_case(h)
    failure_case(h)
    match_cases(h)

  fun fallback_case(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"])

    match resolver.resolve(["http/1.1"])
    | "http/1.1" => None
    else h.fail("ALPNStandardProtocolResolver didn't fall back to clients first identifier, when it should have")
    end

  fun failure_case(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"], false)

    match resolver.resolve(["http/1.1"])
    | ALPNWarning => None
    else h.fail("ALPNStandardProtocolResolver didn't return ALPNFailure, when it should have")
    end

  fun match_cases(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"])

    match resolver.resolve(["dummy"; "h2"; "http/1.1"])
    | "h2" => None
    else h.fail("ALPNStandardProtocolResolver didn't return a matching protocol")
    end
