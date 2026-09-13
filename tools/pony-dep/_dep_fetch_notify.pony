actor _DepFetchNotify is FetchNotify
  """
  Pairs a single `DepEntry` with its `Fetch` call and forwards the
  result to the parent `FetchAll` with the dependency's identity.
  """
  let _parent: FetchAll
  let _dep: DepEntry val

  new create(parent: FetchAll, dep: DepEntry val) =>
    _parent = parent
    _dep = dep

  be fetch_failed(err: FetchError) =>
    _parent._dep_failed(_dep, err)

  be fetch_succeeded() =>
    _parent._dep_succeeded(_dep)
