use "pony_check"

class \nodoc\ iso _PropertyValidMethodParsesCorrectly is Property1[String val]
  fun name(): String => "method/valid_parse"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [ "GET"; "HEAD"; "POST"; "PUT"; "DELETE"
        "CONNECT"; "OPTIONS"; "TRACE"; "PATCH"])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match Methods.parse(arg1)
    | let m: Method =>
      ph.assert_eq[String val](arg1, m.string())
    else
      ph.fail("valid method string should parse: " + arg1)
    end

class \nodoc\ iso _PropertyInvalidMethodReturnsNone is Property1[String val]
  fun name(): String => "method/invalid_returns_none"

  fun gen(): Generator[String val] =>
    Generators.frequency[String val](
      [ as WeightedGenerator[String val]:
        // empty string
        (1, Generators.unit[String val](""))
        // wrong case
        (1, Generators.one_of[String val](
          [ "get"; "head"; "post"; "put"; "delete"
            "connect"; "options"; "trace"; "patch"]))
        // valid method with extra chars appended
        (1, Generators.one_of[String val](
          [ "GETX"; "POSTY"; "PUTS"; "DELETES"
            "HEADS"; "CONNECTX"; "OPTIONS!"
            "TRACES"; "PATCHX"]))
        // truncated valid methods
        (1, Generators.one_of[String val](
          [ "GE"; "HEA"; "POS"; "PU"; "DELET"
            "CONNEC"; "OPTION"; "TRAC"; "PATC"]))
        // numeric strings
        (1, Generators.ascii_numeric(1, 10))
        // random ASCII (vanishingly small chance of being a valid method)
        (2, Generators.ascii(1, 20))
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    ph.assert_true(
      Methods.parse(arg1) is None,
      "invalid method string should not parse: " + arg1)

class \nodoc\ iso _PropertyMethodParseBoundary
  is Property1[(String val, Bool)]
  fun name(): String => "method/parse_boundary"

  fun gen(): Generator[(String val, Bool)] =>
    let valid_gen: Generator[(String val, Bool)] =
      Generators.one_of[String val](
        [ "GET"; "HEAD"; "POST"; "PUT"; "DELETE"
          "CONNECT"; "OPTIONS"; "TRACE"; "PATCH"])
        .map[(String val, Bool)]({(s) => (s, true) })

    let invalid_gen: Generator[(String val, Bool)] =
      Generators.frequency[String val](
        [ as WeightedGenerator[String val]:
          (1, Generators.unit[String val](""))
          (1, Generators.one_of[String val](
            [ "get"; "head"; "post"; "put"
              "delete"; "connect"; "options"
              "trace"; "patch"]))
          (1, Generators.one_of[String val](
            [ "GETX"; "POSTY"; "PUTS"
              "DELETES"]))
          (1, Generators.ascii_numeric(1, 10))
          (2, Generators.ascii(1, 20))
        ]).map[(String val, Bool)](
          {(s) => (s, false) })

    Generators.frequency[(String val, Bool)](
      [ as WeightedGenerator[(String val, Bool)]:
        (1, valid_gen)
        (1, invalid_gen)
      ])

  fun ref property(arg1: (String val, Bool), ph: PropertyHelper) =>
    (let input, let should_parse) = arg1
    let result = Methods.parse(input)
    if should_parse then
      match result
      | let m: Method =>
        ph.assert_eq[String val](input, m.string())
      else
        ph.fail("expected parse success for: " + input)
      end
    else
      ph.assert_true(
        result is None,
        "expected parse failure for: " + input)
    end
