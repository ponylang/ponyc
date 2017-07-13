use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestMinimal)
    test(_TestBadName)
    test(_TestUnknownCommand)
    test(_TestUnexpectedArg)
    test(_TestUnknownShort)
    test(_TestUnknownLong)
    test(_TestHyphenArg)
    test(_TestBools)
    test(_TestDefaults)
    test(_TestShortsAdj)
    test(_TestShortsEq)
    test(_TestShortsNext)
    test(_TestLongsEq)
    test(_TestLongsNext)
    test(_TestEnvs)
    test(_TestOptionStop)
    test(_TestDuplicate)
    test(_TestChat)
    test(_TestMustBeLeaf)
    test(_TestHelp)

class iso _TestMinimal is UnitTest
  fun name(): String => "ponycli/minimal"

  fun apply(h: TestHelper) ? =>
    let cs = CommandSpec.leaf("minimal", "", [
      OptionSpec.bool("aflag", "")
    ])

    h.assert_eq[String]("minimal", cs.name())

    let args: Array[String] = ["ignored"; "--aflag=true"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("minimal", cmd.fullname())
    h.assert_eq[Bool](true, cmd.option("aflag").bool())

class iso _TestBadName is UnitTest
  fun name(): String => "ponycli/badname"

  // Negative test: command names must be alphanum tokens
  fun apply(h: TestHelper) =>
    try
      let cs = CommandSpec.leaf("min imal", "")
      h.fail("expected error on bad command name: " + cs.name())
    end

class iso _TestUnknownCommand is UnitTest
  fun name(): String => "ponycli/unknown_command"

  // Negative test: unknown command should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()

    let args: Array[String] = [
      "ignored"
      "unknown"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String]("Error: unknown command at: 'unknown'", se.string())
    else
      h.fail("expected syntax error for unknown command: " + cmdErr.string())
    end

class iso _TestUnexpectedArg is UnitTest
  fun name(): String => "ponycli/unknown_command"

  // Negative test: unexpected arg/command token should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()

    let args: Array[String] = [
      "ignored"
      "unknown"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String](
        "Error: too many positional arguments at: 'unknown'", se.string())
    else
      h.fail("expected syntax error for unknown command: " + cmdErr.string())
    end

class iso _TestUnknownShort is UnitTest
  fun name(): String => "ponycli/unknown_short"

  // Negative test: unknown short option should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()

    let args: Array[String] = [
      "ignored"
      "-Z"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String]("Error: unknown short option at: 'Z'", se.string())
    else
      h.fail(
        "expected syntax error for unknown short option: " + cmdErr.string())
    end

class iso _TestUnknownLong is UnitTest
  fun name(): String => "ponycli/unknown_long"

  // Negative test: unknown long option should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()

    let args: Array[String] = [
      "ignored"
      "--unknown"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String](
        "Error: unknown long option at: 'unknown'", se.string())
    else
      h.fail(
        "expected syntax error for unknown long option: " + cmdErr.string())
    end

class iso _TestHyphenArg is UnitTest
  fun name(): String => "ponycli/hyphen"

  // Rule 1
  fun apply(h: TestHelper) ? =>
    let cs = CommandSpec.leaf("minimal" where args' = [
      ArgSpec.string("name", "")
    ])
    let args: Array[String] = ["ignored"; "-"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("minimal", cmd.fullname())
    h.assert_eq[String]("-", cmd.arg("name").string())

class iso _TestBools is UnitTest
  fun name(): String => "ponycli/bools"

  // Rules 2, 3, 5, 7 w/ Bools
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()?

    let args: Array[String] = ["ignored"; "-ab"; "-c=true"; "-d=false"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("bools", cmd.fullname())
    h.assert_eq[Bool](true, cmd.option("aaa").bool())
    h.assert_eq[Bool](true, cmd.option("bbb").bool())
    h.assert_eq[Bool](true, cmd.option("ccc").bool())
    h.assert_eq[Bool](false, cmd.option("ddd").bool())

class iso _TestDefaults is UnitTest
  fun name(): String => "ponycli/defaults"

  // Rules 2, 3, 5, 6
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = ["ignored"; "-B"; "-S--"; "-I42"; "-F42.0"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[Bool](true, cmd.option("boolo").bool())
    h.assert_eq[String]("astring", cmd.option("stringo").string())
    h.assert_eq[I64](42, cmd.option("into").i64())
    h.assert_eq[F64](42.0, cmd.option("floato").f64())
    h.assert_eq[USize](0, cmd.option("stringso").string_seq().size())

class iso _TestShortsAdj is UnitTest
  fun name(): String => "ponycli/shorts_adjacent"

  // Rules 2, 3, 5, 6, 8
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS--"; "-I42"; "-F42.0"; "-zaaa"; "-zbbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("--", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0))
    h.assert_eq[String]("bbb", stringso.string_seq()(1))

class iso _TestShortsEq is UnitTest
  fun name(): String => "ponycli/shorts_eq"

  // Rules 2, 3, 5, 7
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS=astring"; "-I=42"; "-F=42.0"; "-z=aaa"; "-z=bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("astring", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0))
    h.assert_eq[String]("bbb", stringso.string_seq()(1))

class iso _TestShortsNext is UnitTest
  fun name(): String => "ponycli/shorts_next"

  // Rules 2, 3, 5, 8
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS"; "--"; "-I"; "42"; "-F"; "42.0"; "-z"; "aaa"; "-z"; "bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("--", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0))
    h.assert_eq[String]("bbb", stringso.string_seq()(1))

