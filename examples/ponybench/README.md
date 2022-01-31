# ponybench

A program showing use of the `ponybench` package.

## How to Compile

With a minimal Pony installation, in the same directory as this README file run `ponyc`. You should see content building the necessary packages, which ends with:

```console
...
Generating
 Reachability
 Selector painting
 Data prototypes
 Data types
 Function prototypes
 Functions
 Descriptors
Optimising
Writing ./ponybench.o
Linking ./ponybench
```

## How to Run

Once `ponybench` has been compiled, in the same directory as this README file run `./ponybench`. You should see a benchmark report including: benchmark name, mean, median, deviation, and iterations.

```console
$ ./ponybench
Benchmark results will have their mean and median adjusted for overhead.
You may disable this with --noadjust.

Benchmark                                   mean            median   deviation  iterations
Nothing                                     0 ns              0 ns      ±0.57%     5000000
_Fib(5)                                    48 ns             48 ns      ±1.20%     2000000
_Fib(10)                                  494 ns            495 ns      ±0.39%      300000
_Fib(20)                                63498 ns          63550 ns      ±0.42%        2000
_Timer (10000 ns)                        6025 ns           6060 ns      ±2.66%       20000
```

## Program Modifications

Modify the program to add a benchmark using the math package's `Fibonacci` primitive.

```console
$ ./ponybench
Benchmark results will have their mean and median adjusted for overhead.
You may disable this with --noadjust.

Benchmark                                   mean            median   deviation  iterations
Nothing                                     0 ns              0 ns      ±0.76%     5000000
_Fib(5)                                    39 ns             39 ns      ±0.87%     2000000
_FibStd(5)                                 35 ns             35 ns      ±0.62%     2000000
_Fib(10)                                  467 ns            468 ns      ±0.96%      300000
_FibStd(10)                                63 ns             63 ns      ±0.83%     2000000
_Fib(20)                                59269 ns          59379 ns      ±0.94%        3000
_FibStd(20)                               131 ns            129 ns      ±7.57%     1000000
_Timer(10000 ns)                         6101 ns           6078 ns      ±2.78%       20000
```
