primitive Sig
  """
  Define the portable signal numbers. Other signals can be used, but they are
  not guaranteed to be portable.
  """
  fun hup(): U32 => 1
  fun int(): U32 => 2
  fun quit(): U32 => 3

  fun ill(): I32 =>
    ifdef linux or freebsd then 4
    else compile_error "no SIGINT" end

  fun trap(): I32 =>
    ifdef linux or freebsd then 5
    else compile_error "no SIGTRAP" end

  fun abrt(): U32 => 6

  fun emt(): I32 =>
    ifdef freebsd then 7
    else compile_error "no SIGEMT" end

  fun fpe(): I32 =>
    ifdef linux or freebsd then 8
    else compile_error "no SIGFPE" end

  fun kill(): U32 => 9

  fun bus(): I32 =>
    ifdef freebsd then 10
    elseif linux then 7
    else compile_error "no SIGBUS" end

  fun segv(): I32 =>
    ifdef freebsd then 11
    else compile_error "no SIGSEGV" end

  fun sys(): I32 =>
    ifdef freebsd then 12
    elseif linux then 31
    else compile_error "no SIGSYS" end

  fun pipe(): I32 =>
    ifdef linux or freebsd then 13
    else compile_error "no SIGPIPE" end

  fun alrm(): U32 => 14
  fun term(): U32 => 15

  fun urg(): I32 =>
    ifdef freebsd then 16
    elseif linux then 23
    else compile_error "no SIGURG" end

  fun stkflt(): I32 =>
    ifdef linux then 16
    else compile_error "no SIGSTKFLT" end

  fun stop(): I32 =>
    ifdef freebsd then 17
    elseif linux then 19
    else compile_error "no SIGSTOP" end

  fun tstp(): I32 =>
    ifdef freebsd then 18
    elseif linux then 20
    else compile_error "no SIGTSTP" end

  fun cont(): I32 =>
    ifdef freebsd then 19
    elseif linux then 18
    else compile_error "no SIGCONT" end

  fun chld(): I32 =>
    ifdef freebsd then 20
    elseif linux then 17
    else compile_error "no SIGCHLD" end

  fun ttin(): I32 =>
    ifdef linux or freebsd then 21
    else compile_error "no SIGTTIN" end

  fun ttou(): I32 =>
    ifdef linux or freebsd then 22
    else compile_error "no SIGTTOU" end

  fun io(): I32 =>
    ifdef freebsd then 23
    elseif linux then 29
    else compile_error "no SIGIO" end

  fun xcpu(): I32 =>
    ifdef linux or freebsd then 24
    else compile_error "no SIGXCPU" end

  fun xfsz(): I32 =>
    ifdef linux or freebsd then 25
    else compile_error "no SIGXFSZ" end

  fun vtalrm(): I32 =>
    ifdef linux or freebsd then 26
    else compile_error "no SIGVTALRM" end

  fun prof(): I32 =>
    ifdef linux or freebsd then 27
    else compile_error "no SIGPROF" end

  fun winch(): I32 =>
    ifdef linux or freebsd then 28
    else compile_error "no SIGWINCH" end

  fun info(): I32 =>
    ifdef freebsd then 29
    else compile_error "no SIGINFO" end

  fun pwr(): I32 =>
    ifdef linux then 30
    else compile_error "no SIGPWR" end

  fun usr1(): I32 =>
    ifdef freebsd then 30
    elseif linux then 10
    else compile_error "no SIGUSR1" end

  fun usr2(): I32 =>
    ifdef freebsd then 31
    elseif linux then 12
    else compile_error "no SIGUSR2" end

  fun rt(n: U32): I32 ? =>
    ifdef freebsd then
      if n < 61 then
        65 + n.i32()
      else
        error
      end
    elseif linux then
      if n < 32 then
        32 + n.i32()
      else
        error
      end
    else
      compile_error "no SIGRT"
    end
