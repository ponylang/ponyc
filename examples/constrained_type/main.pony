use "constrained_types"

type Username is Constrained[String, UsernameValidator]
type MakeUsername is MakeConstrained[String, UsernameValidator]

primitive UsernameValidator is Validator[String]
  fun apply(string: String): ValidationResult =>
    recover val
      let errors: Array[String] = Array[String]()

      if (string.size() < 6) or (string.size() > 12) then
        let msg = "Username must be between 6 and 12 characters"
        errors.push(msg)
      end

      for c in string.values() do
        if (c < 97) or (c > 122) then
          errors.push("Username can only contain lower case ASCII characters")
          break
        end
      end

      if errors.size() == 0 then
        ValidationSuccess
      else
        let failure = ValidationFailure
        for e in errors.values() do
          failure(e)
        end
        failure
      end
    end

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    try
      let arg1 = env.args(1)?
      let username = MakeUsername(arg1)
      match \exhaustive\ username
      | let u: Username =>
        print_username(u)
      | let e: ValidationFailure =>
        print_errors(e)
      end
    end

  fun print_username(username: Username) =>
    _env.out.print(username() + " is a valid username!")

  fun print_errors(errors: ValidationFailure) =>
    _env.err.print("Unable to create username")
    for s in errors.errors().values() do
      _env.err.print("\t- " + s)
    end
