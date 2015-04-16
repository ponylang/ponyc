actor Main
  new create(env: Env) =>
    _throw_catch_local(env)
    _throw_catch_caller(env)

    try 
      _throw_catch_callers(env) 
    else
      env.out.print("catch callers")
    end

    _throw_rethrow_catch(env)
    _throw_rethrow_catch_caller(env)

  fun _throw_catch_local(env: Env) =>
    try
      error
    else
      env.out.print("catch local")
    end

  fun _throw_catch_caller(env: Env) =>
    try
      _throw()
    else
      env.out.print("catch caller")
    end

  fun _throw_catch_callers(env: Env) ? =>
    _throw()

  fun _throw_rethrow_catch(env: Env) =>
    try
      try
        error
      else
        error
      end
    else
      env.out.print("rethrow catch")
    end

  fun _throw_rethrow_catch_caller(env: Env) =>
    try
      _throw()
    else
      env.out.print("throw rethrow catch caller")
    end

  fun _throw() ? => 
    error

  fun rethrow() ? =>
    try 
      _throw()
    end
    error
