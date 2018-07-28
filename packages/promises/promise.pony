"""
# Promises Package

A `Promise` represents a value that will be available at a later
time. `Promise`s can either be fulfilled with a value or rejected. Any
number of function handlers can be added to the `Promise`, to be
called when the `Promise` is fulfilled or rejected. These handlers
themselves are also wrapped in `Promise`s so that they can be chained
together in order for the fulfilled value of one `Promise` to be used
to compute a value which will be used to fulfill the next `Promise` in
the chain, or so that if the `Promise` is rejected then the subsequent
reject functions will also be called. The input and output types of a
fulfill handler do not have to be the same, so a chain of fulfill
handlers can transform the original value into something new.

Fulfill and reject handlers can either be specified as classes that
implment the `Fulfill` and `Reject` interfaces, or as functions with
the same signatures as the `apply` methods in `Fulfill` and `Reject`.

In the following code, the fulfillment of the `Promise` causes the
execution of several fulfillment functions. The output is:

```
fulfilled + foo
fulfilled + bar
fulfilled + baz
```

```pony
use "promises"

class PrintFulfill is Fulfill[String, String]
  let _env: Env
  let _msg: String
  new create(env: Env, msg: String) =>
    _env = env
    _msg = msg
  fun apply(s: String): String =>
    _env.out.print(" + ".join([s; _msg].values()))
    s

actor Main
  new create(env: Env) =>
     let promise = Promise[String]
     promise.next[String](recover PrintFulfill(env, "foo") end)
     promise.next[String](recover PrintFulfill(env, "bar") end)
     promise.next[String](recover PrintFulfill(env, "baz") end)
     promise("fulfilled")
```

In the following code, the fulfill functions are chained together so
that the fulfilled value of the first one is used to generate a value
which fulfills the second one, which in turn is used to compute a
value which fulfills the third one, which in turn is used to compute a
value which fulfills the fourth one. The output is the average length
of the words passed on the command line or `0` if there are no command
line arguments.

```pony
use "promises"

primitive Computation
  fun tag string_to_strings(s: String): Array[String] val =>
    recover s.split() end
  fun tag strings_to_sizes(sa: Array[String] val): Array[USize] val =>
    recover
      let len = Array[USize]
      for s in sa.values() do
        len.push(s.size())
      end
      len
    end
  fun tag sizes_to_avg(sza: Array[USize] val): USize =>
    var acc = USize(0)
    for sz in sza.values() do
      acc = acc + sz
    end
    acc / sza.size()
  fun tag output(env: Env, sz: USize): None =>
    env.out.print(sz.string())

actor Main
  new create(env: Env) =>
     let promise = Promise[String]
     promise.next[Array[String] val](recover Computation~string_to_strings() end)
            .next[Array[USize] val](recover Computation~strings_to_sizes() end)
            .next[USize](recover Computation~sizes_to_avg() end)
            .next[None](recover Computation~output(env) end)
     promise(" ".join(env.args.slice(1).values()))
```
"""
use "time"

