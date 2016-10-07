"""
The PonyBench package provides a microbenchmarking framework. It is designed to
easily benchmark synchronous and asynchronous operations.

## Example program

The following is a complete program with multiple trivial benchmarks followed
by their output.

```pony
use "ponybench"
use "promises"

actor Main
  new create(env: Env) =>
    let bench = PonyBench(env)

    // benchmark Fib with different inputs
    bench[USize]("fib 5", lambda(): USize => Fib(5) end)
    bench[USize]("fib 10", lambda(): USize => Fib(10) end)
    bench[USize]("fib 20", lambda(): USize => Fib(20) end)
    bench[USize]("fib 40", lambda(): USize => Fib(40) end)

    // show what happens when a benchmark fails
    bench[String]("fail", lambda(): String ? => error end)

    // async benchmark
    bench.async[USize]("async", object iso
      fun apply(): Promise[USize] =>
        let p = Promise[USize]
        p(0)
    end)

    // async benchmark timeout
    bench.async[USize]("timeout", object iso
      fun apply(): Promise[USize] =>
        let p = Promise[USize]
        if false then p(0) else p end
    end, 1_000_000)

    // benchmarks with set ops
    bench[USize]("add", lambda(): USize => 1 + 2 end, 10_000_000)
    bench[USize]("sub", lambda(): USize => 2 - 1 end, 10_000_000)

primitive Fib
  fun apply(n: USize): USize =>
    if n < 2 then
      n
    else
      apply(n-1) + apply(n-2)
    end
```
Output:
```
fib 5	   50000000	        36 ns/op
fib 10	   3000000	       448 ns/op
fib 20	     30000	     58828 ns/op
fib 40	         2	 865802018 ns/op
**** FAILED Benchmark: fail
async	     300000	     10166 ns/op
**** FAILED Benchmark: timeout
add	      10000000	         3 ns/op
sub	      10000000	         3 ns/op
```
"""
use "collections"
use "format"
use "promises"
use "term"
use "time"

actor PonyBench
  embed _bs: Array[(String, {()} val, String)]
  let _env: Env

  new create(env: Env) =>
    (_bs, _env) = (Array[(String, {()} val, String)], env)

  be apply[A: Any #share](name: String, f: {(): A ?} val, ops: U64 = 0) =>
    """
    Benchmark the function `f` by calling it repeatedly and output a simple
    average of the total run time over the amount of times `f` is called. This
    amount can be set manually as `ops` or to the default value of 0 which will
    trigger the benchmark runner to increase the value of `ops` until it is
    satisfied with the stability of the benchmark.
    """
    let bf = recover val
      if ops == 0 then
        lambda()(notify = this, name, f) =>
          _AutoBench[A](notify, name, f)()
        end
      else
        lambda()(notify = this, name, f, ops) =>
          _Bench[A](notify)(name, f, ops)
        end
      end
    end
    _bs.push((name, bf, ""))
    if _bs.size() < 2 then bf() end

  be async[A: Any #share](
    name: String,
    f: {(): Promise[A] ?} val,
    timeout: U64 = 0,
    ops: U64 = 0
  ) =>
    """
    Benchmark the time taken for the promise returned by `f` to be fulfilled.
    If `timeout` is greater than 0, the benchmark will fail if the promise is
    not fulfilled within the time given. This check for timeout is done before
    the benchmarks are counted towards an average run time.
    """
    if timeout > 0 then
      let t = Timer(object iso is TimerNotify
        let _bench: PonyBench = this
        let _name: String = name
        let _f: {(): Promise[A] ?} val = f
        let _ops: U64 = ops
        fun ref apply(timer: Timer ref, count: U64): Bool =>
          _bench._failure(_name)
          false
        fun ref cancel(timer: Timer ref) =>
          _bench.async[A](_name, _f, 0, _ops)
      end, timeout)
      let ts = Timers
      let tt: Timer tag = t
      ts(consume t)
      try
        let p = f()
        p.next[A](recover
          lambda(a: A)(ts, tt): A =>
            ts.cancel(tt)
            a
          end
        end)
      else
        _failure(name)
      end
    end

    let bf = recover val
      if ops == 0 then
        lambda()(notify = this, name, f) =>
          _AutoBench[A](notify, name, f)()
        end
      else
        lambda()(notify = this, name, f, ops) =>
          _BenchAsync[A](notify)(name, f, ops)
        end
      end
    end
    _bs.push((name, bf, ""))
    if _bs.size() < 2 then bf() end

  be _result(name: String, ops: U64, nspo: U64) =>
    let sl = [name, "\t", Format.int[U64](ops where width=10), "\t",
      Format.int[U64](nspo where width=10), " ns/op"]
    _update(name, String.join(sl))
    _next()

  be _failure(name: String) =>
    let sl = [ANSI.red(), "**** FAILED Benchmark: ", name, ANSI.reset()]
    _update(name, String.join(sl))
    _next()

  fun ref _update(name: String, result: String) =>
    try
      for (i, (n, f, s)) in _bs.pairs() do
        if n == name then
          _bs(i) = (n, f, result)
        end
      end
    end

  fun ref _next() =>
    try
      while _bs(0)._3 != "" do
        _env.out.print(_bs.shift()._3)
      end
      if _bs.size() > 0 then
        _bs(0)._2()
      end
    end

interface tag _BenchNotify
  be _result(name: String, ops: U64, nspo: U64)
  be _failure(name: String)
