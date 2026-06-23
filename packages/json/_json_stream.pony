// Internal support types for JsonStreamParser: the grammar frame stack, the
// suspendable leaf-scan state, and a standalone number parser.

primitive _StepGo
  """Dispatch made progress within a value; keep looping in pull()."""

primitive _ScanComplete
primitive _ScanNeedMore
type _ScanOutcome is (_ScanComplete | _ScanNeedMore | JsonParseError)

// --- grammar frames ------------------------------------------------------
//
// One frame per open container, holding the grammar sub-position. The position
// is split so that a post-comma state forbids the closing delimiter, which is
// how trailing commas (`[1,]`, `{"a":1,}`) are rejected.

primitive _ArrValueOrEnd  // after '['; a value or ']'
primitive _ArrValue       // after ','; a value is required, ']' is illegal
primitive _ArrCommaOrEnd  // after a value; ',' or ']'
type _ArrPos is (_ArrValueOrEnd | _ArrValue | _ArrCommaOrEnd)

primitive _ObjKeyOrEnd    // after '{'; a key or '}'
primitive _ObjKey         // after ','; a key is required, '}' is illegal
primitive _ObjColon       // after a key; ':'
primitive _ObjValue       // after ':'; a value
primitive _ObjCommaOrEnd  // after a value; ',' or '}'
type _ObjPos is
  (_ObjKeyOrEnd | _ObjKey | _ObjColon | _ObjValue | _ObjCommaOrEnd)

class _ArrayFrame
  var pos: _ArrPos
  new create(pos': _ArrPos) => pos = pos'

class _ObjectFrame
  var pos: _ObjPos
  new create(pos': _ObjPos) => pos = pos'

type _StreamFrame is (_ArrayFrame | _ObjectFrame)

// --- leaf scans ----------------------------------------------------------
//
// State held across pull() calls while a scalar is being scanned, so a value
// split across a chunk boundary resumes exactly where it stopped.

primitive _StrNormal
primitive _StrEscape
primitive _StrUnicode
type _StrPhase is (_StrNormal | _StrEscape | _StrUnicode)

class _StringScan
  let is_key: Bool
  var buf: String iso
  var phase: _StrPhase
  var hex_value: U32
  var hex_count: U8
  var pending_high: (U32 | None)

  new create(is_key': Bool) =>
    is_key = is_key'
    buf = recover iso String end
    phase = _StrNormal
    hex_value = 0
    hex_count = 0
    pending_high = None

class _NumberScan
  let buf: String ref

  new create() =>
    buf = String

class _KeywordScan
  let word: String
  let value: JsonValue
  var matched: USize

  new create(word': String, value': JsonValue) =>
    word = word'
    value = value'
    matched = 0

type _LeafScan is (_StringScan | _NumberScan | _KeywordScan)

// --- number parsing ------------------------------------------------------

primitive _NumberMalformed

primitive _NumberParse
  """
  Parse a number from its complete text, applying the same RFC 8259 rules as the
  batch JsonTokenParser (leading-zero rejection, >18 integer digits force F64,
  fraction and exponent handling) so streaming and batch agree on every number.
  """
  fun apply(s: String box): (I64 | F64 | _NumberMalformed) =>
    let n = s.size()
    if n == 0 then return _NumberMalformed end

    var i: USize = 0
    var sign: I64 = 1
    if try s(0)? == '-' else false end then
      sign = -1
      i = 1
    end

    // integer part
    let int_start = i
    var integer: I64 = 0
    var int_digits: USize = 0
    try
      while i < n do
        let c = s(i)?
        if (c >= '0') and (c <= '9') then
          integer = (integer * 10) + (c - '0').i64()
          i = i + 1
          int_digits = int_digits + 1
        else
          break
        end
      end
    end
    if int_digits == 0 then return _NumberMalformed end
    if (int_digits > 1) and (try s(int_start)? == '0' else false end) then
      return _NumberMalformed // leading zero
    end
    let force_float = int_digits > 18

    // fractional part
    var has_dot = false
    var frac: F64 = 0
    if (i < n) and (try s(i)? == '.' else false end) then
      has_dot = true
      i = i + 1
      var divisor: F64 = 10
      var frac_digits: USize = 0
      try
        while i < n do
          let c = s(i)?
          if (c >= '0') and (c <= '9') then
            frac = frac + ((c - '0').f64() / divisor)
            divisor = divisor * 10
            i = i + 1
            frac_digits = frac_digits + 1
          else
            break
          end
        end
      end
      if frac_digits == 0 then return _NumberMalformed end
    end

    // exponent part
    var has_exp = false
    var exp: I64 = 0
    if (i < n)
      and (try (s(i)? == 'e') or (s(i)? == 'E') else false end)
    then
      has_exp = true
      i = i + 1
      var exp_sign: I64 = 1
      if i < n then
        match try s(i)? else U8(0) end
        | '+' => i = i + 1
        | '-' => exp_sign = -1; i = i + 1
        end
      end
      var exp_digits: USize = 0
      try
        while i < n do
          let c = s(i)?
          if (c >= '0') and (c <= '9') then
            exp = (exp * 10) + (c - '0').i64()
            i = i + 1
            exp_digits = exp_digits + 1
          else
            break
          end
        end
      end
      if exp_digits == 0 then return _NumberMalformed end
      exp = exp * exp_sign
    end

    // any leftover bytes mean the text was not a valid number
    if i != n then return _NumberMalformed end

    if has_dot or has_exp or force_float then
      let integer_f64 =
        if force_float then
          var r: F64 = 0
          try
            var j = int_start
            while j < (int_start + int_digits) do
              r = (r * 10) + (s(j)? - '0').f64()
              j = j + 1
            end
          end
          r
        else
          integer.f64()
        end
      sign.f64() * (integer_f64 + frac) * F64(10).pow(exp.f64())
    else
      sign * integer
    end
