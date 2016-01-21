use "collections"

actor Custodian
  """
  A Custodian keeps a set of actors to dispose. When the Custodian is disposed,
  it disposes of the actors in its set and then clears the set.
  """
  let _workers: SetIs[DisposableActor] = _workers.create()

  be apply(worker: DisposableActor) =>
    """
    Add an actor to be disposed of.
    """
    _workers.set(worker)

  be remove(worker: DisposableActor) =>
    """
    Removes an actor from the set of things to be disposed.
    """
    _workers.unset(worker)

  be dispose() =>
    """
    Dispose of the actors in the set and then clear the set.
    """
    for worker in _workers.values() do
      worker.dispose()
    end

    _workers.clear()
