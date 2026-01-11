use @ponyint_hash_block[USize](ptr: Pointer[None] tag, size: USize)
use "collections"
use "debug"
use "binarysearch"

class val Module is (Hashable & Equatable[Module])
  let ast: AST val
  let file: String val
    """Absolute path to the file of this module"""
  let len: USize
    """length of the file contents in bytes"""
  let _hash: USize
    """
    A hash of the module contents.
    """

  // this one is just kept around so the underlying AST is not lost
  // it is reaped when the Program is collected by GC
  let _program: Program box

  new val create(program: Program val, ast': AST) ? =>
    """
    Construct the module by extracting some metadata from the AST
    """
    _program = program
    ast = ast'
    let source: _Source = ast.source().apply()?
    let f_ptr = source.file
    file = recover val String.copy_cstring(f_ptr) end
    len = source.len
    _hash = @ponyint_hash_block(source.m, len)
  
  fun val create_position_index(): PositionIndex val =>
    PositionIndex.create(this)

  fun box hash(): USize =>
    """Return a hash of the file contents"""
    _hash
    
  fun eq(that: box->Module): Bool =>
    """
    Two modules are considered equal if they are from the same file
    and the file contents are equal
    """
    let same_file = (this.file == that.file)
    let same_content = this._hash == that._hash
    same_file and same_content


class _ModuleIter is Iterator[Module val]
  var _module_ast: (AST val | None)
  let _program: Program val

  new ref create(program: Program val, package: Package val) =>
    _program = program
    _module_ast = package.ast.child()

  fun ref has_next(): Bool =>
    _module_ast isnt None

  fun ref next(): Module val ? =>
    let module_ast = _module_ast as AST
    _module_ast = module_ast.sibling()
    Module.create(_program, module_ast)?


class val PositionIndex
  """
  Index for AST nodes sorted by their position in the index.


  """
  // IMPORTANT: keep the reference to the module alive,
  // as long as we keep this class around
  let _module: Module val
  let _index: Array[_Entry] box
    """
    Sorted by position
    """

  new val create(module: Module val) =>
    _module = module
    let visitor = _PositionIndexBuilder
    _module.ast.visit(visitor)
    _index = visitor.index()

  fun debug(out: OutStream) =>
    for entry in _index.values() do
      try
        out.print(entry.best()?.debug())
      end
    end

  fun find_node_at(line: USize, pos: USize): (AST box | None) =>
    let needle = _Entry.empty(line, pos)
    match BinarySearch.apply[_Entry](needle, _index)
    | (let found_pos: USize, true) =>
      // exact match, return the first item
      try
        let entry = _index(found_pos)?
        _refine_node(entry.best()?)
      end
    | (let insert_pos: USize, false) =>
      // check the index we shall insert ourselves at
      if (insert_pos > 0) and  (_index.size() > 0) then
        try
          // check the entry before
          let entry_before = _index(insert_pos - 1)?
          for candidate in entry_before.candidates() do
            // take the first entry that contains our position
            (let start_pos, let end_pos) = candidate.span()

            Debug("candidate: " + candidate.debug() + " end_pos: " + end_pos.string())
            if (start_pos <= needle.start) and (needle.start <= end_pos) then
              // refine to a meaningful node
              return _refine_node(candidate)
            end
          end
        end
      end
    end

  fun _refine_node(node: AST box): AST box =>
    match node.id()
    | TokenIds.tk_positionalargs() =>
      // first child is TK_SEQ, first child of which is the first param
      try (node.child() as AST box).child() end // go to first parameter
    | TokenIds.tk_none() =>
      try
        let parent' = node.parent() as AST
        match parent'.id()
        | TokenIds.tk_fun() | TokenIds.tk_be() | TokenIds.tk_new()
        | TokenIds.tk_use()
        | TokenIds.tk_nominal() =>
          return parent'
        end
      end
    | TokenIds.tk_id() =>
      match node.parent()
      | let parent': AST box =>
        match parent'.id()
        | TokenIds.tk_param() // name of a parameter
        | TokenIds.tk_fun() | TokenIds.tk_be() | TokenIds.tk_new() // function name
        | TokenIds.tk_funref() | TokenIds.tk_beref() | TokenIds.tk_newref() | TokenIds.tk_newberef() // reference to function
        | TokenIds.tk_funchain() | TokenIds.tk_bechain()
        | TokenIds.tk_typeref() // reference to type
        | TokenIds.tk_typeparamref() // reference to generic type parameter
        | TokenIds.tk_nominal() // name of a type
        | TokenIds.tk_flet() | TokenIds.tk_fvar() | TokenIds.tk_embed() // fields
        | TokenIds.tk_fletref() | TokenIds.tk_fvarref() | TokenIds.tk_embedref() // references to fields
        | TokenIds.tk_paramref()
        | TokenIds.tk_var() | TokenIds.tk_let() // local variables
        | TokenIds.tk_packageref() // package reference
        => return parent'
        else
          Debug("Parent unknown: " + TokenIds.string(parent'.id()))
        end
      end
    | TokenIds.tk_seq() | TokenIds.tk_params() =>
      // assuming when we get a seq (a block of expressions) or params
      // we almost always want the first element
      node.child()
    | TokenIds.tk_tag() | TokenIds.tk_iso() | TokenIds.tk_trn() | TokenIds.tk_val() | TokenIds.tk_box() | TokenIds.tk_ref() =>
      // capabilities on types go to the type
      // capabilities on definitions go to the definition
      match node.parent()
      | let parent': AST box =>
        match parent'.id()
        | TokenIds.tk_actor() | TokenIds.tk_class() | TokenIds.tk_struct()
        | TokenIds.tk_primitive() | TokenIds.tk_trait() | TokenIds.tk_interface()
        | TokenIds.tk_nominal()
        | TokenIds.tk_new() | TokenIds.tk_fun() | TokenIds.tk_be() =>
          return parent'
        end
      end
    | TokenIds.tk_int() =>
      // special case for tuple element references where the RHS is rewritten as
      // TK_INT
      match node.parent()
      | let parent': AST box =>
        match parent'.id()
        | TokenIds.tk_tupleelemref() =>
          return parent'
        end
      end
    | TokenIds.tk_call() =>
      // simple type names are desugared to a call of the form TypeName.create()
      // with all children of the TK_CALL being at the same position
      // if we detect this case, we refine to the constructor
      match node.child()
      | let child': AST box =>
        match child'.id()
        | TokenIds.tk_newref() if node.position() == child'.position() =>
          return child'
        end
      end
    end
    node

class ref _PositionIndexBuilder is ASTVisitor
  let _index: Array[_Entry] ref

  new ref create() =>
    _index = Array[_Entry].create(128)

  fun ref visit(ast: AST box): VisitResult =>
    if not ast.is_abstract() then
      // handle special cases
      try
        match ast.id()
        | TokenIds.tk_this() =>
          // detect synthetically added `this.` prefixed to field references and
          // skip them, as they don't appear in the sources
          let par = ast.parent() as AST box
          match par.id()
          | TokenIds.tk_fvarref() | TokenIds.tk_fletref() | TokenIds.tk_embedref() =>
            let sibl = ast.sibling() as AST box
            if ast.position() == sibl.position() then
              return Continue
            end
          end
        | TokenIds.tk_fvarref() | TokenIds.tk_fletref() | TokenIds.tk_embedref() =>
          // exclude artificially created field access nodes,
          // added when a field is referenced without explicit `this.`
          let pos = ast.position()
          let rhs = (ast.child() as AST box).sibling() as AST box
          if rhs.position() == pos then
            return Continue
          end
        end
      end
      let entry = _Entry(ast)
      match BinarySearch.apply[_Entry](entry, _index)
      | (let pos: USize, true) =>
        try
          let existing = _index(pos)?
          existing.merge(entry)
        else
          Debug("Binarysearch found an invalid existing position")
        end
      | (let pos: USize, false) =>
        try
          _index.insert(pos, entry)?
        else
          Debug("Binarysearch found an invalid insert position")
        end
      end
    end
    Continue

  fun index(): this->Array[_Entry] =>
    _index

class ref _Entry is Comparable[_Entry]
  """
  Entry in the PositionIndex, containing possibly multiple AST nodes per
  position.
  """
  let start: Position
  let _candidates: Array[AST box] ref

  new ref create(ast: AST box) =>
    start = ast.position()
    _candidates = Array[AST box].create(1)
    _candidates.push(ast)

  new ref empty(line: USize, pos: USize) =>
    start = Position(line, pos)
    _candidates = Array[AST box].create()

  fun ref merge(other: _Entry) =>
    for oc in other._candidates.values() do
      _candidates.push(oc)
    end

  fun first(): AST box ? =>
    _candidates(0)?

  fun best(): AST box ? =>
    if start == Position(1, 1) then
      // nasty ponyc added a `use "builtin"` at the beginning
      // chose the next node
      _candidates(2)?
    elseif _candidates.size() > 1 then
      ifdef debug then
        var s = String .> append("[")
        for c in _candidates.values() do
          s.append(c.debug())
          s.append(",")
        end
        s.append("]")
        Debug("CANDIDATES: " + s)
      end
      first()?
    else
      // first is best
      first()?
    end

  fun candidates(): Iterator[AST box] =>
    _candidates.values()

  fun eq(other: box->_Entry): Bool =>
    """
    delegated to Position
    """
    start == other.start

  fun lt(other: box->_Entry): Bool =>
    """
    delegated to Position
    """
    start < other.start

