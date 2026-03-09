use "constrained_types"

primitive SignalValidator is Validator[U32]
  """
  Validates that a signal number is handleable via the ASIO mechanism.

  Only signals that can be safely caught and dispatched to Pony actors
  are accepted. Fatal signals (SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS,
  SIGSEGV), uncatchable signals (SIGKILL, SIGSTOP), and unknown signal
  numbers are rejected.
  """
  fun apply(sig: U32): ValidationResult =>
    if _is_handleable(sig) then
      ValidationSuccess
    else
      recover val ValidationFailure(sig.string() + " is not a handleable signal") end
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
    // SIGUSR2 is only available when the runtime doesn't use it for
    // scheduler scaling. Sig.usr2() has a compile_error when
    // scheduler_scaling_pthreads is set, so guard the call here.
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

// A signal number that has been validated as handleable.
type ValidSignal is Constrained[U32, SignalValidator]

// Factory for creating ValidSignal instances. Returns either a ValidSignal
// or a ValidationFailure with an error message.
type MakeValidSignal is MakeConstrained[U32, SignalValidator]
