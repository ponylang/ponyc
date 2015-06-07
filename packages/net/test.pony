use "ponytest"

actor Main
  new create(env: Env) =>
    let test = PonyTest(env)
    test(_TestBuffer)
    test.complete()

class _TestBuffer iso is UnitTest
  """
  Test adding to and reading from a Buffer.
  """
  new iso create() => None

  fun name(): String => "net.buffer"

  fun apply(h: TestHelper): TestResult ? =>
    let b = Buffer

    b.append(recover [as U8:
      0x42,
      0xDE, 0xAD,
      0xAD, 0xDE,
      0xDE, 0xAD, 0xBE, 0xEF,
      0xEF, 0xBE, 0xAD, 0xDE,
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE,
      0xCE, 0xFA, 0xED, 0xFE, 0xEF, 0xBE, 0xAD, 0xDE,
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE,
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE,
      0xCE, 0xFA, 0xED, 0xFE, 0xEF, 0xBE, 0xAD, 0xDE,
      0xCE, 0xFA, 0xED, 0xFE, 0xEF, 0xBE, 0xAD, 0xDE
      ] end)

    b.append(recover [as U8: 'h', 'i'] end)
    b.append(recover [as U8: '\n', 't', 'h', 'e'] end)
    b.append(recover [as U8: 'r', 'e', '\r', '\n'] end)

    h.expect_eq[U8](b.u8(), 0x42)
    h.expect_eq[U16](b.u16_be(), 0xDEAD)
    h.expect_eq[U16](b.u16_le(), 0xDEAD)
    h.expect_eq[U32](b.u32_be(), 0xDEADBEEF)
    h.expect_eq[U32](b.u32_le(), 0xDEADBEEF)
    h.expect_eq[U64](b.u64_be(), 0xDEADBEEFFEEDFACE)
    h.expect_eq[U64](b.u64_le(), 0xDEADBEEFFEEDFACE)
    h.expect_eq[U128](b.u128_be(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.expect_eq[U128](b.u128_le(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.expect_eq[String](b.line(), "hi")
    h.expect_eq[String](b.line(), "there")

    b.append(recover [as U8: 'h', 'i'] end)

    try
      b.line()
      h.assert_failed("shouldn't have a line")
    end

    b.append(recover [as U8: '!', '\n'] end)
    h.expect_eq[String](b.line(), "hi!")

    true
