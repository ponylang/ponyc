use "pony_test"
use "files"
use "time"

primitive \nodoc\ _ChoiceArrayEq
  fun apply(
    h: TestHelper,
    a: Array[_Choice val] val,
    b: Array[_Choice val] val)
    ?
  =>
    h.assert_eq[USize](
      a.size(), b.size(), "choice array size mismatch")
    var i: USize = 0
    while i < a.size() do
      choice_eq(h, a(i)?, b(i)?, i)
      i = i + 1
    end

  fun choice_eq(
    h: TestHelper,
    a: _Choice val,
    b: _Choice val,
    idx: USize)
  =>
    match (a, b)
    | (let ai: _IntChoice, let bi: _IntChoice) =>
      h.assert_eq[I128](
        ai.value,
        bi.value,
        "IntChoice.value at " + idx.string())
      h.assert_eq[I128](
        ai.min,
        bi.min,
        "IntChoice.min at " + idx.string())
      h.assert_eq[I128](
        ai.max,
        bi.max,
        "IntChoice.max at " + idx.string())
      h.assert_eq[I128](
        ai.shrink_towards,
        bi.shrink_towards,
        "IntChoice.shrink_towards at " + idx.string())
    | (let au: _U128Choice, let bu: _U128Choice) =>
      h.assert_eq[U128](
        au.value,
        bu.value,
        "U128Choice.value at " + idx.string())
      h.assert_eq[U128](
        au.min,
        bu.min,
        "U128Choice.min at " + idx.string())
      h.assert_eq[U128](
        au.max,
        bu.max,
        "U128Choice.max at " + idx.string())
      h.assert_eq[U128](
        au.shrink_towards,
        bu.shrink_towards,
        "U128Choice.shrink_towards at " + idx.string())
    | (let af: _FloatChoice, let bf: _FloatChoice) =>
      h.assert_eq[U64](
        af.value.bits(),
        bf.value.bits(),
        "FloatChoice.value bits at " + idx.string())
      h.assert_eq[U64](
        af.min.bits(),
        bf.min.bits(),
        "FloatChoice.min bits at " + idx.string())
      h.assert_eq[U64](
        af.max.bits(),
        bf.max.bits(),
        "FloatChoice.max bits at " + idx.string())
    | (let ab_val: _BoolChoice, let bb_val: _BoolChoice) =>
      h.assert_eq[Bool](
        ab_val.value,
        bb_val.value,
        "BoolChoice.value at " + idx.string())
      h.assert_eq[Bool](
        ab_val.forced,
        bb_val.forced,
        "BoolChoice.forced at " + idx.string())
    else
      h.fail("choice type mismatch at index " + idx.string())
    end

class \nodoc\ iso _SerializerRoundTripAllTypesTest is UnitTest
  fun name(): String => "regression/serializer/round_trip_all_types"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      recover val
        [ as _Choice val:
          _IntChoice(42, -100, 100, 0)
          _U128Choice(999, 0, U128.max_value(), 0)
          _FloatChoice(3.14, 0.0, 100.0)
          _BoolChoice(true, false)
          _IntChoice(-7, I128.min_value(), I128.max_value(), 0)
          _BoolChoice(false, true)
        ]
      end

    let data = _ChoiceSerializer.serialize(choices)
    match \exhaustive\ _ChoiceSerializer.deserialize(consume data)
    | let result: Array[_Choice val] val =>
      _ChoiceArrayEq(h, choices, result)?
    | None =>
      h.fail("deserialize returned None for valid input")
    end

class \nodoc\ iso _SerializerEmptyArrayTest is UnitTest
  fun name(): String => "regression/serializer/empty_array"

  fun apply(h: TestHelper) =>
    let choices: Array[_Choice val] val =
      recover val Array[_Choice val] end

    let data = _ChoiceSerializer.serialize(choices)
    match \exhaustive\ _ChoiceSerializer.deserialize(consume data)
    | let result: Array[_Choice val] val =>
      h.assert_eq[USize](result.size(), 0)
    | None =>
      h.fail("deserialize returned None for empty array")
    end

class \nodoc\ iso _SerializerBadHeaderTest is UnitTest
  fun name(): String => "regression/serializer/bad_header"

  fun apply(h: TestHelper) =>
    h.assert_true(
      _ChoiceSerializer.deserialize("ponycheck v99\nI 1 0 10 0\n") is None,
      "wrong version should return None")
    h.assert_true(
      _ChoiceSerializer.deserialize("") is None,
      "empty string should return None")
    h.assert_true(
      _ChoiceSerializer.deserialize("garbage") is None,
      "garbage header should return None")

