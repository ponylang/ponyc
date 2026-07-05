use "files"
use "pony_test"
use "promises"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // ANSI escape-string builders (pure, synchronous).
    test(_TestANSICursorMotion)
    test(_TestANSICursorXY)
    test(_TestANSIErase)
    test(_TestANSIToggles)
    test(_TestANSIColors)

    // ANSITerm escape-sequence decoding.
    test(_TestANSITermPassthrough)
    test(_TestANSITermCSINavigation)
    test(_TestANSITermSS3Navigation)
    test(_TestANSITermKeypad)
    test(_TestANSITermFunctionKeys)
    test(_TestANSITermModifiers)
    test(_TestANSITermAltWord)
    test(_TestANSITermUnrecognizedEscape)
    test(_TestANSITermUnknownKeypad)
    test(_TestANSITermParamOverflow)
    test(_TestANSITermPrivateParam)
    test(_TestANSITermControlAbort)
    test(_TestANSITermTimeoutFlush)
    test(_TestANSITermRealTimerFlush)
    test(_TestANSITermDispose)
    test(_TestANSITermPromptForward)
    // Size reporting is posix only (no-op on Windows).
    ifdef posix then
      test(_TestANSITermSize)
    end

    // Readline line editing, history, and tab completion.
    test(_TestReadlineInsertAndCursor)
    test(_TestReadlineDeleteOps)
    test(_TestReadlineCtrlKRefresh)
    test(_TestReadlineUTF8Editing)
    test(_TestReadlineBlockedQueue)
    test(_TestReadlineCtrlDEmptyDisposes)
    test(_TestReadlineRejectDisposes)
    test(_TestReadlineHistoryNavigation)
    test(_TestReadlineHistoryDedup)
    test(_TestReadlineHistoryMaxlen)
    test(_TestReadlineHistoryMissingFile)
    test(_TestReadlineTabNone)
    test(_TestReadlineTabSingle)
    test(_TestReadlineTabCommonPrefix)

// ---------------------------------------------------------------------------
// Shared test doubles.
// ---------------------------------------------------------------------------

primitive \nodoc\ _Bytes
  """
  Copy a String's bytes into a fresh `Array[U8] iso` suitable for
  `ANSITerm.apply`. String literals carry arbitrary bytes (`\x1B`, control
  characters), so they are a convenient way to spell input sequences.
  """
  fun apply(s: String): Array[U8] iso^ =>
    s.clone().iso_array()

actor \nodoc\ _NullSource
  """
  A disposable source for an ANSITerm that does nothing when disposed. Used
  when the test does not observe source disposal.
  """
  be dispose() =>
    None

