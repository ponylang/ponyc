use "collections"

actor Custodian
  let _workers: SetIs[DisposableActor] = _workers.create()

  be apply(worker: DisposableActor) =>
    _workers.set(worker)

  be remove(worker: DisposableActor) =>
    _workers.unset(worker)

  be dispose() =>
    for worker in _workers.values() do
      worker.dispose()
    end

    _workers.clear()
