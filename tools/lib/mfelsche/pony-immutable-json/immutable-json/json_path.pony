use "peg"
use "debug"
use "itertools"

primitive JsonPath
  fun compile(json_path: String): Query val ? =>
    let parser: Parser val = recover JsonPathParser().eof() end
    let source = Source.from_string(json_path)
    let query = Query
    match parser.parse(source)
    | (_, let ast: AST) =>
        for ast_child in ast.children.values() do
          let segment =
            match ast_child.label()
            | JPChildSegment => ChildSegment
            | JPDescendantSegment => DescendantSegment
            else
              Debug("No correct segment node")
              error
            end
          for selector_ast in (ast_child as AST).children.values() do
            let selector = 
              match selector_ast.label()
              | JPWildcard => WildcardSelector
              | JPSlice =>
                let start: (I64 | None) = 
                  try
                    let start_ast = (selector_ast as AST).children(0)?
                    let _ = start_ast.label() as JPInt
                    (start_ast as Token).string().i64()?
                  end
                let endd: (I64 | None) = 
                  try
                    let end_ast = (selector_ast as AST).children(1)?
                    let _ = end_ast.label() as JPInt
                    (end_ast as Token).string().i64()?
                  end
                let step: (I64 | None) = 
                  try
                    let step_ast = (selector_ast as AST).children(2)?
                    let _ = step_ast.label() as JPInt
                    (step_ast as Token).string().i64()?
                  end
                SliceSelector.create(start, endd, step)
              | JPMemberName =>
                let name = recover val (selector_ast as Token).string() end
                NameSelector.create(name)
              | JPString =>
                let name = JPString.unescape(selector_ast as Token)
                NameSelector.create(name)
              | JPInt =>
                let index = (selector_ast as Token).string().i64()?
                IndexSelector.create(index)
              else
                Debug("Invalid Selector AST Node: " + selector_ast.label().text())
                error
              end
            segment.push(selector)
          end
          query.push(consume segment)
        end
    else
      error
    end
    consume query

  fun apply(
    path: String,
    json: JsonType)
  : Array[JsonType] ?
  =>
    let query = this.compile(path)?
    query.execute(json)


class val Query
  """
  A parsed and compiled JSONPath Query.
  Execute the query against a `JsonType` or a `JsonDoc` to get a (possibly) empty list of
  matching JSON "nodes" back.
  """
  embed segments: Array[Segment] = segments.create(4)

  new trn create() => None

  fun ref push(segment: Segment) =>
    segments.push(segment)

  fun execute(json: JsonType): Array[JsonType] =>
    var node_list: Array[JsonType] = [json]
    
    for segment in segments.values() do
      node_list = segment.execute(node_list)
    end
    node_list


class val ChildSegment
  embed selectors: Array[Selector] = selectors.create(1) // most segments have just 1 selector

  new trn create() => None

  fun ref push(selector: Selector) =>
    selectors.push(selector)

  fun val execute(input: Array[JsonType]): Array[JsonType] =>
    let out = Array[JsonType](selectors.size())
    for input1 in input.values() do
      for selector in selectors.values() do
        out.append(selector.select(input1))
      end
    end
    out

  fun is_descendant(): Bool => false

class val DescendantSegment
  embed selectors: Array[Selector] trn = selectors.create(1) // most segments have just 1 selector

  new trn create() => None

  fun ref push(selector: Selector) =>
    selectors.push(selector)

  fun val execute(input: Array[JsonType]): Array[JsonType] =>
    let out = Array[JsonType](selectors.size() * input.size())
    for input1 in input.values() do
      let enqueued = Array[Iterator[JsonType]].create()
      var current: Iterator[JsonType] = OnceIter[JsonType](input1)
      while true do
        while current.has_next() do
          try
            let input2 = current.next()?
            match input2
            | let obj: JsonObject =>
              enqueued.push(obj.data.values())
            | let arr: JsonArray =>
              enqueued.push(arr.data.values())
            end
            for selector in selectors.values() do
              out.append(selector.select(input2))
            end
          end
        end
        if enqueued.size() > 0 then
          try
            current = enqueued.pop()?
          end
        else
          break
        end
      end
    end
    out

  fun is_descendant(): Bool => true


type Segment is (ChildSegment | DescendantSegment)


