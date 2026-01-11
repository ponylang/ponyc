type ASTChild is (AST | Token | NotPresent)

class val AST
  let _label: Label
  embed children: Array[ASTChild] = children.create()

  new iso create(label': Label = NoLabel) =>
    _label = label'

  fun ref push(some: ASTChild) =>
    children.push(some)

  fun label(): Label => _label

  fun size(): USize => children.size()

  fun extract(): ASTChild =>
    try children(0)? else NotPresent end