class \nodoc\ iso _SerializerBadLineTest is UnitTest
  fun name(): String => "regression/serializer/bad_line"

  fun apply(h: TestHelper) =>
    h.assert_true(
      _ChoiceSerializer.deserialize("ponycheck v1\nX 1 2 3\n") is None,
      "unknown type tag should return None")
    h.assert_true(
      _ChoiceSerializer.deserialize("ponycheck v1\nI 1 2\n") is None,
      "too few fields for IntChoice should return None")
    h.assert_true(
      _ChoiceSerializer.deserialize("ponycheck v1\nB maybe false\n") is None,
      "non-boolean value should return None")
    h.assert_true(
      _ChoiceSerializer.deserialize("ponycheck v1\nF abc 0 0\n") is None,
      "non-numeric float bits should return None")

class \nodoc\ iso _SerializerFloatExactBitsTest is UnitTest
  fun name(): String => "regression/serializer/float_exact_bits"

  fun apply(h: TestHelper) ? =>
    let neg_zero = F64.from_bits(0x8000000000000000)
    let pos_inf = F64.max_value() * F64(2)
    let neg_inf = -pos_inf

    let choices: Array[_Choice val] val =
      recover val
        [ as _Choice val:
          _FloatChoice(neg_zero, neg_inf, pos_inf)
          _FloatChoice(F64(0), F64(-1e308), F64(1e308))
        ]
      end

    let data = _ChoiceSerializer.serialize(choices)
    match \exhaustive\ _ChoiceSerializer.deserialize(consume data)
    | let result: Array[_Choice val] val =>
      _ChoiceArrayEq(h, choices, result)?
    | None =>
      h.fail("deserialize returned None for float edge cases")
    end

class \nodoc\ iso _SerializerNaNRoundTripTest is UnitTest
  fun name(): String => "regression/serializer/nan_round_trip"

  fun apply(h: TestHelper) ? =>
    let nan = F64(0) / F64(0)
    let choices: Array[_Choice val] val =
      recover val
        [ as _Choice val:
          _FloatChoice(nan, F64(0), F64(1))
        ]
      end

    let data = _ChoiceSerializer.serialize(choices)
    match \exhaustive\ _ChoiceSerializer.deserialize(consume data)
    | let result: Array[_Choice val] val =>
      h.assert_eq[USize](1, result.size(), "expected one choice")
      match result(0)?
      | let fc: _FloatChoice =>
        h.assert_true(fc.value.nan(), "NaN did not survive round-trip")
        h.assert_eq[U64](
          nan.bits(), fc.value.bits(), "NaN bit pattern changed")
      else
        h.fail("expected FloatChoice")
      end
    | None =>
      h.fail("deserialize returned None for NaN")
    end

class \nodoc\ iso _EncodeNameSafeCharsTest is UnitTest
  fun name(): String => "regression/encode_name/safe_chars"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      _RegressionDb._encode_name("abc123"), "abc123")
    h.assert_eq[String](
      _RegressionDb._encode_name("foo.bar_baz-qux"), "foo.bar_baz-qux")
    h.assert_eq[String](
      _RegressionDb._encode_name("AZ09"), "az09")
    h.assert_eq[String](
      _RegressionDb._encode_name("MyProperty"), "myproperty")

class \nodoc\ iso _EncodeNameSpecialCharsTest is UnitTest
  fun name(): String => "regression/encode_name/special_chars"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      _RegressionDb._encode_name("a b"), "a%20b")
    h.assert_eq[String](
      _RegressionDb._encode_name("foo/bar"), "foo%2Fbar")
    h.assert_eq[String](
      _RegressionDb._encode_name("a:b"), "a%3Ab")
    h.assert_eq[String](
      _RegressionDb._encode_name("100%"), "100%25")

class \nodoc\ iso _RegressionDbSaveLoadClearTest is UnitTest
  fun name(): String => "regression/db/save_load_clear"

  fun apply(h: TestHelper) ? =>
    let dir = _make_temp_dir(h)?

    let choices: Array[_Choice val] val =
      recover val
        [ as _Choice val:
          _IntChoice(10, 0, 100, 0)
          _BoolChoice(true, false)
        ]
      end

    let logger = _UnitTestPropertyLogger(h)
    _RegressionDb.save(dir, "test_prop", choices, logger)

    match \exhaustive\ _RegressionDb.load(dir, "test_prop", logger)
    | let loaded: Array[_Choice val] val =>
      _ChoiceArrayEq(h, choices, loaded)?
    | None =>
      h.fail("load returned None after save")
    end

    _RegressionDb.clear(dir, "test_prop", logger)

    h.assert_true(
      _RegressionDb.load(dir, "test_prop", logger) is None,
      "load should return None after clear")

    dir.remove()

  fun _make_temp_dir(h: TestHelper): FilePath ? =>
    let nanos = Time.nanos().string()
    let dir_name: String val = ".ponycheck-test-" + consume nanos
    let dir = FilePath(FileAuth(h.env.root), dir_name)
    if not dir.mkdir() then error end
    dir

