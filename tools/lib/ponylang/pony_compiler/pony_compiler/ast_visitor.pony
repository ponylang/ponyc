interface ASTVisitor
  """
  Callback interface for visiting AST nodes depth-first.
  """
  fun ref visit(ast: AST box): VisitResult
    """
    Called for each AST node during depth-first traversal.
    Return `Continue` to keep traversing, `Stop` to halt.
    """

  fun ref leave(ast: AST box): VisitResult =>
    """
    Called after all children of this AST node have been visited.
    Return `Continue` to keep traversing, `Stop` to halt.
    """
    Continue

primitive Continue is Equatable[VisitResult]
  """
  Keep traversing.
  """
  fun string(): String =>
    "Continue"

primitive Stop is Equatable[VisitResult]
  """
  Stop traversing.
  """
  fun string(): String =>
    "Stop"

type VisitResult is (Continue | Stop)