class iso _TestLongsEq is UnitTest
  fun name(): String => "ponycli/shorts_eq"

  // Rules 4, 5, 7
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--boolr=true"; "--stringr=astring"; "--intr=42"; "--floatr=42.0"
      "--stringso=aaa"; "--stringso=bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("astring", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0))
    h.assert_eq[String]("bbb", stringso.string_seq()(1))

class iso _TestLongsNext is UnitTest
  fun name(): String => "ponycli/longs_next"

  // Rules 4, 5, 8
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--boolr"; "--stringr"; "--"; "--intr"; "42"; "--floatr"; "42.0"
      "--stringso"; "aaa"; "--stringso"; "bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("--", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0))
    h.assert_eq[String]("bbb", stringso.string_seq()(1))

class iso _TestEnvs is UnitTest
  fun name(): String => "ponycli/envs"

  // Rules
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
    ]
    let envs: Array[String] = [
      "SIMPLE_BOOLR=true"
      "SIMPLE_STRINGR=astring"
      "SIMPLE_INTR=42"
      "SIMPLE_FLOATR=42.0"
    ]
    let cmdErr = CommandParser(cs).parse(args, envs)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("astring", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())

class iso _TestOptionStop is UnitTest
  fun name(): String => "ponycli/option_stop"

  // Rules 2, 3, 5, 7, 9
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS=astring"; "-I=42"; "-F=42.0"
      "--"; "-f=1.0"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("-f=1.0", cmd.arg("words").string())
    h.assert_eq[F64](42.0, cmd.option("floato").f64())

class iso _TestDuplicate is UnitTest
  fun name(): String => "ponycli/duplicate"

  // Rules 4, 5, 7, 10
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--boolr=true"; "--stringr=astring"; "--intr=42"; "--floatr=42.0"
      "--stringr=newstring"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("newstring", cmd.option("stringr").string())