class \nodoc\ iso _RegressionDbLoadMissingTest is UnitTest
  fun name(): String => "regression/db/load_missing"

  fun apply(h: TestHelper) =>
    let dir_name: String val =
      ".ponycheck-test-missing-" + Time.nanos().string()
    let dir = FilePath(FileAuth(h.env.root), dir_name)
    let logger = _UnitTestPropertyLogger(h)

    h.assert_true(
      _RegressionDb.load(dir, "nonexistent", logger) is None,
      "load from nonexistent dir should return None")

class \nodoc\ iso _RegressionDbCorruptDeletesTest is UnitTest
  fun name(): String => "regression/db/corrupt_deletes"

  fun apply(h: TestHelper) ? =>
    let dir = _make_temp_dir(h)?
    let logger = _UnitTestPropertyLogger(h)

    let filename: String val =
      _RegressionDb._encode_name("corrupt_prop") + ".choices"
    let path = dir.join(filename)?
    try
      (CreateFile(path) as File)
        .> write("this is not valid ponycheck data\n")
        .> dispose()
    else
      h.fail("could not write corrupt file")
      dir.remove()
      return
    end

    h.assert_true(path.exists(), "corrupt file should exist before load")

    h.assert_true(
      _RegressionDb.load(dir, "corrupt_prop", logger) is None,
      "corrupt file should return None")

    h.assert_false(path.exists(), "corrupt file should be deleted after load")

    dir.remove()

  fun _make_temp_dir(h: TestHelper): FilePath ? =>
    let dir_name: String val =
      ".ponycheck-test-corrupt-" + Time.nanos().string()
    let dir = FilePath(FileAuth(h.env.root), dir_name)
    if not dir.mkdir() then error end
    dir

class \nodoc\ iso _RegressionDbCreatesDirTest is UnitTest
  fun name(): String => "regression/db/creates_dir"

  fun apply(h: TestHelper) =>
    let dir_name: String val =
      ".ponycheck-test-newdir-" + Time.nanos().string()
    let dir = FilePath(FileAuth(h.env.root), dir_name)
    let logger = _UnitTestPropertyLogger(h)

    let choices: Array[_Choice val] val =
      recover val
        [ as _Choice val: _IntChoice(1, 0, 10, 0) ]
      end

    h.assert_false(dir.exists(), "dir should not exist before save")

    _RegressionDb.save(dir, "newdir_prop", choices, logger)

    h.assert_true(dir.exists(), "save should create directory")

    match \exhaustive\ _RegressionDb.load(dir, "newdir_prop", logger)
    | let loaded: Array[_Choice val] val =>
      h.assert_eq[USize](loaded.size(), 1)
    | None =>
      h.fail("could not load after save to new dir")
    end

    _RegressionDb.clear(dir, "newdir_prop", logger)
    dir.remove()

class \nodoc\ iso _RegressionSaveOnFailProperty is Property1[U8]
  fun name(): String => "regression-save-on-fail-test-prop"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 42)

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun ref property(arg1: U8, h: PropertyHelper) =>
    h.assert_true(arg1 == U8(0))

primitive \nodoc\ _RegressionTestEnv
  fun apply(env: Env, db_dir: String): Env val =>
    let vars: Array[String] val =
      recover val
        Array[String]
          .> push("PONYCHECK_DB_DIR=" + db_dir)
      end
    Env.create(
      env.root,
      env.input,
      env.out,
      env.err,
      env.args,
      vars,
      env.exitcode)

class \nodoc\ iso _RegressionSaveOnFailTest is UnitTest
  fun name(): String => "regression/integration/save_on_fail"

  fun apply(h: TestHelper) =>
    let property = recover iso _RegressionSaveOnFailProperty end
    let prop_name = "regression-save-on-fail-test-prop"
    let params = property.params()
    h.long_test(params.timeout)

    let dir_name: String val =
      ".ponycheck-test-save-" + Time.nanos().string()
    let regression_dir = FilePath(FileAuth(h.env.root), dir_name)
    let expected_filename: String val =
      _RegressionDb._encode_name(prop_name) + ".choices"
    let expected_path =
      try
        regression_dir.join(expected_filename)?
      else
        h.fail("could not join regression path")
        return
      end

    let test_env = _RegressionTestEnv(h.env, dir_name)
    let notify =
      _RegressionSaveOnFailNotify(h, expected_path, regression_dir)
    let logger = _UnitTestPropertyLogger(h)

    let runner =
      PropertyRunner[U8](
        consume property,
        params,
        notify,
        logger,
        test_env)
    runner.run()

