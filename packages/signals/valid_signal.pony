use "constrained_types"

primitive SignalValidator is Validator[U32]
  """
  Validates that a signal number is handleable via the ASIO mechanism.

  Only signals that can be safely caught and dispatched to Pony actors
  are accepted. Fatal signals (SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS,
  SIGSEGV), uncatchable signals (SIGKILL, SIGSTOP), and unknown signal
  numbers are rejected.

  Validation is necessary but not sufficient: the operating system can
  still refuse a registration the whitelist admits (for example, glibc
  reserves the two lowest real-time signals for its own threading
  internals). Such a refusal surfaces through the normal failure path —
  the handler is automatically disposed.
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
        or _is_usr2(sig)
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
        or _is_usr2(sig)
        or _is_rt(sig)
    elseif windows then
      // Windows signal() only supports SIGINT and SIGTERM as catchable
      // signals. SIGABRT, SIGFPE, SIGILL, SIGSEGV are fatal.
      (sig == Sig.int()) or (sig == Sig.term())
    else
      false
    end

  fun _is_usr2(sig: U32): Bool =>
    // Sig.usr2() has a compile_error when scheduler_scaling_pthreads is
    // set, so this guard has to mirror that gate to be able to call it
    // at all.
    ifdef not "scheduler_scaling_pthreads" then
      ifdef bsd or osx then
        sig == Sig.usr2()
      elseif linux then
        sig == Sig.usr2()
      else
        false
      end
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

type ValidSignal is Constrained[U32, SignalValidator]
  """
  A signal number that has been validated as handleable on this platform.
  Required by `SignalHandler`; obtain one from `MakeValidSignal`.
  """

type MakeValidSignal is MakeConstrained[U32, SignalValidator]
  """
  Factory for `ValidSignal` instances. Returns either a `ValidSignal` or a
  `ValidationFailure` with an error message.
  """
