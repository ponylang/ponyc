## Introduction of empty Ranges

Previously, it was easy to accidentally create infinite `Range`s due to the implementation giving precedence to parameters `min` and `inc` and the iterative relationship that the next value be `idx + inc` where `idx` starts equal to `min`. Iteration stopped when `idx` was equal to or passed `max`. This resulted in infinite ranges when either 1) the `inc` was 0, or any of `min`, `max`, or `inc` were NaN, +Inf or -Inf, or 2) if no progress could be made from `min` to `max` due to the sign of `inc`. Therefore, even a seemingly correct range such as `Range[ISize](-10, 10, -1)` would be infinite -- producing values `-10, -11, -12, ...` rather than moving between values `-10` and `10`, as might be expected. In order to avoid this possible error of seemingly correct ranges, we decided to introduce empty ranges which produce no values and reclassify much of what was previously infinite to instead be empty. There are now three mutually exclusive classifications for ranges: finite, infinite, and empty ranges. Finite ranges will produce some number of values before terminating, infinite ranges will never terminate, and empty ranges will immediately terminate due to having no values.

### User Code Changes

We conducted a search of public Pony code and it appears that the vast majority of `Range` usage should not require any changes. The most likely required change is any infinite `Range` producing an infinite iterator of a single value; for this we recommend switching to use of `Iter[A].repeat_value(value)` from the itertools package.

The only remaining infinite ranges are a positive and negative towards their respective infinities, so any code that previously relied on an infinite iterator will need to change. For example, any code that previously flipped the sign of the iterator or max parameter as a way to iterate in a given direction infinitely will need to change.

Previously, the code below would iterate infinitely.

```pony
// This code will no longer work
let nan = F64(0) / F64(0)
for i in Range[F64](nan) do
  // Previously infinite in the positive direction
end
```

Will now need to be specified as follows.

```pony
let inf = F64.max_value() + F64.max_value()
for i in Range[F64](0, inf) do
  // Infinite in the positive direction
end
```

We can flip this relationship for a negative iterator.

```pony
let inf = F64.max_value() + F64.max_value()
for i in Range[F64](0, -inf, -1) do
  // Infinite in the negative direction
end
```

## Fix infinite loop in compiler

The following program used to cause an infinite loop in the compiler:

```pony
type Message is (String, String, None)

interface tag Manager
  be handle_message(msg: Message val)

actor Main
  new create(env: Env) =>
    Foo(env, recover tag this end)

  be handle_message(msg: Message val) =>
    None

actor Foo
  var manager: Manager

  new create(env: Env, manager': Manager) =>
    manager = manager'
    let notifier = InputNotifier(this)
    env.input(consume notifier)

  be handle_data(data: String) =>
    manager.handle_message(("","",None))

class InputNotifier is InputNotify
  let parent: Foo

  new iso create(parent': Foo) =>
    parent = parent'

  fun ref apply(data': Array[U8 val] iso) =>
    parent.handle_data("")
```

## Fix compiler segfault caused by infinite recursion

The following program used to cause ponyc to segfault due to infinite recursion:

```pony
type Message is ((I64 | String), String, None)

interface tag Manager
  be handle_message(msg: Message val)

actor Main
  new create(env: Env) =>
    Foo(env, recover tag this end)

  be handle_message(msg: Message val) =>
    None

actor Foo
  var manager: Manager

  new create(env: Env, manager': Manager) =>
    manager = manager'
    let notifier = InputNotifier(this)
    env.input(consume notifier)

  be handle_data(data: String) =>
    manager.handle_message(("","",None))

class InputNotifier is InputNotify
  let parent: Foo

  new iso create(parent': Foo) =>
    parent = parent'

  fun ref apply(data': Array[U8 val] iso) =>
    parent.handle_data("")
```

