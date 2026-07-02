use ".."
use "json"
use "pony_compiler"

primitive SelectionRanges
  """
  Builds a textDocument/selectionRange response for a single cursor position.

  The result is a linked-list SelectionRange object — innermost range first,
  with each `parent` field pointing to the next wider ancestor — or None
  (JSON null) if the cursor is not on any AST node.

  Ancestor nodes with identical spans are collapsed: when a parent produces
  the same (start, end) as its child, it is skipped. This eliminates
  duplicate ranges produced by thin wrapper nodes such as single-child
  `tk_seq` without needing to enumerate specific token ids.

  Span computation uses a source-file-filtered visitor that excludes
  descendants from other files (e.g. trait methods merged into the class
  AST by ponyc). This prevents positions from stdlib or trait source files
  from inflating the ranges of the user's own nodes.

  The parent walk stops before `tk_package` and `tk_program` nodes, which
  span the entire compiled program rather than a single file. `tk_module`
  nodes are included — their span covers the whole current file, producing
  a useful "select all" outermost range.
  """

  fun collect(node: AST box, doc_path: String val): JsonValue =>
    """
    Build the SelectionRange chain starting at `node` and walking to the
    root via `parent()`. Returns the innermost SelectionRange with ancestors
    embedded in successive `parent` fields, or None if no span is found.
    """
    // 1. Collect the ancestor chain, innermost first.
    let chain: Array[AST box] = Array[AST box].create(16)
    var current: (AST box | None) = node
    while current isnt None do
      match \exhaustive\ current
      | let n: AST box =>
        let nid = n.id()
        // Stop before nodes that span the whole program.
        if (nid == TokenIds.tk_package()) or (nid == TokenIds.tk_program()) then
          break
        end
        chain.push(n)
        current = n.parent()
      | None =>
        break
      end
    end

    // 2. Build the JSON linked list from outermost to innermost, nesting as
    // we go. Walk `chain` in reverse (index chain.size()-1 down to 0).
    var result: JsonValue = None
    var last_sl: USize = 0
    var last_sc: USize = 0
    var last_el: USize = 0
    var last_ec: USize = 0

    // TODO(perf): ASTSourceSpan traverses each ancestor's entire subtree, so
    // total work is O(sum of subtree sizes) ≈ O(depth * module_node_count).
    // A bottom-up approach — one full traversal recording spans by node, then
    // reading cached values per chain entry — would reduce this to O(N).
    var i = chain.size()
    while i > 0 do
      i = i - 1
      let n =
        try
          chain(i)?
        else
          continue
        end
      match \exhaustive\ ASTSourceSpan(n, doc_path, SiblingBound(n))
      | (let s: Position, let e: Position) =>
        // Note: no clamped-start correction here, unlike _symbol_ranges and
        // _node_location. Selection ranges must grow outward — an ancestor's
        // range legitimately starts before n.position() when real descendants
        // (e.g. earlier tokens in a sequence) extend the span backwards.
        // Clamping to n.position() would move a parent's start forward past
        // its own child's start, breaking the LSP containment requirement.
        //
        // Deduplicate: skip if this span is identical to the last emitted.
        let sl = s.line()
        let sc = s.column()
        let el = e.line()
        let ec = e.column()
        if (sl == last_sl) and (sc == last_sc)
          and (el == last_el) and (ec == last_ec)
        then
          continue
        end
        last_sl = sl
        last_sc = sc
        last_el = el
        last_ec = ec
        let range_json =
          LspPositionRange(
            LspPosition.from_ast_pos(s),
            LspPosition.from_ast_pos_end(e))
          .to_json()
        result =
          match \exhaustive\ result
          | None =>
            JsonObject.update("range", range_json)
          | let parent_json: JsonValue =>
            JsonObject
              .update("range", range_json)
              .update("parent", parent_json)
          end
      | None =>
        None  // inverted or empty span — skip this ancestor
      end
    end
    result
