actor Leaf
  new create() =>
    None

actor Branch
  let _left: Leaf
  let _right: Leaf

  new create() =>
    _left = Leaf
    _right = Leaf

actor Main
  new create(env: Env) =>
    let b1 = Branch
    let b2 = Branch
    None
