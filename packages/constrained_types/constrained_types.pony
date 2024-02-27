"""
# Constrained Types package

The constrained types package provides a standard means for encoding domain constraints using the Pony type system.

For example, in your applications domain, you have usernames that must be between 6 and 12 characters and can only contain lower case ASCII letters. The constrained types package allows you to express those constraints in the type system and assure that your usernames conform.

The key user supplied component of constrained types is a `Validator`. A validator takes a base type and confirms that meets certain criteria. You then have a type of `Constrained[BaseType, ValidatedBy]` to enforce the constraint via the Pony type system.

## Example

For the sake of simplicity, let's represent a username as a constrained `String`. Here's a full Pony program that takes a potential username as a command line argument and validates whether is is acceptable as a username and if it is, prints the username. Note, that the "username printing" function will only take a valid username and there's no way to create a Username that doesn't involve going through the validation step:

```pony
use "constrained_types"

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    try
      let arg1 = env.args(1)?
      let username = MakeUsername(arg1)
      match username
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

type Username is Constrained[String, UsernameValidator]
type MakeUsername is MakeConstrained[String, UsernameValidator]

primitive UsernameValidator is Validator[String]
  fun apply(string: String): ValidationResult =>
    // We do all our work in a recover block so we
    // can mutate any failure object before returning
    // it as a `val`.
    recover val
      let errors: Array[String] = Array[String]()

      // Isn't too big or too small
      if (string.size() < 6) or (string.size() > 12) then
        let msg = "Username must be between 6 and 12 characters"
        errors.push(msg)
      end

      // Every character is valid
      for c in string.values() do
        if (c < 97) or (c > 122) then
          errors.push("Username can only contain lower case ASCII characters")
          break
        end
      end

      // If no errors, return success
      if errors.size() == 0 then
        ValidationSuccess
      else
        // We have some errors, let's package them all up
        // and return the failure
        let failure = ValidationFailure
        for e in errors.values() do
          failure(e)
        end
        failure
      end
    end
```

## Discussion

Let's dig into that code some:

`type Username is Constrained[String, UsernameValidator]` defines a type `Username` that is `Constrained` type. `Constrained` is a generic type that takes two type arguments: the base type being constrained and the validator used to validate that base type conforms to our constraints. So, `Username` is an alias for a `String` that has been constrained using the `UsernameValidator`.

`type MakeUsername is MakeConstrained[String, UsernameValidator]` is another type alias. It's a nice name for "constraint constructor" type `MakeConstrained`. Like `Constrained`, `MakeConstrained` takes the base type being constrained and the validator used.

`primitive UsernameValidator is Validator[String]` is our validator for the `Username` type. It's single function `apply` takes a `String` and examines it returning either `ValidationSuccess` or `ValidationFailure`.

On our usage side we have:

```pony
    let username = MakeUsername(arg1)
    match username
    | let u: Username =>
      print_username(u)
    | let e: ValidationFailure =>
      print_errors(e)
    end
```

Where we use the `MakeUsername` alias that we use to attempt to create a
`Username` and get back either a `Username` or `ValidationFailure` that should
have one ore more error messages for us to display.

Finally, we have a function that can only be used with a valid `Username`:

```pony
fun print_username(username: Username) =>
  _env.out.print(username() + " is a valid username!")
```

In a "real program", we would be doing more complicated things with our `Username` safe in the knowledge that it will always be between 6 and 12 characters and only contain lower case ASCII values.

## Notes

### `val` Entities only

It is important to note that only `val` entities can be used with the
constrained types package. If an entity was mutable, then it could be changed
in a way that violates the constraints after it was validated. And that
wouldn't be very useful.

### Valdatior Requirements

Because validators must:

- Be `val`
- Have a zero argument constructor that returns a `val` Validator

We recommend using `primitive` for your validators although, using a class is supported. However, given that validators are required to be stateless, there is no advantage to using a `class` instead of a `primitive`.

The `interface` for validators is:

```pony
interface val Validator[T]
  new val create()
  fun apply(i: T): ValidationResult
```

### Validators aren't Composable

Also note, that unfortunately, there is no way with the Pony type system to be able to compose validators. You can't for example have a function that takes a "must be lower case" `String` and use a `Username` in place of it. We know that the `Username` type has been validated to be "only lower case strings", but there's no way to represent that in the type system.
"""
