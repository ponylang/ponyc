primitive Sig
  """
  Define the portable signal numbers. Other signals can be used, but they are
  not guaranteed to be portable.
  """
  fun hup(): U32 => 1
  fun int(): U32 => 2
  fun quit(): U32 => 3

  fun ill(): U32 =>
    ifdef linux or bsd or osx then 4
    else compile_error "no SIGINT"
    end

  fun trap(): U32 =>
    ifdef linux or bsd or osx then 5
    else compile_error "no SIGTRAP"
    end

  fun abrt(): U32 => 6

  fun emt(): U32 =>
    ifdef bsd or osx then 7
    else compile_error "no SIGEMT"
    end

  fun fpe(): U32 =>
    ifdef linux or bsd or osx then 8
    else compile_error "no SIGFPE"
    end

  fun kill(): U32 => 9

  fun bus(): U32 =>
    ifdef bsd or osx then 10
    elseif linux then 7
    else compile_error "no SIGBUS"
    end

  fun segv(): U32 =>
    ifdef linux or bsd or osx then 11
    else compile_error "no SIGSEGV"
    end

  fun sys(): U32 =>
    ifdef bsd or osx then 12
    elseif linux then 31
    else compile_error "no SIGSYS"
    end

  fun pipe(): U32 =>
    ifdef linux or bsd or osx then 13
    else compile_error "no SIGPIPE"
    end

  fun alrm(): U32 => 14
  fun term(): U32 => 15

  fun urg(): U32 =>
    ifdef bsd or osx then 16
    elseif linux then 23
    else compile_error "no SIGURG"
    end

  fun stkflt(): U32 =>
    ifdef linux then 16
    else compile_error "no SIGSTKFLT"
    end

  fun stop(): U32 =>
    ifdef bsd or osx then 17
    elseif linux then 19
    else compile_error "no SIGSTOP"
    end

  fun tstp(): U32 =>
    ifdef bsd or osx then 18
    elseif linux then 20
    else compile_error "no SIGTSTP"
    end

  fun cont(): U32 =>
    ifdef bsd or osx then 19
    elseif linux then 18
    else compile_error "no SIGCONT"
    end

  fun chld(): U32 =>
    ifdef bsd or osx then 20
    elseif linux then 17
    else compile_error "no SIGCHLD"
    end

  fun ttin(): U32 =>
    ifdef linux or bsd or osx then 21
    else compile_error "no SIGTTIN"
    end

  fun ttou(): U32 =>
    ifdef linux or bsd or osx then 22
    else compile_error "no SIGTTOU"
    end

  fun io(): U32 =>
    ifdef bsd or osx then 23
    elseif linux then 29
    else compile_error "no SIGIO"
    end

  fun xcpu(): U32 =>
    ifdef linux or bsd or osx then 24
    else compile_error "no SIGXCPU"
    end

  fun xfsz(): U32 =>
    ifdef linux or bsd or osx then 25
    else compile_error "no SIGXFSZ"
    end

  fun vtalrm(): U32 =>
    ifdef linux or bsd or osx then 26
    else compile_error "no SIGVTALRM"
    end

  fun prof(): U32 =>
    ifdef linux or bsd or osx then 27
    else compile_error "no SIGPROF"
    end

  fun winch(): U32 =>
    ifdef linux or bsd or osx then 28
    else compile_error "no SIGWINCH"
    end

  fun info(): U32 =>
    ifdef bsd or osx then 29
    else compile_error "no SIGINFO"
    end

  fun pwr(): U32 =>
    ifdef linux then 30
    else compile_error "no SIGPWR"
    end

  fun usr1(): U32 =>
    ifdef bsd or osx then 30
    elseif linux then 10
    else compile_error "no SIGUSR1"
    end

  fun usr2(): U32 =>
    ifdef not "scheduler_scaling_pthreads" then
      ifdef bsd or osx then 31
      elseif linux then 12
      else compile_error "no SIGUSR2"
      end
    else
      ifdef linux or bsd or osx then
        compile_error "SIGUSR2 reserved for runtime use"
      else
        compile_error "no SIGUSR2"
      end
    end

  fun rt(n: U32): U32 ? =>
    ifdef bsd then
      if n <= 61 then
        65 + n.u32()
      else
        error
      end
    elseif linux then
      if n <= 32 then
        32 + n.u32()
      else
        error
      end
    else
      compile_error "no SIGRT"
    end
