struct RuntimeOptions
  """
  Pony struct for the Pony runtime options C struct that can be used to
  override the Pony runtime defaults via code compiled into the program.

  The way this is done is by adding the following function to your `Main` actor:

  ```
    fun @runtime_override_defaults(rto: RuntimeOptions) =>
  ```

  and then overriding the fields of `rto` (the `RuntimeOptions` instance) as
  needed.

  NOTE: Command line arguments still any values set via
        `@runtime_override_defaults`.


  The following example overrides the `--ponyhelp` argument to default it to
  `true` instead of `false` causing the compiled program to always display
  the Pony runtime help:

  ```
  actor Main
    new create(env: Env) =>
      env.out.print("Hello, world.")

    fun @runtime_override_defaults(rto: RuntimeOptions) =>
      rto.ponyhelp = true
  ```
  """

/* NOTE: if you change any of the field docstrings, update `options.h` in
 *       the runtime to keep them in sync.
 */

  var ponymaxthreads: U32 = 0
    """
    Use N scheduler threads. Defaults to the number of cores (not hyperthreads)
    available.
    This can't be larger than the number of cores available.
    """

  var ponyminthreads: U32 = 0
    """
    Minimum number of active scheduler threads allowed.
    Defaults to 0, meaning that all scheduler threads are allowed to be
    suspended when no work is available.
    This can't be larger than --ponymaxthreads if provided, or the physical
    cores available.
    """

  var ponynoscale: Bool = false
    """
    Don't scale down the scheduler threads.
    See --ponymaxthreads on how to specify the number of threads explicitly.
    Can't be used with --ponyminthreads.
    """

  var ponysuspendthreshold: U32 = 0
    """
    Amount of idle time before a scheduler thread suspends itself to minimize
    resource consumption (max 1000 ms, min 1 ms).
    Defaults to 1 ms.
    """

  var ponycdinterval: U32 = 100
    """
    Run cycle detection every N ms (max 1000 ms, min 10 ms).
    Defaults to 100 ms.
    """

  var ponygcinitial: USize = 14
    """
    Defer garbage collection until an actor is using at least 2^N bytes.
    Defaults to 2^14.
    """

  var ponygcfactor: F64 = 2.0
    """
    After GC, an actor will next be GC'd at a heap memory usage N times its
    current value. This is a floating point value. Defaults to 2.0.
    """

  var ponynoyield: Bool = false
    """
    Do not yield the CPU when no work is available.
    """

  var ponynoblock: Bool = false
    """
    Do not send block messages to the cycle detector.
    """

  var ponypin: Bool = false
    """
    Pin scheduler threads to CPU cores. The ASIO thread can also be pinned if
    `--ponypinasio` is set.
    """

  var ponypinasio: Bool = false
    """
    Pin the ASIO thread to a CPU the way scheduler threads are pinned to CPUs.
    Requires `--ponypin` to be set to have any effect.
    """

  var ponyversion: Bool = false
    """
    Print the version of the compiler and exit.
    """

  var ponyhelp: Bool = false
    """
    Print the runtime usage options and exit.
    """
