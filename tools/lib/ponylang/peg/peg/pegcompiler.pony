use "term"
use "collections"

type Defs is Map[String, (Forward, Token)]

primitive PegCompiler
  fun apply(source: Source): (Parser | Array[PegError]) =>
    let p: Parser = PegParser().eof()
    let errors = Array[PegError]

    match p.parse(source)
    | (_, let ast: AST) =>
      let r = _compile_grammar(errors, ast)
      if errors.size() > 0 then
        errors
      else
        r.eof()
      end
    | (let offset: USize, let r: Parser) =>
      errors.push(SyntaxError(source, offset, r))
      errors
    else
      // Unreachable
      NoParser
    end

  fun _compile_grammar(errors: Array[PegError], ast: AST): Parser =>
    let defs = Defs

    for node in ast.children.values() do
      try _forward_definition(errors, defs, node as AST) end
    end

    for node in ast.children.values() do
      try _compile_definition(errors, defs, node as AST) end
    end

    if not defs.contains("start") then
      errors.push(NoStartDefinition)
    end

    if errors.size() == 0 then
      try
        (var start: Parser, _) = defs("start")?
        if defs.contains("hidden") then
          start = start.hide(defs("hidden")?._1)
        end
        return start
      end
    end

    NoParser

  fun _forward_definition(errors: Array[PegError], defs: Defs, ast: AST) =>
    try
      let token = ast.children(0)? as Token
      let ident: String = token.string()

      if defs.contains(ident) then
        (_, let prev) = defs(ident)?
        errors.push(DuplicateDefinition(token, prev))
      else
        defs(ident) = (Forward, token)
      end
    end

  fun _compile_definition(errors: Array[PegError], defs: Defs, ast: AST) =>
    try
      let ident: String = (ast.children(0)? as Token).string()
      let rule = _compile_expr(errors, defs, ast.children(1)?)
      (let p, _) = defs(ident)?

      if not p.complete() then
        let c = ident(0)?
        if (c >= 'A') and (c <= 'Z') then
          p() = rule.term(PegLabel(ident))
        else
          p() =
            match rule
            | let rule': Sequence => rule'.node(PegLabel(ident))
            | let rule': Many => rule'.node(PegLabel(ident))
            else
              rule
            end
        end
      end
    end

  fun _compile_expr(errors: Array[PegError], defs: Defs, node: ASTChild)
    : (Parser ref | NoParser)
  =>
    try
      match node.label()
      | PegChoice =>
        let ast = node as AST
        var p = _compile_expr(errors, defs, ast.children(0)?)
        for i in Range(1, ast.children.size()) do
          p = p / _compile_expr(errors, defs, ast.children(i)?)
        end
        p
      | PegSeq =>
        let ast = node as AST
        var p = _compile_expr(errors, defs, ast.children(0)?)
        for i in Range(1, ast.children.size()) do
          p = p * _compile_expr(errors, defs, ast.children(i)?)
        end
        p
      | PegSkip =>
        -_compile_expr(errors, defs, (node as AST).children(0)?)
      | PegNot =>
        not _compile_expr(errors, defs, (node as AST).children(0)?)
      | PegAnd =>
        not not _compile_expr(errors, defs, (node as AST).children(0)?)
      | PegMany1 =>
        _compile_expr(errors, defs, (node as AST).children(0)?).many1()
      | PegMany =>
        _compile_expr(errors, defs, (node as AST).children(0)?).many()
      | PegSep1 =>
        let ast = node as AST
        let sep = _compile_expr(errors, defs, ast.children(1)?)
        _compile_expr(errors, defs, ast.children(0)?).many1(sep)
      | PegSep =>
        let ast = node as AST
        let sep = _compile_expr(errors, defs, ast.children(1)?)
        _compile_expr(errors, defs, ast.children(0)?).many(sep)
      | PegOpt =>
        _compile_expr(errors, defs, (node as AST).children(0)?).opt()
      | PegRange =>
        let ast = node as AST
        let a = _unescape(ast.children(0)? as Token).utf32(0)?._1
        let b = _unescape(ast.children(1)? as Token).utf32(0)?._1
        R(a, b)
      | PegIdent =>
        let token = node as Token
        let ident: String = token.string()
        if not defs.contains(ident) then
          errors.push(MissingDefinition(token))
          NoParser
        else
          defs(ident)?._1
        end
      | PegAny =>
        R(' ')
      | PegChar =>
        let text = _unescape(node as Token)
        L(text).term(PegLabel(text))
      | PegString =>
        let text = _unescape(node as Token)
        L(text).term(PegLabel(text))
      else
        errors.push(UnknownNodeLabel(node.label()))
        NoParser
      end
    else
      errors.push(MalformedAST)
      NoParser
    end

  fun _unescape(token: Token): String =>
    recover
      let out = String
      var escape = false
      var hex = USize(0)
      var hexrune = U32(0)

      for rune in token.substring(1, -1).runes() do
        if escape then
          match rune
          | '0' => out.append("\0")
          | '"' => out.append("\"")
          | '\\' => out.append("\\")
          | 'a' => out.append("\a")
          | 'b' => out.append("\b")
          | 'f' => out.append("\f")
          | 'n' => out.append("\n")
          | 'r' => out.append("\r")
          | 't' => out.append("\t")
          | 'v' => out.append("\v")
          | '\'' => out.append("'")
          | 'x' => hex = 2
          | 'u' => hex = 4
          | 'U' => hex = 6
          end
        elseif rune == '\\' then
          escape = true
        elseif hex > 0 then
          hexrune = (hexrune << 8) or match rune
          | if (rune >= '0') and (rune <= '9') => rune - '0'
          | if (rune >= 'a') and (rune <= 'f') => (rune - 'a') + 10
          else (rune - 'A') + 10 end
          if (hex = hex - 1) == 1 then
            out.push_utf32(hexrune = 0)
          end
        else
          out.push_utf32(rune)
        end
      end

      out
    end

class val PegLabel is Label
  let _text: String

  new val create(text': String) =>
    _text = text'
  
  fun text(): String => _text
