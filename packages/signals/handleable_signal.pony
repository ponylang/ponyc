use "constrained_types"

primitive HandleableSignalValidator is Validator[U32]
  """
  Validates that a signal number is handleable via the ASIO mechanism.

  Only signals that can be safely caught and dispatched to Pony actors
  are accepted. Fatal signals (SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS,
  SIGSEGV), uncatchable signals (SIGKILL, SIGSTOP), and unknown signal
  numbers are rejected. SIGUSR2 is accepted only on
  scheduler_scaling_pthreads builds; on other builds the runtime reserves
  it for scheduler sleep/wake and `Sig.usr2()` is a compile error.

  Validation is necessary but not sufficient: the operating system can
  still refuse a registration the whitelist admits (for example, glibc
  reserves the two lowest real-time signals for its own threading
  internals, and musl the three lowest). Such a refusal surfaces through
  the normal failure path — the notify's `registration_failed` is called
  and the handler is automatically disposed.
  Diagnostic runtime builds (`use=runtime_tracing`) additionally reserve a
  pause signal — SIGRTMIN on Linux, SIGINFO on BSD and macOS — and
  handling that signal in such a build breaks runtime tracing.
  """
  fun apply(sig: U32): ValidationResult =>
    """
    Return `ValidationSuccess` for a handleable signal number, or a
    `ValidationFailure` naming the rejected number.
    """
    if _is_handleable(sig) then
      ValidationSuccess
    else
      recover val
        ValidationFailure(sig.string() + " is not a handleable signal")
      end
    end

  // SIGUSR2 is handleable only on scheduler_scaling_pthreads builds. On other
  // builds the runtime reserves it as its scheduler sleep/wake signal and
  // `Sig.usr2()` is a compile error, so it can never reach the whitelist. On
  // scheduler_scaling_pthreads builds the scheduler uses condition variables
  // instead, leaving SIGUSR2 free. runtime_tracing also uses SIGUSR2
  // (PONY_TRACING_SLEEP_WAKE_SIGNAL in src/libponyrt/tracing/tracing.c), but it
  // blocks the signal only inside its own tracing thread, not in every runtime
  // thread the way the scheduler does. A process-directed SIGUSR2 is delivered
  // to a thread that has not blocked it, so a user handler still fires and
  // runtime_tracing does not need excluding here.
  // This gate must match Sig.usr2()'s in sig.pony, and both mirror the
  // runtime's PONY_SCHED_SLEEP_WAKE_SIGNAL (src/libponyrt/sched/scheduler.h).
  // If the runtime's sleep/wake signal changes, or the build flag that frees
  // it does, both gates must change to match or the package hands out a signal
  // the runtime has taken.
  fun _usr2_handleable(sig: U32): Bool =>
    ifdef "scheduler_scaling_pthreads" and not windows then
      sig == Sig.usr2()
    else
      false
    end

  fun _is_handleable(sig: U32): Bool =>
    ifdef bsd or osx then
      (sig == Sig.hup()) or (sig == Sig.int()) or (sig == Sig.quit())
        or (sig == Sig.emt()) or (sig == Sig.pipe()) or (sig == Sig.alrm())
        or (sig == Sig.term()) or (sig == Sig.urg()) or (sig == Sig.tstp())
        or (sig == Sig.cont()) or (sig == Sig.chld()) or (sig == Sig.ttin())
        or (sig == Sig.ttou()) or (sig == Sig.io()) or (sig == Sig.xcpu())
        or (sig == Sig.xfsz()) or (sig == Sig.vtalrm()) or (sig == Sig.prof())
        or (sig == Sig.winch()) or (sig == Sig.info()) or (sig == Sig.usr1())
        or (sig == Sig.sys())
        or _usr2_handleable(sig)
        or _is_rt(sig)
    elseif linux then
      (sig == Sig.hup()) or (sig == Sig.int()) or (sig == Sig.quit())
        or (sig == Sig.pipe()) or (sig == Sig.alrm()) or (sig == Sig.term())
        or (sig == Sig.urg()) or (sig == Sig.stkflt()) or (sig == Sig.tstp())
        or (sig == Sig.cont()) or (sig == Sig.chld()) or (sig == Sig.ttin())
        or (sig == Sig.ttou()) or (sig == Sig.io()) or (sig == Sig.xcpu())
        or (sig == Sig.xfsz()) or (sig == Sig.vtalrm()) or (sig == Sig.prof())
        or (sig == Sig.winch()) or (sig == Sig.pwr()) or (sig == Sig.usr1())
        or (sig == Sig.sys())
        or _usr2_handleable(sig)
        or _is_rt(sig)
    elseif windows then
      // Only SIGINT and SIGTERM can be meaningfully handled through the
      // ASIO mechanism on Windows; the other signals the CRT knows
      // (SIGABRT, SIGFPE, SIGILL, SIGSEGV) are synchronous or fatal.
      (sig == Sig.int()) or (sig == Sig.term())
    else
      false
    end

  fun _is_rt(sig: U32): Bool =>
    ifdef bsd then
      (sig >= 65) and (sig <= 126)
    elseif linux then
      (sig >= 32) and (sig <= 64)
    else
      false
    end

type HandleableSignal is Constrained[U32, HandleableSignalValidator]
  """
  A signal number that has been validated as handleable on this platform.
  Required by `SignalHandler`; obtain one from `MakeHandleableSignal`.
  """

type MakeHandleableSignal is MakeConstrained[U32, HandleableSignalValidator]
  """
  Factory for `HandleableSignal` instances. Returns either a `HandleableSignal`
  or a `ValidationFailure` with an error message.
  """
