import os
if not __debug__:
    raise SystemExit("Experiment verification requires Python assertions enabled")

import json
from pathlib import Path

d = Path(__file__).resolve().parent
peg = Path(os.environ.get('PEG_DIR', Path.home()/'code/ponylang/peg'))
main = '''
actor Main
  new create(env: Env) =>
    @pony_exitcode(1)
    let large = try env.args(1)? == "large" else false end
    Worker(large)
'''
shell = '''
  new create(large: Bool) =>
    if large then run_large() else run_small() end
'''
def copy_fixture(name, package, declarations, fields, methods):
    path = d / name
    path.mkdir(exist_ok=True)
    code = package + '\nuse @pony_exitcode[None](code: I32)\nuse @puts[I32](s: Pointer[U8] tag)\n'
    code += declarations + main + '\nactor Worker\n' + fields + shell
    for label, count in [('small',8),('large',64)]:
        code += f'\n  be run_{label}() =>\n    start_{label}()\n'+'    batch()\n'*count+'    finish()\n'
        code += f'\n  fun ref start_{label}() => _expected = {count}\n'
    code += methods
    (path/'main.pony').write_text(code)
fields = '''
  var _round: USize = 0
  var _expected: USize = 0
  var _ok: Bool = true
'''
finish = '''
  fun ref finish() =>
    if _ok and (_round == _expected) then
      @puts("verified".cstring())
      @pony_exitcode(0)
    end
'''
copy_fixture('graph','', '''
class Binding
  let factor: USize = 2

class FunctionDef
  let symbol: USize
  let parameter: Binding
  new create(i: USize, b: Binding) => (symbol, parameter) = (i, b)

class Instr
  let opcode: USize
  let operand: USize
  new create(op: USize, value: USize) => (opcode, operand) = (op, value)
''', fields + '''
  let _shared: Binding = Binding
  let _source: Array[FunctionDef] = Array[FunctionDef]
  var _checksum: USize = 0
''', '''
  fun ref batch() =>
    if _source.size() == 0 then
      var i: USize = 0
      while i < 512 do
        _source.push(FunctionDef(i, _shared))
        i = i + 1
      end
    end
    let code = Array[Instr]
    var index: USize = 0
    for def in _source.values() do
      _ok = _ok and (def.symbol == index) and (def.parameter is _shared)
      code.push(Instr(0, def.parameter.factor))
      code.push(Instr(1, def.symbol))
      index = index + 1
    end
    try
      var i: USize = 0
      while i < 512 do
        let mul = code(i * 2)?
        let add = code((i * 2) + 1)?
        let value = (3 * mul.operand) + add.operand
        _ok = _ok and (mul.opcode == 0) and (add.opcode == 1) and (value == (6 + i))
        _checksum = _checksum + value
        i = i + 1
      end
    else _ok = false end
    _round = _round + 1
''' + finish.replace('_ok and','(_checksum == (_expected * ((512 * 6) + ((511 * 512) / 2)))) and _ok and'))
copy_fixture('regex','use "iregex"', '', fields + '''
  var _first: (IRegexp | None) = None
''','''
  fun ref batch() =>
    var i: USize = 0
    while i < 32 do
      let even = ((i % 2) == 0)
      let pattern = if even then "(ab|cd)[0-9]{2}" else "x+y" end
      match IRegexpCompiler.parse(pattern)
      | let r: IRegexp =>
        if even then
          _ok = _ok and r.is_match("ab42") and r.is_match("cd00")
            and (not r.is_match("ab4")) and (not r.is_match("xab42"))
            and (not r.is_match("cd0z")) and r.search("xab42!")
        else
          _ok = _ok and r.is_match("xxxy") and (not r.is_match("ab42"))
            and (not r.is_match("y"))
        end
        if (_round == 0) and (i == 0) then _first = r end
      else _ok = false end
      i = i + 1
    end
    match _first
    | let r: IRegexp => _ok = _ok and r.is_match("ab17") and (not r.is_match("xxxy"))
    else _ok = false end
    _round = _round + 1
''' + finish)
def literal(path):
    return json.dumps(path.read_bytes().decode(), ensure_ascii=False)
copy_fixture('grammar','use "peg"','', fields + '''
  var _first: (Parser val | None) = None
''', '''
  fun ref batch() =>
    let grammar = Source.from_string(GRAMMAR)
    match recover val PegCompiler(grammar) end
    | let p: Parser val =>
      verify(p)
      if _round == 0 then _first = p end
    else _ok = false end
    match _first
    | let p: Parser val => verify(p)
    else _ok = false end
    _round = _round + 1

  fun ref verify(p: Parser val) =>
    let src = Source.from_string(INPUT)
    match recover val p.parse(src) end
    | (let offset: USize, let tree: ASTChild) =>
      let text: String = recover Printer(tree) end
      _ok = _ok and (offset == src.content.size()) and (text == EXPECTED)
    else _ok = false end
'''.replace('GRAMMAR', literal(peg/'examples/json.peg')).replace('INPUT', literal(peg/'test/json_test.json')).replace('EXPECTED', literal(peg/'test/json_out.txt')) + finish)
