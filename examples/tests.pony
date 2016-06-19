"""
Pony examples tests

This package will eventually test all examples. At the moment we only
use them to type check.
"""

use "ponytest"

// Include all example packages here, even if they don't have tests, so
// that the package loads and them and perform syntax and type checks

// Commented packages are broken at the moment. We have to fix them and
// then keep the tests green.
use ex_circle = "examples/circle"
use ex_counter = "examples/counter"
// use ex_delegates = "examples/delegates"
use ex_echo = "examples/echo"
// use ex_ffi_struct = "examples/ffi-struct"
use ex_files = "examples/files"
use ex_gups_basic = "examples/gups_basic"
use ex_gups_opt = "examples/gups_opt"
use ex_helloworld = "examples/helloworld"
use ex_httpget = "examples/httpget"
use ex_httpserver = "examples/httpserver"
use ex_ifdef = "examples/ifdef"
use ex_lambda = "examples/lambda"
use ex_mailbox = "examples/mailbox"
use ex_mandelbrot = "examples/mandelbrot"
use ex_mixed = "examples/mixed"
use ex_n_body = "examples/n-body"
use ex_net = "examples/net"
use ex_printargs = "examples/printargs"
use ex_producer_consumer = "examples/producer-consumer"
use ex_readline = "examples/readline"
use ex_ring = "examples/ring"
use ex_spreader = "examples/spreader"
use ex_timers = "examples/timers"
// use ex_yield = "examples/yield"


actor Main
  new create(env: Env) =>
    env.out.write("All examples compile!\n")