class \nodoc\ val _RegressionSaveOnFailNotify is PropertyResultNotify
  let _h: TestHelper
  let _path: FilePath
  let _dir: FilePath

  new val create(h: TestHelper, path: FilePath, dir: FilePath) =>
    _h = h
    _path = path
    _dir = dir

  fun fail(msg: String) =>
    _h.log("FAIL: " + msg)

  fun complete(success: Bool) =>
    _h.assert_false(success, "property should have failed")
    _h.assert_true(
      _path.exists(),
      "regression file should exist after failure")

    _path.remove()
    _dir.remove()

    _h.complete(true)

class \nodoc\ iso _RegressionDbDisabledTest is UnitTest
  fun name(): String => "regression/integration/db_disabled"

  fun apply(h: TestHelper) =>
    let property = recover iso _RegressionDbDisabledProperty end
    let prop_name = "regression-disabled-test-prop"
    let params = property.params()
    h.long_test(params.timeout)

    let regression_dir =
      match \exhaustive\ _RegressionDb.resolve_dir(h.env)
      | let dir: FilePath => dir
      | None =>
        h.fail("resolve_dir returned None — PONYCHECK_NO_DB set?")
        return
      end
    let expected_filename: String val =
      _RegressionDb._encode_name(prop_name) + ".choices"
    let expected_path =
      try
        regression_dir.join(expected_filename)?
      else
        h.fail("could not join regression path")
        return
      end

    let notify =
      _RegressionDbDisabledNotify(h, expected_path)
    let logger = _UnitTestPropertyLogger(h)

    let runner =
      PropertyRunner[U8](
        consume property,
        params,
        notify,
        logger,
        h.env)
    runner.run()

class \nodoc\ iso _RegressionDbDisabledProperty is Property1[U8]
  fun name(): String => "regression-disabled-test-prop"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 42,
      regression_db' = false)

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun ref property(arg1: U8, h: PropertyHelper) =>
    h.assert_true(arg1 == U8(0))

class \nodoc\ val _RegressionDbDisabledNotify is PropertyResultNotify
  let _h: TestHelper
  let _path: FilePath

  new val create(h: TestHelper, path: FilePath) =>
    _h = h
    _path = path

  fun fail(msg: String) =>
    _h.log("FAIL: " + msg)

  fun complete(success: Bool) =>
    _h.assert_false(success, "property should have failed")
    _h.assert_false(
      _path.exists(),
      "regression file should NOT exist when db disabled")
    _h.complete(true)

primitive \nodoc\ _StatefulRegressionIncrement is Stringable
  fun string(): String iso^ => "increment".string()

class \nodoc\ iso _StatefulRegressionFailProperty
  is StatefulProperty[USize, USize, _StatefulRegressionIncrement]
  fun name(): String => "regression-stateful-save-on-fail-test-prop"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 42)

  fun max_steps(): USize => 5

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _StatefulRegressionIncrement
  =>
    ctx.sut = ctx.sut + 1
    ctx.model = ctx.model + 2
    _StatefulRegressionIncrement

  fun invariant(
    ctx: StatefulContext[USize, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut)

class \nodoc\ iso _StatefulRegressionSaveOnFailTest is UnitTest
  fun name(): String => "regression/integration/stateful_save_on_fail"

  fun apply(h: TestHelper) =>
    let property = recover iso _StatefulRegressionFailProperty end
    let prop_name = "regression-stateful-save-on-fail-test-prop"
    let params = property.params()
    h.long_test(params.timeout)

    let dir_name: String val =
      ".ponycheck-test-stateful-save-" + Time.nanos().string()
    let regression_dir = FilePath(FileAuth(h.env.root), dir_name)
    let expected_filename: String val =
      _RegressionDb._encode_name(prop_name) + ".choices"
    let expected_path =
      try
        regression_dir.join(expected_filename)?
      else
        h.fail("could not join regression path")
        return
      end

    let test_env = _RegressionTestEnv(h.env, dir_name)
    let notify =
      _RegressionSaveOnFailNotify(h, expected_path, regression_dir)
    let logger = _UnitTestPropertyLogger(h)

    let runner =
      StatefulPropertyRunner[USize, USize, _StatefulRegressionIncrement](
        consume property, params, notify, logger, test_env)
    h.dispose_when_done(runner)
    runner.run()
