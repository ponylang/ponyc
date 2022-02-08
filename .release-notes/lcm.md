## Add greatest common divisor to `math` package

Code to determine the greatest common divisor for two integers has been added to the standard library:

```pony
use "math"

actor Main
  new create(env: Env) =>
    try
      let gcd = GreatestCommonDivisor(10, 20)?
      env.out.print(gcd.string())
    else
      env.out.print("No GCD")
    end
```

## Add least common multiple to `math` package

Code to determine the least common multiple of two integers has been added to the standard library:

```pony
use "math"

actor Main
  new create(env: Env) =>
    try
      let lcm = LeastCommonMultiple(10, 20)?
      env.out.print(lcm.string())
    else
      env.out.print("No LCM")
    end
```