class iso _TestChat is UnitTest
  fun name(): String => "ponycli/chat"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--admin"; "--name=carl"; "say"; "-v80"; "hi"; "yo"; "hey"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = match cmdErr | let c: Command => c else error end

    h.assert_eq[String]("chat/say", cmd.fullname())

    h.assert_eq[String]("say", cmd.spec().name())

    let f1 = cmd.option("admin")
    h.assert_eq[String]("admin", f1.spec().name())
    h.assert_eq[Bool](true, f1.bool())

    let f2 = cmd.option("name")
    h.assert_eq[String]("name", f2.spec().name())
    h.assert_eq[String]("carl", f2.string())

    let f3 = cmd.option("volume")
    h.assert_eq[String]("volume", f3.spec().name())
    h.assert_eq[F64](80.0, f3.f64())

    let a1 = cmd.arg("words")
    h.assert_eq[String]("words", a1.spec().name())
    let words = a1.string_seq()
    h.assert_eq[USize](3, words.size())
    h.assert_eq[String]("hi", words(0))
    h.assert_eq[String]("yo", words(1))
    h.assert_eq[String]("hey", words(2))

class iso _TestMustBeLeaf is UnitTest
  fun name(): String => "ponycli/must_be_leaf"

  // Negative test: can't just supply parent command
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()

    let args: Array[String] = [
      "ignored"
      "--admin"; "--name=carl"; "config"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
    else
      h.fail("expected syntax error for non-leaf command: " + cmdErr.string())
    end

class iso _TestHelp is UnitTest
  fun name(): String => "ponycli/help"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()?

    let chErr = Help.for_command(cs, ["config"; "server"])
    let ch = match chErr | let c: CommandHelp => c else error end

    let help = ch.help_string()
    h.log(help)
    h.assert_true(help.contains("Address of the server"))

primitive _Fixtures
  fun bools_cli_spec(): CommandSpec box ? =>
    """
    Builds and returns the spec for a CLI with four bool options.
    """
    CommandSpec.leaf("bools", "A sample CLI with four bool options", [
      OptionSpec.bool("aaa" where short' = 'a')
      OptionSpec.bool("bbb" where short' = 'b')
      OptionSpec.bool("ccc" where short' = 'c')
      OptionSpec.bool("ddd" where short' = 'd')
    ])?

  fun simple_cli_spec(): CommandSpec box ? =>
    """
    Builds and returns the spec for a CLI with short options of each type.
    """
    CommandSpec.leaf("simple",
        "A sample program with various short options, optional and required", [
      OptionSpec.bool("boolr" where short' = 'B')
      OptionSpec.bool("boolo" where short' = 'b', default' = true)
      OptionSpec.string("stringr" where short' = 'S')
      OptionSpec.string("stringo" where short' = 's', default' = "astring")
      OptionSpec.i64("intr" where short' = 'I')
      OptionSpec.i64("into" where short' = 'i', default' = I64(42))
      OptionSpec.f64("floatr" where short' = 'F')
      OptionSpec.f64("floato" where short' = 'f', default' = F64(42.0))
      OptionSpec.string_seq("stringso" where short' = 'z')
    ], [
      ArgSpec.string("words" where default' = "hello")
      ArgSpec.string_seq("argz")
    ])

  fun chat_cli_spec(): CommandSpec box ? =>
    """
    Builds and returns the spec for a sample chat client's CLI.
    """
    CommandSpec.parent("chat", "A sample chat program", [
      OptionSpec.bool("admin", "Chat as admin" where default' = false)
      OptionSpec.string("name", "Your name" where short' = 'n')
      OptionSpec.f64("volume", "Chat volume" where short' = 'v', default' = 1.0)
    ], [
      CommandSpec.leaf("say", "Say something", Array[OptionSpec](), [
        ArgSpec.string_seq("words", "The words to say")
      ])
      CommandSpec.leaf("emote", "Send an emotion", [
        OptionSpec.f64("speed", "Emote play speed" where default' = F64(1.0))
      ], [
        ArgSpec.string("emotion", "Emote to send")
      ])
      CommandSpec.parent("config", "Configuration commands",
        Array[OptionSpec](), [
        CommandSpec.leaf("server", "Server configuration", Array[OptionSpec](), [
          ArgSpec.string("address", "Address of the server")
        ])?
      ])?
    ])?
