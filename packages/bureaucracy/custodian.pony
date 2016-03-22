"""
# Bureaucracy package

It happens to almost every program. It starts small, tiny if you will, like a
village where every actor knows every other actor and shutdown is easy. One day
you realize your program is no longer a cute seaside hamlet, its a bustling
metropolis and you are doing way too much work to keep track of everything. What
do you do? Call for a little bureaucracy.

The bureaucracy contains objects designed to ease your bookkeeping burdens. Need
to shutdown a number of actors together? Check out `Custodian`. Need to keep
track of a lot of stuff and be able to look it up by name? Check out `Registrar`.

Put bureaucracy to use today and before long, you're sprawling metropolis of a
code base will be manageable again in no time.
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

  ```pony
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
