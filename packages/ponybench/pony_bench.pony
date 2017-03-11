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
    bench[USize]("fib 5", {(): USize => Fib(5)})
    bench[USize]("fib 10", {(): USize => Fib(10)})
    bench[USize]("fib 20", {(): USize => Fib(20)})
    bench[USize]("fib 40", {(): USize => Fib(40)})

    // show what happens when a benchmark fails
    bench[String]("fail", {(): String ? => error})

    // async benchmark
    bench.async[USize](
      "async",
      {(): Promise[USize] => Promise[USize].>apply(0)} iso
    )

    // async benchmark timeout
    bench.async[USize](
      "timeout",
      {(): Promise[USize] => Promise[USize]} iso,
      1_000_000
    )

    // benchmarks with set ops
    bench[USize]("add", {(): USize => 1 + 2}, 10_000_000)
    bench[USize]("sub", {(): USize => 2 - 1}, 10_000_000)

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
fib 5       50000000            33 ns/op
fib 10       5000000           371 ns/op
fib 20         30000         45310 ns/op
fib 40             2     684868666 ns/op
**** FAILED Benchmark: fail
async         200000         26512 ns/op
**** FAILED Benchmark: timeout (timeout)
add         10000000             2 ns/op
sub         10000000             2 ns/op
```
"""
use "collections"
use "format"
use "promises"
use "term"

actor PonyBench
  embed _bs: Array[(String, _Benchmark)] = Array[(String, _Benchmark)]
  let _env: Env

  new create(env: Env) =>
    _env = env

  be apply[A: Any #share](name: String, f: {(): A ?} val, ops: U64 = 0) =>
    """
    Benchmark the function `f` by calling it repeatedly and output a simple
    average of the total run time over the amount of times `f` is called. This
    amount can be set manually as `ops` or to the default value of 0 which will
    trigger the benchmark runner to increase the value of `ops` until it is
    satisfied with the stability of the benchmark.
    """
    _add(
      name,
      if ops == 0 then
        _AutoBench[A](name, this, f)
      else
        _Bench[A](name, this, f, ops)
      end
    )

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
    _add(
      name,
      if ops == 0 then
        _AutoBenchAsync[A](name, this, f, timeout)
      else
        _BenchAsync[A](name, this, f, ops, timeout)
      end
    )

  be _result(name: String, ops: U64, nspo: U64) =>
    _remove(name)
    let sl = [name, "\t", Format.int[U64](ops where width=10), "\t",
      Format.int[U64](nspo where width=10), " ns/op"]
    _env.out.print(String.join(sl))
    _next()

  be _failure(name: String, timeout: Bool) =>
    _remove(name)
    let sl = [ANSI.red(), "**** FAILED Benchmark: ", name]
    if timeout then
      sl.push(" (timeout)")
    end
    sl.push(ANSI.reset())
    _env.out.print(String.join(sl))
    _next()

  fun ref _add(name: String, b: _Benchmark) =>
    _bs.push((name, b))
    if _bs.size() < 2 then
      b.run()
    end

  fun ref _remove(name: String) =>
    for (i, (n, _)) in _bs.pairs() do
      if n == name then
        try _bs.delete(i) end
        return
      end
    end

  fun ref _next() =>
    try
      _bs(0)._2.run()
    end

interface tag _BenchNotify
  be _result(name: String, ops: U64, nspo: U64)
  be _failure(name: String, timeout: Bool)

trait tag _Benchmark
  be run()
