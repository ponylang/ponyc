"""
# Bureaucracy package

"""

use "collections"

actor Custodian
  """
  A Custodian keeps a set of actors to dispose. When the Custodian is disposed,
  it disposes of the actors in its set and then clears the set.

  ## Example program

  Imagine you have a program with 3 actors that you need to shutdown when it
  receives a TERM signal. We can set up a Custodian that knows about each
  of our actors and when a TERM signal is received, is disposed of.

  ```
  use "bureaucracy"
  use "signals"

  actor Main
    new create(env: Env) =>
      let actor1 = Actor1
      let actor2 = Actor2
      let actor3 = Actor3

      let custodian = Custodian
      custodian(actor1)(actor2)(actor3)

      SignalHandler(TermHandler(custodian), Sig.term())

  class TermHandler is SignalNotify
    let _custodian: Custodian

    new iso create(custodian: Custodian) =>
      _custodian = custodian

    fun ref apply(count: U32): Bool =>
      _custodian.dispose()
      true
  ```
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