class \nodoc\ _RecordNotify is ANSINotify
  """
  Records every decoded key event as a descriptor string. `prompt` is the
  barrier: a `term.prompt("")` queued after the input behaviors runs once all
  input has been decoded (FIFO), so it asserts the recorded list against the
  expected one and completes the test. `size` is left as the interface default
  so the constructor's initial size callback is ignored.
  """
  let _h: TestHelper
  let _expected: Array[String] val
  embed _got: Array[String] = Array[String]

  new iso create(h: TestHelper, expected: Array[String] val) =>
    _h = h
    _expected = expected

  fun _mods(ctrl: Bool, alt: Bool, shift: Bool): String =>
    (if ctrl then "1" else "0" end)
      + (if alt then "1" else "0" end)
      + (if shift then "1" else "0" end)

  fun ref apply(term: ANSITerm ref, input: U8) =>
    _got.push("byte " + input.string())

  fun ref up(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("up " + _mods(ctrl, alt, shift))

  fun ref down(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("down " + _mods(ctrl, alt, shift))

  fun ref left(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("left " + _mods(ctrl, alt, shift))

  fun ref right(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("right " + _mods(ctrl, alt, shift))

  fun ref delete(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("delete " + _mods(ctrl, alt, shift))

  fun ref insert(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("insert " + _mods(ctrl, alt, shift))

  fun ref home(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("home " + _mods(ctrl, alt, shift))

  fun ref end_key(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("end " + _mods(ctrl, alt, shift))

  fun ref page_up(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("page_up " + _mods(ctrl, alt, shift))

  fun ref page_down(ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("page_down " + _mods(ctrl, alt, shift))

  fun ref fn_key(i: U8, ctrl: Bool, alt: Bool, shift: Bool) =>
    _got.push("fn" + i.string() + " " + _mods(ctrl, alt, shift))

  fun ref closed() =>
    _got.push("closed")

  fun ref prompt(term: ANSITerm ref, value: String) =>
    _h.assert_array_eq[String](_expected, _got)
    _h.complete(true)

class \nodoc\ _PromptNotify is ANSINotify
  """
  Asserts that `prompt` forwards its value unchanged to the notifier.
  """
  let _h: TestHelper
  let _expected: String

  new iso create(h: TestHelper, expected: String) =>
    _h = h
    _expected = expected

  fun ref prompt(term: ANSITerm ref, value: String) =>
    _h.assert_eq[String](_expected, value)
    _h.complete(true)

class \nodoc\ _SizeNotify is ANSINotify
  """
  Completes when a size callback arrives. Used to assert that constructing an
  ANSITerm reports the terminal size (posix only; the dimensions are
  environment dependent, so they are not asserted).
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref size(rows: U16, cols: U16) =>
    _h.complete(true)

class \nodoc\ _FlushNotify is ANSINotify
  """
  Completes when the buffered ESC byte is flushed back as plain input. Used by
  the real-timer test to prove a partial escape is actually scheduled to flush.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref apply(term: ANSITerm ref, input: U8) =>
    if input == 0x1B then
      _h.complete(true)
    end

// ---------------------------------------------------------------------------
// ANSITerm: escape-sequence decoding.
// ---------------------------------------------------------------------------

class \nodoc\ iso _TestANSITermCSINavigation is UnitTest
  """
  CSI sequences for the navigation keys decode to the matching callback with no
  modifiers.
  """
  fun name(): String => "term/ANSITerm.csi-navigation"

  fun ref apply(h: TestHelper) =>
    let expected = recover val
      ["up 000"; "down 000"; "right 000"; "left 000"; "home 000"; "end 000"]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B[A\x1B[B\x1B[C\x1B[D\x1B[H\x1B[F"))
    term.prompt("")

class \nodoc\ iso _TestANSITermPassthrough is UnitTest
  """
  Bytes that are not part of an escape sequence are passed straight to the
  notifier, and the decoder returns to its ground state after a recognised
  sequence so following plain bytes still pass through.
  """
  fun name(): String => "term/ANSITerm.passthrough"

  fun ref apply(h: TestHelper) =>
    let expected = recover val
      ["byte 97"; "byte 98"; "up 000"; "byte 99"]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("ab\x1B[Ac"))
    term.prompt("")

class \nodoc\ iso _TestANSITermSS3Navigation is UnitTest
  """
  SS3 sequences (ESC O x) decode to the navigation keys and to function keys
  F1-F4, all without modifiers.
  """
  fun name(): String => "term/ANSITerm.ss3-navigation"

  fun ref apply(h: TestHelper) =>
    let expected = recover val
      [ "up 000"; "down 000"; "right 000"; "left 000"; "home 000"; "end 000"
        "fn1 000"; "fn2 000"; "fn3 000"; "fn4 000" ]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes(
      "\x1BOA\x1BOB\x1BOC\x1BOD\x1BOH\x1BOF\x1BOP\x1BOQ\x1BOR\x1BOS"))
    term.prompt("")

class \nodoc\ iso _TestANSITermKeypad is UnitTest
  """
  Keypad sequences (ESC [ n ~) decode to the navigation and editing keys.
  """
  fun name(): String => "term/ANSITerm.keypad"

  fun ref apply(h: TestHelper) =>
    let expected = recover val
      [ "home 000"; "insert 000"; "delete 000"; "end 000"; "page_up 000"
        "page_down 000" ]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B[1~\x1B[2~\x1B[3~\x1B[4~\x1B[5~\x1B[6~"))
    term.prompt("")

class \nodoc\ iso _TestANSITermFunctionKeys is UnitTest
  """
  The keypad function-key codes decode to F1 through F20, exercising multi-digit
  escape-number accumulation.
  """
  fun name(): String => "term/ANSITerm.function-keys"

  fun ref apply(h: TestHelper) =>
    let expected = recover val
      [ "fn1 000"; "fn2 000"; "fn3 000"; "fn4 000"; "fn5 000"; "fn6 000"
        "fn7 000"; "fn8 000"; "fn9 000"; "fn10 000"; "fn11 000"; "fn12 000"
        "fn13 000"; "fn14 000"; "fn15 000"; "fn16 000"; "fn17 000"; "fn18 000"
        "fn19 000"; "fn20 000" ]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes(
      "\x1B[11~\x1B[12~\x1B[13~\x1B[14~\x1B[15~\x1B[17~\x1B[18~\x1B[19~" +
      "\x1B[20~\x1B[21~\x1B[23~\x1B[24~\x1B[25~\x1B[26~\x1B[28~\x1B[29~" +
      "\x1B[31~\x1B[32~\x1B[33~\x1B[34~"))
    term.prompt("")

class \nodoc\ iso _TestANSITermModifiers is UnitTest
  """
  A modifier parameter (ESC [ 1 ; m X) sets the ctrl/alt/shift flags per the
  modifier table; an out-of-range value yields no modifiers; and the modifier
  also applies to keypad sequences.
  """
  fun name(): String => "term/ANSITerm.modifiers"

  fun ref apply(h: TestHelper) =>
    // Mods encode as ctrl,alt,shift. 2=shift, 3=alt, 4=alt+shift, 5=ctrl,
    // 6=ctrl+shift, 7=ctrl+alt, 8=ctrl+alt+shift, then two out-of-range values
    // (9 single digit, 10 multi-digit) both decode to no modifiers.
    let expected = recover val
      [ "up 001"; "up 010"; "up 011"; "up 100"; "up 101"; "up 110"; "up 111"
        "up 000"; "up 000"; "delete 100" ]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes(
      "\x1B[1;2A\x1B[1;3A\x1B[1;4A\x1B[1;5A\x1B[1;6A\x1B[1;7A\x1B[1;8A" +
      "\x1B[1;9A\x1B[1;10A\x1B[3;5~"))
    term.prompt("")

class \nodoc\ iso _TestANSITermAltWord is UnitTest
  """
  ESC b and ESC f are alt-left and alt-right word motions.
  """
  fun name(): String => "term/ANSITerm.alt-word"

  fun ref apply(h: TestHelper) =>
    let expected = recover val ["left 010"; "right 010"] end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1Bb\x1Bf"))
    term.prompt("")

class \nodoc\ iso _TestANSITermUnrecognizedEscape is UnitTest
  """
  A complete but unrecognised escape sequence, or an unreportable Alt+char, is
  discarded; nothing from it reaches the notifier. A plain byte after it still
  passes through.
  """
  fun name(): String => "term/ANSITerm.unrecognized-escape"

  fun ref apply(h: TestHelper) =>
    let expected = recover val
      // ESC x      -> Alt+x, no way to report -> discarded
      // ESC [ Z    -> unrecognised CSI final   -> discarded; Y passes through
      // ESC O z    -> unrecognised SS3 final   -> discarded
      // ESC [1;2 Z -> unrecognised CSI final   -> discarded
      // Only the plain Y survives.
      [ "byte 89" ]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1Bx\x1B[ZY\x1BOz\x1B[1;2Z"))
    term.prompt("")

class \nodoc\ iso _TestANSITermUnknownKeypad is UnitTest
  """
  An unrecognised keypad code is discarded without stalling the decoder: a
  following recognised sequence still decodes.
  """
  fun name(): String => "term/ANSITerm.unknown-keypad"

  fun ref apply(h: TestHelper) =>
    // ESC[16~ has no keypad mapping -> discarded. A stalled decoder would then
    // swallow the plain 'x' as a sequence byte; here it passes through, and the
    // following ESC[3~ decodes cleanly.
    let expected = recover val ["byte 120"; "delete 000"] end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B[16~x\x1B[3~"))
    term.prompt("")

class \nodoc\ iso _TestANSITermParamOverflow is UnitTest
  """
  An oversized numeric parameter is clamped so it cannot wrap into a different
  valid key or modifier: the keypad code is discarded, and the modifier decodes
  as no modifiers rather than aliasing to a real one.
  """
  fun name(): String => "term/ANSITerm.param-overflow"

  fun ref apply(h: TestHelper) =>
    // ESC[289~: 289 would wrap a U8 to 33 (F19); clamped, it is discarded.
    // ESC[1;258A: 258 would wrap to 2 (shift); clamped, it is no modifiers.
    let expected = recover val ["up 000"] end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B[289~\x1B[1;258A"))
    term.prompt("")

class \nodoc\ iso _TestANSITermPrivateParam is UnitTest
  """
  A CSI sequence that carries a private parameter marker (such as `?`) is
  discarded even when it ends in an otherwise-recognised final byte, rather than
  firing that key.
  """
  fun name(): String => "term/ANSITerm.private-param"

  fun ref apply(h: TestHelper) =>
    // ESC[?A: '?' marks the sequence non-standard, so the 'A' final does not
    // fire up; the sequence is discarded and the trailing 'x' passes through.
    let expected = recover val ["byte 120"] end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B[?Ax"))
    term.prompt("")

class \nodoc\ iso _TestANSITermControlAbort is UnitTest
  """
  A control byte in the middle of an escape sequence aborts it and is still
  delivered; a second ESC restarts a fresh sequence rather than leaking the
  rest as text.
  """
  fun name(): String => "term/ANSITerm.control-abort"

  fun ref apply(h: TestHelper) =>
    // ESC[3 then CR     -> sequence aborted, CR delivered (byte 13).
    // ESC O then ESC[A  -> mid-SS3 ESC restarts; up decodes, nothing leaks.
    // ESC then CR       -> aborted at the start, CR delivered.
    // ESC[1 then ESC[A  -> mid-CSI ESC restarts; up decodes, nothing leaks.
    let expected = recover val
      ["byte 13"; "up 000"; "byte 13"; "up 000"]
    end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B[3\r\x1BO\x1B[A\x1B\r\x1B[1\x1B[A"))
    term.prompt("")

class \nodoc\ iso _TestANSITermTimeoutFlush is UnitTest
  """
  When the partial-escape timer fires, the buffered bytes are flushed back to
  the notifier as plain input. The timeout is driven directly here for
  determinism; the real timer is exercised separately.
  """
  fun name(): String => "term/ANSITerm.timeout-flush"

  fun ref apply(h: TestHelper) =>
    let expected = recover val ["byte 27"; "byte 27"; "byte 91"] end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B"))
    term._timeout()
    term.apply(_Bytes("\x1B["))
    term._timeout()
    term.prompt("")

class \nodoc\ iso _TestANSITermRealTimerFlush is UnitTest
  """
  A partial escape is actually scheduled to flush: after feeding a lone ESC and
  waiting for the real 25ms timer, the buffered ESC is flushed back as input.
  This is the only test that waits on wall-clock time.
  """
  fun name(): String => "term/ANSITerm.real-timer-flush"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let term = ANSITerm(_FlushNotify(h), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("\x1B"))

class \nodoc\ iso _TestANSITermDispose is UnitTest
  """
  dispose notifies the notifier of closure exactly once (a second dispose is a
  no-op) and input received after dispose is ignored.
  """
  fun name(): String => "term/ANSITerm.dispose"

  fun ref apply(h: TestHelper) =>
    // "Z" passes through; dispose closes once; the post-dispose "a" is ignored;
    // the second dispose is guarded. closed() and the source disposal share the
    // same guard, so one closed event implies one source disposal.
    let expected = recover val ["byte 90"; "closed"] end
    let timers = Timers
    let term = ANSITerm(_RecordNotify(h, expected), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.apply(_Bytes("Z"))
    term.dispose()
    term.apply(_Bytes("a"))
    term.dispose()
    term.prompt("")

class \nodoc\ iso _TestANSITermPromptForward is UnitTest
  """
  prompt forwards its value to the notifier unchanged.
  """
  fun name(): String => "term/ANSITerm.prompt-forward"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let term = ANSITerm(_PromptNotify(h, "PS> "), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)
    term.prompt("PS> ")

class \nodoc\ iso _TestANSITermSize is UnitTest
  """
  Constructing an ANSITerm reports the terminal size to the notifier. Posix
  only: on Windows the size query is a no-op. The dimensions themselves are
  environment dependent and are not asserted.
  """
  fun name(): String => "term/ANSITerm.size-callback"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let term = ANSITerm(_SizeNotify(h), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(2_000_000_000)

// ---------------------------------------------------------------------------
// ANSI primitive: pure escape-string builders.
// ---------------------------------------------------------------------------

class \nodoc\ iso _TestANSICursorMotion is UnitTest
  """
  The four cursor-motion builders collapse 0 and 1 to the bare form and emit a
  numbered form for n >= 2, each with its own final letter.
  """
  fun name(): String => "term/ANSI.cursor-motion"

  fun apply(h: TestHelper) =>
    // up -> A
    h.assert_eq[String]("\x1B[A", ANSI.up())
    h.assert_eq[String]("\x1B[A", ANSI.up(0))
    h.assert_eq[String]("\x1B[A", ANSI.up(1))
    h.assert_eq[String]("\x1B[2A", ANSI.up(2))
    h.assert_eq[String]("\x1B[5A", ANSI.up(5))
    // down -> B
    h.assert_eq[String]("\x1B[B", ANSI.down())
    h.assert_eq[String]("\x1B[B", ANSI.down(0))
    h.assert_eq[String]("\x1B[B", ANSI.down(1))
    h.assert_eq[String]("\x1B[2B", ANSI.down(2))
    // right -> C
    h.assert_eq[String]("\x1B[C", ANSI.right())
    h.assert_eq[String]("\x1B[C", ANSI.right(0))
    h.assert_eq[String]("\x1B[C", ANSI.right(1))
    h.assert_eq[String]("\x1B[2C", ANSI.right(2))
    // left -> D
    h.assert_eq[String]("\x1B[D", ANSI.left())
    h.assert_eq[String]("\x1B[D", ANSI.left(0))
    h.assert_eq[String]("\x1B[D", ANSI.left(1))
    h.assert_eq[String]("\x1B[2D", ANSI.left(2))

class \nodoc\ iso _TestANSICursorXY is UnitTest
  """
  cursor(x, y) collapses to the home code when both coordinates are <= 1 and
  otherwise emits row before column: "\x1B[y;xH".
  """
  fun name(): String => "term/ANSI.cursor-xy"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("\x1B[H", ANSI.cursor())
    h.assert_eq[String]("\x1B[H", ANSI.cursor(1, 1))
    // Only one coordinate above 1 still triggers the long form.
    h.assert_eq[String]("\x1B[1;2H", ANSI.cursor(2, 1))
    h.assert_eq[String]("\x1B[2;1H", ANSI.cursor(1, 2))
    // Row precedes column in the output even though the call is (x, y).
    h.assert_eq[String]("\x1B[3;2H", ANSI.cursor(2, 3))

class \nodoc\ iso _TestANSIErase is UnitTest
  """
  erase maps each direction to a distinct code and defaults to EraseRight.
  """
  fun name(): String => "term/ANSI.erase"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("\x1B[0K", ANSI.erase())
    h.assert_eq[String]("\x1B[0K", ANSI.erase(EraseRight))
    h.assert_eq[String]("\x1B[1K", ANSI.erase(EraseLeft))
    h.assert_eq[String]("\x1B[2K", ANSI.erase(EraseLine))
    h.assert_eq[String]("\x1B[H\x1B[2J", ANSI.clear())
    h.assert_eq[String]("\x1B[0m", ANSI.reset())

class \nodoc\ iso _TestANSIToggles is UnitTest
  """
  The style toggles emit their on code for a true state and their off code for
  a false state, defaulting to on.
  """
  fun name(): String => "term/ANSI.toggles"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("\x1B[1m", ANSI.bold())
    h.assert_eq[String]("\x1B[1m", ANSI.bold(true))
    h.assert_eq[String]("\x1B[22m", ANSI.bold(false))
    h.assert_eq[String]("\x1B[4m", ANSI.underline(true))
    h.assert_eq[String]("\x1B[24m", ANSI.underline(false))
    h.assert_eq[String]("\x1B[5m", ANSI.blink(true))
    h.assert_eq[String]("\x1B[25m", ANSI.blink(false))
    h.assert_eq[String]("\x1B[7m", ANSI.reverse(true))
    h.assert_eq[String]("\x1B[27m", ANSI.reverse(false))

class \nodoc\ iso _TestANSIColors is UnitTest
  """
  The foreground and background colour builders emit their exact SGR codes,
  including the irregular grey and white codes.
  """
  fun name(): String => "term/ANSI.colors"

  fun apply(h: TestHelper) =>
    let cases =
      [ (ANSI.black(), "\x1B[30m")
        (ANSI.red(), "\x1B[31m")
        (ANSI.green(), "\x1B[32m")
        (ANSI.yellow(), "\x1B[33m")
        (ANSI.blue(), "\x1B[34m")
        (ANSI.magenta(), "\x1B[35m")
        (ANSI.cyan(), "\x1B[36m")
        (ANSI.grey(), "\x1B[90m")
        (ANSI.white(), "\x1B[97m")
        (ANSI.bright_red(), "\x1B[91m")
        (ANSI.bright_green(), "\x1B[92m")
        (ANSI.bright_yellow(), "\x1B[93m")
        (ANSI.bright_blue(), "\x1B[94m")
        (ANSI.bright_magenta(), "\x1B[95m")
        (ANSI.bright_cyan(), "\x1B[96m")
        (ANSI.bright_grey(), "\x1B[37m")
        (ANSI.black_bg(), "\x1B[40m")
        (ANSI.red_bg(), "\x1B[41m")
        (ANSI.green_bg(), "\x1B[42m")
        (ANSI.yellow_bg(), "\x1B[43m")
        (ANSI.blue_bg(), "\x1B[44m")
        (ANSI.magenta_bg(), "\x1B[45m")
        (ANSI.cyan_bg(), "\x1B[46m")
        (ANSI.grey_bg(), "\x1B[100m")
        (ANSI.white_bg(), "\x1B[107m")
        (ANSI.bright_red_bg(), "\x1B[101m")
        (ANSI.bright_green_bg(), "\x1B[102m")
        (ANSI.bright_yellow_bg(), "\x1B[103m")
        (ANSI.bright_blue_bg(), "\x1B[104m")
        (ANSI.bright_magenta_bg(), "\x1B[105m")
        (ANSI.bright_cyan_bg(), "\x1B[106m")
        (ANSI.bright_grey_bg(), "\x1B[47m") ]

    for (actual, expected) in cases.values() do
      h.assert_eq[String](expected, actual)
    end

// ---------------------------------------------------------------------------
// Readline test doubles.
//
// Readline can only be exercised through a real ANSITerm: its input methods
// take an `ANSITerm ref` that exists only inside the actor. So each test builds
// `ANSITerm(Readline(notify, out, ...), source, timers)` and drives it with
// `term.apply(bytes)`. Observation is through the injected ReadlineNotify (the
// dispatched line) and, for the synchronous history tests, the history file.
// ---------------------------------------------------------------------------

actor \nodoc\ _NullOut is OutStream
  """
  An output stream that discards everything. Used when a test observes the
  dispatched line rather than the rendered output.
  """
  be print(data: ByteSeq) => None
  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

actor \nodoc\ _MatchOut is OutStream
  """
  Counts the writes whose content contains a target string, for tests that
  check an editing operation redraws a particular line. `assert_matches` is
  issued from the dispatch notifier, which runs inside the term after every
  write, so it observes the full count without racing the writes.
  """
  let _target: String
  var _matches: USize = 0

  new create(target: String) =>
    _target = target

  be print(data: ByteSeq) => _record(data)
  be write(data: ByteSeq) => _record(data)
  be printv(data: ByteSeqIter) =>
    for d in data.values() do _record(d) end
  be writev(data: ByteSeqIter) =>
    for d in data.values() do _record(d) end
  be flush() => None

  fun ref _record(data: ByteSeq) =>
    let s =
      match data
      | let str: String box => str
      | let arr: Array[U8] val => String.from_array(arr)
      end
    if s.contains(_target) then
      _matches = _matches + 1
    end

  be assert_matches(h: TestHelper, expected: USize) =>
    h.assert_eq[USize](expected, _matches)
    h.complete(true)

class \nodoc\ _MatchNotify is ReadlineNotify
  """
  When a line is dispatched, asks its output stream how many writes matched.
  """
  let _h: TestHelper
  let _out: _MatchOut
  let _expected: USize

  new iso create(h: TestHelper, out: _MatchOut, expected: USize) =>
    _h = h
    _out = out
    _expected = expected

  fun ref apply(line: String, prompt: Promise[String]) =>
    _out.assert_matches(_h, _expected)

actor \nodoc\ _ReadlineDriver
  """
  Feeds a Readline (through an ANSITerm) one scripted line at a time, sending
  the next line only after the previous one has dispatched. Serialising this
  way keeps each line's editing on a fresh cursor: every line dispatches while
  unblocked, so `_handle_line` resets the cursor to 0 before the next line
  begins. (Front-loading all lines would queue the later ones while blocked,
  where `_dispatch` resets the edit buffer but not the cursor position.)
  """
  let _h: TestHelper
  let _steps: Array[(String, String)] val
  var _term: (ANSITerm | None) = None
  var _idx: USize = 0

  new create(h: TestHelper, steps: Array[(String, String)] val) =>
    _h = h
    _steps = steps

  be start(term: ANSITerm) =>
    _term = term
    _feed()

  be dispatched(line: String) =>
    if _idx >= _steps.size() then
      _h.fail("unexpected extra dispatched line: " + line)
      return
    end
    try
      _h.assert_eq[String](_steps(_idx)?._2, line
        where msg = "step " + _idx.string())
    end
    _idx = _idx + 1
    if _idx >= _steps.size() then
      _h.complete(true)
    else
      _feed()
    end

  fun ref _feed() =>
    try
      let input = _steps(_idx)?._1
      match _term
      | let t: ANSITerm =>
        t.prompt("")
        t.apply(_Bytes(input))
      end
    end

class \nodoc\ _DriverNotify is ReadlineNotify
  """
  Forwards each dispatched line to its driver. Does not fulfil the promise: the
  driver owns re-prompting.
  """
  let _driver: _ReadlineDriver

  new iso create(driver: _ReadlineDriver) =>
    _driver = driver

  fun ref apply(line: String, prompt: Promise[String]) =>
    _driver.dispatched(line)

class \nodoc\ _ScriptNotify is ReadlineNotify
  """
  Asserts dispatched lines against an expected list, fulfilling each promise to
  drain the next queued line. Safe only for plain typing (a queued line keeps
  the prior line's cursor position); used for the blocked-queue ordering test.
  """
  let _h: TestHelper
  let _expected: Array[String] val
  var _idx: USize = 0

  new iso create(h: TestHelper, expected: Array[String] val) =>
    _h = h
    _expected = expected

  fun ref apply(line: String, prompt: Promise[String]) =>
    if _idx >= _expected.size() then
      _h.fail("unexpected extra dispatched line: " + line)
      return
    end
    try
      _h.assert_eq[String](_expected(_idx)?, line
        where msg = "line " + _idx.string())
    end
    _idx = _idx + 1
    if _idx >= _expected.size() then
      _h.complete(true)
    else
      prompt("")
    end

class \nodoc\ _LineNotify is ReadlineNotify
  """
  Asserts the first dispatched line equals the expected value and completes. For
  tab-completion tests, `tab` returns a supplied completion list and asserts it
  was handed the expected current edit buffer.
  """
  let _h: TestHelper
  let _expected: String
  let _completions: Array[String] val
  let _tab_input: String

  new iso create(h: TestHelper, expected: String,
    completions: Array[String] val = recover val Array[String] end,
    tab_input: String = "")
  =>
    _h = h
    _expected = expected
    _completions = completions
    _tab_input = tab_input

  fun ref apply(line: String, prompt: Promise[String]) =>
    _h.assert_eq[String](_expected, line)
    _h.complete(true)

  fun ref tab(line: String): Seq[String] box =>
    _h.assert_eq[String](_tab_input, line where msg = "tab input")
    _completions

actor \nodoc\ _DisposeProbe
  """
  A disposable source that completes the test when disposed, observing that the
  term disposes its source.
  """
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  be dispose() =>
    _h.complete(true)

class \nodoc\ _RejectNotify is ReadlineNotify
  """
  Rejects the continuation promise for each dispatched line, which drives the
  term to dispose.
  """
  new iso create() =>
    None

  fun ref apply(line: String, prompt: Promise[String]) =>
    prompt.reject()

class \nodoc\ _NullReadlineNotify is ReadlineNotify
  """
  A notifier that does nothing, for the synchronous history tests that drive a
  Readline directly and never dispatch a line.
  """
  new iso create() =>
    None

primitive \nodoc\ _HistoryFile
  """
  Reads and writes newline-separated history files, matching the format
  Readline's own load and save use.
  """
  fun write(path: FilePath, lines: Array[String] box) =>
    with file = File(path) do
      for line in lines.values() do
        file.print(line)
      end
    end

  fun read(path: FilePath): Array[String] =>
    let result = Array[String]
    with file = File.open(path) do
      for line in file.lines() do
        result.push(consume line)
      end
    end
    result

// ---------------------------------------------------------------------------
// Readline: line editing.
// ---------------------------------------------------------------------------

class \nodoc\ iso _TestReadlineInsertAndCursor is UnitTest
  """
  Insertion, empty-line handling, control-byte filtering, and cursor movement
  (home/end/left/right), each observed through the dispatched line. Cursor
  position is revealed by inserting a marker byte and seeing where it lands.
  """
  fun name(): String => "term/Readline.insert-and-cursor"

  fun ref apply(h: TestHelper) =>
    let steps = recover val
      [ ("abc\n", "abc")                // plain insertion
        ("\nx\n", "x")                  // empty line is not dispatched
        ("a\x03\x1Fb\n", "ab")          // bytes below 0x20 are ignored
        ("abc\x01X\n", "Xabc")          // ctrl-a (home) then marker
        ("abc\x01\x05X\n", "abcX")      // ctrl-e (end) then marker
        ("abc\x02X\n", "abXc")          // ctrl-b (left) then marker
        ("abc\x01\x06X\n", "aXbc")      // ctrl-f (right) then marker
        ("ab\r", "ab") ]                // CR also dispatches
    end
    _DriveReadline(h, steps)

class \nodoc\ iso _TestReadlineDeleteOps is UnitTest
  """
  The deletion and kill operations, each observed through the dispatched line.
  """
  fun name(): String => "term/Readline.delete-ops"

  fun ref apply(h: TestHelper) =>
    let steps = recover val
      [ ("abcX\x7F\n", "abc")           // backspace (DEL)
        ("abcX\x08\n", "abc")           // backspace (ctrl-h)
        ("abc\x01\x7F\n", "abc")        // backspace at column 0 is a no-op
        ("Xabc\x01\x04\n", "abc")       // ctrl-d on a non-empty line deletes
        ("helloworld\x01\x06\x06\x06\x06\x06\x0B\n", "hello") // ctrl-k
        ("junk\x15hello\n", "hello")    // ctrl-u clears the line
        ("ab\x0Cc\n", "abc")            // ctrl-l clears screen, not buffer
        ("foo bar\x17\n", "foo ")       // ctrl-w deletes the previous word
        ("a foo \x17\n", "a ")          // ctrl-w skips a trailing space first
        ("ab\x02\x14\n", "ba")          // ctrl-t transposes
        ("ab\x14\n", "ab")              // ctrl-t at the end is a no-op
        ("abc\x01\x1B[3~\n", "bc") ]    // Delete key (forward delete)
    end
    _DriveReadline(h, steps)

class \nodoc\ iso _TestReadlineCtrlKRefresh is UnitTest
  """
  ctrl-k redraws the line after truncating it, so the deleted text does not stay
  on screen. Typing "ab", moving the cursor left, then ctrl-k truncates to "a";
  the truncated line "a" is redrawn once while typing 'a' and again by ctrl-k,
  so it is rendered twice. Without the ctrl-k redraw it is rendered only once.
  """
  fun name(): String => "term/Readline.ctrl-k-refresh"

  fun ref apply(h: TestHelper) =>
    // A redraw of the buffer "a" writes "a" followed by the erase-to-end code.
    let out = _MatchOut("a" + ANSI.erase())
    let timers = Timers
    let term = ANSITerm(Readline(_MatchNotify(h, out, 2), out), _NullSource,
      timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    term.apply(_Bytes("ab\x02\x0B\n"))

class \nodoc\ iso _TestReadlineUTF8Editing is UnitTest
  """
  Cursor motion and deletion treat a multi-byte UTF-8 character as a unit,
  skipping over continuation bytes so the edit buffer stays valid UTF-8.
  """
  fun name(): String => "term/Readline.utf8-editing"

  fun ref apply(h: TestHelper) =>
    // "é" is 2 UTF-8 bytes (0xC3 0xA9); "€" is 3 (0xE2 0x82 0xAC).
    let steps = recover val
      [ ("aé\x7F\n", "a")           // backspace removes the whole "é"
        ("a€\x7F\n", "a")           // backspace removes the whole 3-byte "€"
        ("é\x02X\n", "Xé")          // left skips "é", marker before it
        ("éb\x01\x1B[3~\n", "b") ]  // Delete removes the whole "é"
    end
    _DriveReadline(h, steps)

primitive \nodoc\ _DriveReadline
  """
  Builds a driver-backed Readline test from a step script.
  """
  fun apply(h: TestHelper, steps: Array[(String, String)] val) =>
    let timers = Timers
    let driver = _ReadlineDriver(h, steps)
    let term = ANSITerm(Readline(_DriverNotify(driver), _NullOut), _NullSource,
      timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(10_000_000_000)
    driver.start(term)

class \nodoc\ iso _TestReadlineBlockedQueue is UnitTest
  """
  Input received while blocked (before the first prompt) is queued and drained
  one line per prompt, in order.
  """
  fun name(): String => "term/Readline.blocked-queue"

  fun ref apply(h: TestHelper) =>
    let expected = recover val ["one"; "two"] end
    let timers = Timers
    let term = ANSITerm(Readline(_ScriptNotify(h, expected), _NullOut),
      _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    // No prompt yet, so both lines queue while blocked.
    term.apply(_Bytes("one\n"))
    term.apply(_Bytes("two\n"))
    // The first prompt drains "one"; the notifier re-prompts to drain "two".
    term.prompt("")

class \nodoc\ iso _TestReadlineCtrlDEmptyDisposes is UnitTest
  """
  ctrl-d on an empty line disposes the term (and therefore its source).
  """
  fun name(): String => "term/Readline.ctrl-d-empty-disposes"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let term = ANSITerm(
      Readline(_NullReadlineNotify, _NullOut), _DisposeProbe(h), timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    term.apply(_Bytes("\x04"))

class \nodoc\ iso _TestReadlineRejectDisposes is UnitTest
  """
  Rejecting the continuation promise disposes the term (and its source).
  """
  fun name(): String => "term/Readline.reject-disposes"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let term = ANSITerm(Readline(_RejectNotify, _NullOut), _DisposeProbe(h),
      timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    term.apply(_Bytes("go\n"))

// ---------------------------------------------------------------------------
// Readline: history.
// ---------------------------------------------------------------------------

class \nodoc\ iso _TestReadlineHistoryNavigation is UnitTest
  """
  A history file is loaded on construction; ctrl-p (up) and ctrl-n (down) walk
  it. Loading three entries then up, up, up, down recalls the second entry.
  """
  var _dir: (FilePath | None) = None

  fun name(): String => "term/Readline.history-navigation"

  fun ref apply(h: TestHelper) ? =>
    let dir = FilePath.mkdtemp(FileAuth(h.env.root), "term.history.")?
    _dir = dir
    let path = dir.join("history")?
    _HistoryFile.write(path, ["alpha"; "beta"; "gamma"])
    let timers = Timers
    let term = ANSITerm(Readline(_LineNotify(h, "beta"), _NullOut, path),
      _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    // ctrl-p thrice reaches "alpha"; ctrl-n once returns to "beta".
    term.apply(_Bytes("\x10\x10\x10\x0E\n"))

  fun ref tear_down(h: TestHelper) =>
    try (_dir as FilePath).remove() end

class \nodoc\ iso _TestReadlineHistoryDedup is UnitTest
  """
  A line identical to the most recent history entry is not added again.
  """
  var _dir: (FilePath | None) = None

  fun name(): String => "term/Readline.history-dedup"

  fun ref apply(h: TestHelper) ? =>
    let dir = FilePath.mkdtemp(FileAuth(h.env.root), "term.history.")?
    _dir = dir
    let path = dir.join("history")?
    let rl: Readline ref = Readline(_NullReadlineNotify, _NullOut, path)
    rl._add_history("a")
    rl._add_history("a")
    rl.closed()
    h.assert_array_eq[String](["a"], _HistoryFile.read(path))

  fun ref tear_down(h: TestHelper) =>
    try (_dir as FilePath).remove() end

class \nodoc\ iso _TestReadlineHistoryMaxlen is UnitTest
  """
  With a bounded history, the oldest entry is trimmed once the limit is
  reached.
  """
  var _dir: (FilePath | None) = None

  fun name(): String => "term/Readline.history-maxlen"

  fun ref apply(h: TestHelper) ? =>
    let dir = FilePath.mkdtemp(FileAuth(h.env.root), "term.history.")?
    _dir = dir
    let path = dir.join("history")?
    let rl: Readline ref = Readline(_NullReadlineNotify, _NullOut, path, 2)
    rl._add_history("a")
    rl._add_history("b")
    rl._add_history("c")
    rl.closed()
    h.assert_array_eq[String](["b"; "c"], _HistoryFile.read(path))

  fun ref tear_down(h: TestHelper) =>
    try (_dir as FilePath).remove() end

class \nodoc\ iso _TestReadlineHistoryMissingFile is UnitTest
  """
  Constructing with a history path that does not exist yields an empty history
  rather than an error.
  """
  var _dir: (FilePath | None) = None

  fun name(): String => "term/Readline.history-missing-file"

  fun ref apply(h: TestHelper) ? =>
    let dir = FilePath.mkdtemp(FileAuth(h.env.root), "term.history.")?
    _dir = dir
    let path = dir.join("does-not-exist")?
    let rl: Readline ref = Readline(_NullReadlineNotify, _NullOut, path)
    // Saving an empty history writes an empty file, confirming load found
    // nothing rather than partially populating or erroring.
    rl.closed()
    h.assert_array_eq[String](Array[String], _HistoryFile.read(path))

  fun ref tear_down(h: TestHelper) =>
    try (_dir as FilePath).remove() end

// ---------------------------------------------------------------------------
// Readline: tab completion.
// ---------------------------------------------------------------------------

class \nodoc\ iso _TestReadlineTabNone is UnitTest
  """
  Tab with no completions leaves the edit buffer unchanged.
  """
  fun name(): String => "term/Readline.tab-none"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let notify = _LineNotify(h, "ab" where tab_input = "ab")
    let term = ANSITerm(Readline(consume notify, _NullOut), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    term.apply(_Bytes("ab\x09\n"))

class \nodoc\ iso _TestReadlineTabSingle is UnitTest
  """
  Tab with a single completion replaces the edit buffer with it.
  """
  fun name(): String => "term/Readline.tab-single"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let notify = _LineNotify(h, "hello", ["hello"] where tab_input = "h")
    let term = ANSITerm(Readline(consume notify, _NullOut), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    term.apply(_Bytes("h\x09\n"))

class \nodoc\ iso _TestReadlineTabCommonPrefix is UnitTest
  """
  Tab with several completions sets the edit buffer to their common prefix.
  """
  fun name(): String => "term/Readline.tab-common-prefix"

  fun ref apply(h: TestHelper) =>
    let timers = Timers
    let notify = _LineNotify(h, "hel", ["hello"; "help"] where tab_input = "h")
    let term = ANSITerm(Readline(consume notify, _NullOut), _NullSource, timers)
    h.dispose_when_done(term)
    h.dispose_when_done(timers)
    h.long_test(5_000_000_000)
    term.prompt("")
    term.apply(_Bytes("h\x09\n"))
