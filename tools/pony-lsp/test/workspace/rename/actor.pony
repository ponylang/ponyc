actor RenameableActor
  """
  Demonstrates rename of actor behaviours and single-occurrence fields.

  Place the cursor on `work` (be declaration at line 12, col 5) and invoke
  Rename Symbol. Expect 2 edits: the declaration and the call at line 20.

  Place the cursor on `_tag_only` (field declaration at line 10, col 6) and
  invoke Rename Symbol. Expect 1 edit: only the declaration (unused field).
  """
  var _tag_only: U32 = 0

  be work(value: U32) =>
    None

class ActorUser
  """
  Calls behaviours on RenameableActor to provide cross-entity call sites.
  """
  fun use_it(a: RenameableActor) =>
    a.work(0)
