type _Selector is
  ( _NameSelector
  | _IndexSelector
  | _WildcardSelector
  | _SliceSelector
  | _FilterSelector
  )

class val _NameSelector
  """Select an object member by key name."""
  let _name: String

  new val create(name': String) =>
    _name = name'

  fun select(node: JsonValue, out: Array[JsonValue] ref) =>
    match node
    | let obj: JsonObject =>
      try out.push(obj(_name)?) end
    end

class val _IndexSelector
  """Select an array element by index. Supports negative indices."""
  let _index: I64

  new val create(index': I64) =>
    _index = index'

  fun select(node: JsonValue, out: Array[JsonValue] ref) =>
    match node
    | let arr: JsonArray =>
      let effective = if _index >= 0 then
        _index.usize()
      else
        let abs_idx = _index.abs().usize()
        if abs_idx <= arr.size() then
          arr.size() - abs_idx
        else
          return
        end
      end
      try out.push(arr(effective)?) end
    end

primitive _WildcardSelector
  """Select all children of an object or array."""

  fun select(node: JsonValue, out: Array[JsonValue] ref) =>
    match node
    | let obj: JsonObject =>
      for v in obj.values() do out.push(v) end
    | let arr: JsonArray =>
      for v in arr.values() do out.push(v) end
    end

class val _SliceSelector
  """
  Select a range of array elements with optional step.

  Implements RFC 9535 Section 2.3.4.2 slice semantics: [start:end:step]
  where start is inclusive and end is exclusive. Missing start, end, and
  step use defaults that depend on step direction. Negative values wrap
  from the end. Step of 0 produces no results.
  """
  let _start: (I64 | None)
  let _end: (I64 | None)
  let _step: (I64 | None)

  new val create(
    start': (I64 | None),
    end': (I64 | None),
    step': (I64 | None) = None)
  =>
    _start = start'
    _end = end'
    _step = step'

  fun select(node: JsonValue, out: Array[JsonValue] ref) =>
    match node
    | let arr: JsonArray =>
      let len = arr.size().i64()
      let step = match _step | let n: I64 => n else I64(1) end

      if step == 0 then return end

      let s = if step >= 0 then
        match _start | let n: I64 => n else I64(0) end
      else
        match _start | let n: I64 => n else len - 1 end
      end
      let e = if step >= 0 then
        match _end | let n: I64 => n else len end
      else
        match _end | let n: I64 => n else (-len) - 1 end
      end

      let n_start = _normalize(s, len)
      let n_end = _normalize(e, len)

      if step > 0 then
        let lower = n_start.max(0).min(len)
        let upper = n_end.max(0).min(len)
        var i = lower
        while i < upper do
          try out.push(arr(i.usize())?) end
          i = i + step
        end
      else
        let upper = n_start.max(-1).min(len - 1)
        let lower = n_end.max(-1).min(len - 1)
        var i = upper
        while lower < i do
          try out.push(arr(i.usize())?) end
          i = i + step
        end
      end
    end

  fun _normalize(idx: I64, len: I64): I64 =>
    if idx >= 0 then idx else len + idx end

class val _FilterSelector
  """
  Select elements of an array or object that satisfy a filter expression.

  For arrays, each element is tested against the expression; matching
  elements are selected in order. For objects, each value is tested;
  matching values are selected.

  Unlike other selectors, `select` takes an additional `root` parameter
  for resolving absolute queries (`$`) within filter expressions. This
  difference is handled by explicit match dispatch in `_JsonPathEval._select_all`.
  """
  let _expr: _LogicalExpr

  new val create(expr': _LogicalExpr) =>
    _expr = expr'

  fun select(node: JsonValue, root: JsonValue, out: Array[JsonValue] ref) =>
    match node
    | let arr: JsonArray =>
      for v in arr.values() do
        if _FilterEval(_expr, v, root) then
          out.push(v)
        end
      end
    | let obj: JsonObject =>
      for v in obj.values() do
        if _FilterEval(_expr, v, root) then
          out.push(v)
        end
      end
    end