class val NameSelector
  """
  Selects a member value of a JSON object with the key `name`
  or nothing if there is no such member value or if the node is not an object.
  """
  let name: String

  new val create(name': String) =>
    name = name'

  fun select(node: JsonType): Array[JsonType] =>
    let out = Array[JsonType].create(1)
    match node
    | let obj: JsonObject =>
      try
        let child = obj.data(name)?
        out.push(child)
      end
    end
    out

class val WildcardSelector
  new val create() => None

  fun select(node: JsonType): Array[JsonType] =>
    match node
    | let obj: JsonObject =>
        Iter[JsonType](obj.data.values()).collect(Array[JsonType].create(obj.data.size()))
    | let arr: JsonArray =>
        Iter[JsonType](arr.data.values()).collect(Array[JsonType].create(arr.data.size()))
    else
      []
    end


class val IndexSelector
  let index: I64

  new val create(index': I64) =>
    index = index'

  fun select(node: JsonType): Array[JsonType] =>
    match node
    | let array: JsonArray =>
      try
        let effective_index = 
          if index >= 0 then
            index.usize()
          else
            // indexing from the back of the array
            // -1 => last element
            // -2 => second-to-last
            // ...
            let abs_idx = index.abs().usize()
            array.data.size() -? abs_idx
          end
        //Debug("INDEX SELECTOR: " + effective_index.string())
        let value = array.data(effective_index)?
        [value]
      else
        []
      end
    else
      []
    end


class val SliceSelector
  let start: (I64 | None)
    """inclusive lower bound"""
  let endd: (I64 | None)
    """exclusive upper bound"""
  let step: I64
    """step (default: 1)"""

  new val create(
    start': (I64 | None) = None,
    endd': (I64 | None) = None,
    step': (I64 | None) = None)
  =>
    start = start'
    endd = endd'
    step = 
      try
        step' as I64
      else
        1
      end

  fun select(node: JsonType): Array[JsonType] =>
    let out = Array[JsonType]
    match node
    | let arr: JsonArray =>
      let len = arr.data.size()
      (let lower, let upper) = 
        _bounds(try
            start as I64
          else
            if step >= 0 then 0 else len.i64() - 1 end
          end, 
          try
            endd as I64
          else
            if step >= 0 then len.i64() else -(len.i64()) - 1 end
          end,
          len.i64())
      //Debug("SLICE SELECTOR: " + lower.string() + ":" + upper.string() + ":" + step.string())
      if step > 0 then
        // moving forward
        var i: USize = lower.usize()
        try
          while i < upper.usize() do
            out.push(arr.data(i)?)
            i = i + step.usize()
          end
        end
      elseif step < 0 then
        // moving backward
        var i: USize = lower.usize()
        try
          while i.i64() > upper do
            out.push(arr.data(i)?)
            i = i - step.abs().usize()
          end
        end
      end
    end
    out

  fun tag _normalize(idx: I64, len: I64): I64 =>
    if idx >= 0 then
      idx
    else
      len + idx
    end

  fun _bounds(start': I64, end': I64, len: I64): (I64, I64) =>
    let n_start = _normalize(start', len)
    let n_end = _normalize(end', len)
    if step >= 0 then
      (n_start.max(0).min(len), n_end.max(0).min(len))
    else
      (n_start.max(-1).min(len - 1), n_end.max(-1).min(len - 1))
    end


type Selector is (WildcardSelector | NameSelector | IndexSelector | SliceSelector)


primitive JsonPathParser
  """
  See:  https://datatracker.ietf.org/doc/draft-ietf-jsonpath-base/21/ 
        Appendix A, Figure 2
  """
  fun apply(): Parser val =>
    recover
      // let logical_expr = Forward

      let whitespace = (L(" ") / L("\t") / L("\n") / L("\r")).many()
      let digit = R('0', '9')
      let alpha = R('A', 'Z') / R('a', 'z')
      let name_first = alpha / L("_") / R(0x80, 0xD7FF) / R(0xE000, 0x10FFFF)
      let name_char = digit / name_first
      let member_name_shorthand = (name_first * name_char.many()).term(JPMemberName)
      let double_quote = L("\"")
      let single_quote = L("'")
      let backslash = L("\\")
      let unescaped = R(0x20, 0x21) / R(0x23, 0x26) / R(0x28, 0x5B) / R(0x5D, 0xD7FF) / R(0xE000, 0x10FFFF)
      let hexdigit = digit / R('A', 'F')
      let high_surrogate = L("D") * (L("8") / L("9") / L("A") / L("B")) * hexdigit * hexdigit
      let low_surrogate = L("D") * (L("C") / L("D") / L("E") / L("F")) * hexdigit * hexdigit
      let non_surrogate = (digit / L("A") / L("B") / L("C") / L("E") / L("F")) * hexdigit * hexdigit * hexdigit
      let hexchar = non_surrogate / (high_surrogate  * L("\\") * L("u") * low_surrogate)
      let escapes = backslash * (L("b") / L("f") / L("n") / L("r") / L("t") / L("/") / L("\\") / (L("u") * hexchar))
      let single_quoted_char = unescaped / double_quote / L("\\'") / escapes
      let double_quoted_char = unescaped / single_quote / L("\\\"") / escapes
      let double_quoted_string = L("\"") * double_quoted_char.many() * L("\"")
      let single_quoted_string = L("'") * single_quoted_char.many() * L("'")
      let string = (double_quoted_string / single_quoted_string).term(JPString)
      let int = (L("0") / (L("-").opt() * R('1', '9') * digit.many())).term(JPInt)
      /* TODO: uncomment once filter-selectors are supported
      *
      let comparison_op = L("==") / L("!=") / L("<=") / L(">=") / L("<") / L(">")
      let bool_literal = (L("true") / L("false")).term(JPBool)
      let null = L("null")
      let frac = L(".") * digit.many1()
      let exp = L("e") * (L("-") / L("+")).opt() * digit.many1()
      let number = ((int / L("-0") * frac.opt() * exp.opt()).term(JPNumber)
      let literal = number / string / bool_literal / null
      let comparable = literal / singular_query
      let comparison_expr = comparable * comarison_op * comparable
      let paren_expr = L("!").opt() * L("(") * logical_expr * L(")")
      let basic_expr = paren_expr / comparison_expr / test_expr
      let logical_and_expr = basic_expr * (L("&&") * basic_expr).many()
      let logical_or_expr = logical_and_expr * (L("||") * logical_and_expr).many()
      let logical_expr() = logical_or_expr
      */
      let name_selector = string
      let wildcard_selector = L("*").term(JPWildcard)
      // slice_selector: start : end : step
      let slice_selector = (int.opt() * -L(":") * int.opt() * (-L(":") * int.opt()).opt()).node(JPSlice)
      let index_selector = int
      
      // TODO: add support for filter selectors and shit
      //let filter_selector = L("?") * logical_expr
      let selector = name_selector / wildcard_selector / slice_selector / index_selector // / filter_selector
      let bracketed_child = (-L("[") * selector.many1(L(",")) * -L("]")).node(JPChildSegment)
      let bracketed_descendant = (-L("[") * selector.many1(L(",")) * -L("]")).node(JPDescendantSegment)
      let child_segment = bracketed_child / (-L(".") * (wildcard_selector / member_name_shorthand)).node(JPChildSegment)
      let descendant_segment = (-L("..") * bracketed_descendant) / (-L("..") *(wildcard_selector / member_name_shorthand)).node(JPDescendantSegment)
      let segment = child_segment / descendant_segment
      let segments = (segment).many().hide(whitespace)
      let jsonpath_query = (-L("$") * segments)

      jsonpath_query
    end

primitive JPMemberName is Label fun text(): String => "MemberName"
primitive JPInt is Label fun text(): String => "Int"
primitive JPBool is Label fun text(): String => "Bool"
primitive JPNumber is Label fun text(): String => "Number"
primitive JPQuery is Label fun text(): String => "Query"
primitive JPChildSegment is Label fun text(): String => "ChildSegment"
primitive JPDescendantSegment is Label fun text(): String => "DescendantSegment"
primitive JPSelector is Label fun text(): String => "Selector"
primitive JPSlice is Label fun text(): String => "Slice"
primitive JPWildcard is Label fun text(): String => "Wildcard"

primitive JPString is Label 
  fun text(): String => "String"

  fun unescape(token: Token): String =>
    let tstr = token.string()
    tstr.strip("\"'")
    _unescape_inner(recover val consume tstr end)

  fun _unescape_inner(inner: String): String =>
    recover val
      var escape = false
      var hex = USize(0)
      var hexrune = U32(0)
      
      let out = String.create(inner.size())
      for rune in inner.runes() do
        if escape then
          match rune
          | 'b' => 
            out.append("\b")
          | 'f' => out.append("\f")
          | 'n' => out.append("\n")
          | 'r' => out.append("\r")
          | 't' => out.append("\t")
          | '/' => out.append("/")
          | '\\' => out.append("\\")
          | '\'' => out.append("'")
          | '"' => out.append("\"")
          | 'u' => hex = 4
          end
          escape = false
        elseif rune == '\\' then
          escape = true
        elseif hex > 0 then
          hexrune = (hexrune << 8) or match rune
          | if (rune >= '0') and (rune <= '9') => rune - '0'
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


class ref OnceIter[T] is Iterator[T]
  let _value: T
  var _emitted: Bool = false

  new ref create(value: T) =>
    _value = consume value

  fun ref has_next(): Bool =>
    not _emitted

  fun ref next(): T =>
    _emitted = true
    _value
