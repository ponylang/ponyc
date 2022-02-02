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

  fun tag flatten_next[B: Any #share](
    fulfill: Fulfill[A, Promise[B]],
    rejected: Reject[Promise[B]] = RejectAlways[Promise[B]])
    : Promise[B]
  =>
    """
    Chain a promise after this one and unwrap the promise returned from this
    one.

    `flatten_next` is a companion to `next`. It operates in an identical
    fashion except for the type of the fulfilled promise. Whereas `next` takes
    a function that returns a type `B`, `flatten_next` takes a function that
    returns `Promise[B]`.

    Why is `flatten_next` valuable given that next could take a `B` that is of
    a type like `Promise[String]`? Let's start with some code to demonstrate the
    problem that arises when returning `Promise[Promise[B]]` from `next`.

    Let's say we have a library for accessing the GitHub REST API:

    ```pony
    class GitHub
      new create(personal_access_token: String)

      fun get_repo(repo: String): Promise[Repository]

    class Repository
      fun get_issue(number: I64): Promise[Issue]

    class Issue
      fun title(): String
    ```

    And we want to use this promise based API to look up the title of an issue.
    Without `flatten_next`, we could attempt to do the following using `next`:

    ```pony
    actor Main
      new create(env: Env) =>
        let repo: Promise[Repository] =
          GitHub("my token").get_repo("ponylang/ponyc")

        //
        // do something with the repo once the promise is fulfilled
        // in our case, get the issue
        //

        let issue = Promise[Promise[Issue]] =
          repo.next[Promise[Issue]](FetchIssue~apply(1))

        // once we get the issue, print the title
        issue.next[None](PrintIssueTitle~apply(env.out))

    primitive FetchIssue
      fun apply(number: I64, repo: Repository): Promise[Issue] =>
        repo.get_issue(number)

    primitive PrintIssueTitle
      fun apply(out: OutStream, issue: Promise[Issue]) =>
        // O NO! We can't print the title
        // We don't have an issue, we have a promise for an issue
    ```

    Take a look at what happens in the example, when we get to
    `PrintIssueTitle`, we can't print anything because we "don't have anything".
    In order to print the issue title, we need an `Issue` not a
    `Promise[Issue]`.

    We could solve this by doing something like this:

    ```pony
    primitive PrintIssueTitle
      fun apply(out: OutStream, issue: Promise[Issue]) =>
        issue.next[None](ActuallyPrintIssueTitle~apply(out))

    primitive ActuallyPrintIssueTitle
      fun apply(out: OutStream, issue: Issue) =>
        out.print(issue.title())
    ```

    That will work, however, it is kind of awful. When looking at:

    ```pony
        let repo: Promise[Repoository] =
          GitHub("my token").get_repo("ponylang/ponyc")
        let issue = Promise[Promise[Issue]] =
          repo.next[Promise[Issue]](FetchIssue~apply(1))
        issue.next[None](PrintIssueTitle~apply(env.out))
    ```

    it can be hard to follow what is going on. We can only tell what is
    happening because we gave `PrintIssueTitle` a very misleading name; it
    doesn't print an issue title.

    `flatten_next` addresses the problem of "we want the `Issue`, not the
    intermediate `Promise`". `flatten_next` takes an intermediate promise and
    unwraps it into the fulfilled type. You get to write your promise chain
    without having to worry about intermediate promises.

    Updated to use `flatten_next`, our API example becomes:

    ```pony
    actor Main
      new create(env: Env) =>
        let repo: Promise[Repository] =
          GitHub("my token").get_repo("ponylang/ponyc")

        let issue = Promise[Issue] =
          repo.flatten_next[Issue](FetchIssue~apply(1))

        issue.next[None](PrintIssueTitle~apply(env.out))

    primitive FetchIssue
      fun apply(number: I64, repo: Repository): Promise[Issue] =>
        repo.get_issue(number)

    primitive PrintIssueTitle
      fun apply(out: OutStream, issue: Issue) =>
        out.print(issue.title())
    ```

    Our promise `Issue`, is no longer a `Promise[Promise[Issue]]`. By using
    `flatten_next`, we have a much more manageable `Promise[Issue]` instead.

    Other than unwrapping promises for you, `flatten_next` otherwise acts the
    same as `next` so all the same rules apply to fulfillment and rejection.
    """
    let outer = Promise[B]

    next[None](object iso
      var f: Fulfill[A, Promise[B]] = consume fulfill
      let p: Promise[B] = outer

      fun ref apply(value: A) =>
        let fulfill' = f = _PromiseFulFill[A, B]

        try
          let inner = (consume fulfill').apply(value)?

          inner.next[None](
            {(fulfilled: B) => p(fulfilled)},
            {()? => p.reject(); error})
        else
          p.reject()
        end
    end,
    object iso
      var r: Reject[Promise[B]] = consume rejected
      let p: Promise[B] = outer

      fun ref apply() =>
        let rejected' = r = RejectAlways[Promise[B]]

        try
          (consume rejected').apply()?
        else
          p.reject()
        end
    end)

    outer

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

    Join `p1` and `p2` with an existing promise, `p3`.
    ```pony
    use "promises"

    actor Main
      new create(env: Env) =>

        let p1 = Promise[String val]
        let p2 = Promise[String val]
        let p3 = Promise[String val]

        p3.join([p1; p2].values())
          .next[None]({(a: Array[String val] val) =>
            for s in a.values() do
              env.out.print(s)
            end
          })

        p2("second")
        p3("third")
        p1("first")
    ```
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

    Join three existing promises to make a fourth.
    ```pony
    use "promises"

    actor Main
      new create(env: Env) =>

        let p1 = Promise[String val]
        let p2 = Promise[String val]
        let p3 = Promise[String val]

        Promises[String val].join([p1; p2; p3].values())
          .next[None]({(a: Array[String val] val) =>
            for s in a.values() do
              env.out.print(s)
            end
          })

        p2("second")
        p3("third")
        p1("first")
    ```
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


class iso _PromiseFulFill[A: Any #share, B: Any #share] is Fulfill[A, Promise[B]]
  """
  Fulfill discarding its input value of `A` and returning a promise of type `B`.
  """
  new iso create() => None
  fun ref apply(value: A): Promise[B] => Promise[B]