actor Promise[A: Any #share]
  """
  A promise to eventually produce a result of type A. This promise can either
  be fulfilled or rejected.

  Any number of promises can be chained after this one.
  """
  var _value: (_Pending | _Reject | A) = _Pending
  embed _list: Array[_IThen[A]] = _list.create()

  be apply(value: A) =>
    """
    Fulfill the promise.
    """
    if _value isnt _Pending then
      return
    end

    _value = value

    for f in _list.values() do
      f(value)
    end

    _list.clear()

  be reject() =>
    """
    Reject the promise.
    """
    if _value isnt _Pending then
      return
    end

    _value = _Reject

    for f in _list.values() do
      f.reject()
    end

    _list.clear()

  fun tag next[B: Any #share](
    fulfill: Fulfill[A, B],
    rejected: Reject[B] = RejectAlways[B])
    : Promise[B]
  =>
    """
    Chain a promise after this one.

    When this promise is fulfilled, the result of type A is passed to the
    fulfill function, generating in an intermediate result of type B. This
    is then used to fulfill the next promise in the chain.

    If there is no fulfill function, or if the fulfill function raises an
    error, then the next promise in the chain will be rejected.

    If this promise is rejected, this step's reject function is called with no
    input, generating an intermediate result of type B which is used to
    fulfill the next promise in the chain.

    If there is no reject function, of if the reject function raises an error,
    then the next promise in the chain will be rejected.
    """
    let attach = _Then[A, B](consume fulfill, consume rejected)
    let promise = attach.promise()
    _attach(consume attach)
    promise

  fun tag add[B: Any #share = A](p: Promise[B]): Promise[(A, B)] =>
    """
    Add two promises into one promise that returns the result of both when
    they are fulfilled. If either of the promises is rejected then the new
    promise is also rejected.
    """
    let p' = Promise[(A, B)]

    let c =
      object
        var _a: (A | _None) = _None
        var _b: (B | _None) = _None

        be fulfill_a(a: A) =>
          match _b
          | let b: B => p'((a, b))
          else _a = a
          end

        be fulfill_b(b: B) =>
          match _a
          | let a: A => p'((a, b))
          else _b = b
          end
      end

    next[None](
      {(a) => c.fulfill_a(a) },
      {() => p'.reject() })

    p.next[None](
      {(b) => c.fulfill_b(b) },
      {() => p'.reject() })

    p'

  fun tag join(ps: Iterator[Promise[A]]): Promise[Array[A] val] =>
    """
    Create a promise that is fulfilled when the receiver and all promises in
    the given iterator are fulfilled. If the receiver or any promise in the
    sequence is rejected then the new promise is also rejected.
    """
    Promises[A].join(
      [this]
        .> concat(ps)
        .values())

  fun tag select(p: Promise[A]): Promise[(A, Promise[A])] =>
    """
    Return a promise that is fulfilled when either promise is fulfilled,
    resulting in a tuple of its value and the other promise.
    """
    let p' = Promise[(A, Promise[A])]

    let s =
      object tag
        var _complete: Bool = false
        let _p: Promise[(A, Promise[A])] = p'

        be apply(a: A, p: Promise[A]) =>
          if not _complete then
            _p((a, p))
            _complete = true
          end
      end

    next[None]({(a) => s(a, p) })
    p.next[None]({(a)(p = this) => s(a, p) })

    p'

  fun tag timeout(expiration: U64) =>
    """
    Reject the promise after the given expiration in nanoseconds.
    """
    Timers.apply(Timer(
      object iso is TimerNotify
        let _p: Promise[A] = this
        fun ref apply(timer: Timer, count: U64): Bool =>
          _p.reject()
          false
      end,
      expiration))

  be _attach(attach: _IThen[A] iso) =>
    """
    Attaches a step asynchronously. If this promise has already been fulfilled
    or rejected, immediately fulfill or reject the incoming step. Otherwise,
    keep it in a list.
    """
    if _value is _Pending then
      _list.push(consume attach)
    elseif _value is _Reject then
      attach.reject()
    else
      try attach(_value as A) end
    end

primitive Promises[A: Any #share]
  fun join(ps: Iterator[Promise[A]]): Promise[Array[A] val] =>
    """
    Create a promise that is fulfilled when all promises in the given sequence
    are fulfilled. If any promise in the sequence is rejected then the new
    promise is also rejected. The order that values appear in the final array
    is based on when each promise is fulfilled and not the order that they are
    given.
    """
    let p' = Promise[Array[A] val]
    let ps' = Array[Promise[A]] .> concat(consume ps)

    if ps'.size() == 0 then
      p'(recover Array[A] end)
      return p'
    end

    let j = _Join[A](p', ps'.size())
    for p in ps'.values() do
      p.next[None]({(a)(j) => j(a)}, {() => p'.reject()})
    end

    p'

actor _Join[A: Any #share]
  embed _xs: Array[A]
  let _space: USize
  let _p: Promise[Array[A] val]

  new create(p: Promise[Array[A] val], space: USize) =>
    (_xs, _space, _p) = (Array[A](space), space, p)

  be apply(a: A) =>
    _xs.push(a)
    if _xs.size() == _space then
      let len = _xs.size()
      let xs = recover Array[A](len) end
      for x in _xs.values() do
        xs.push(x)
      end
      _p(consume xs)
    end

primitive _None
