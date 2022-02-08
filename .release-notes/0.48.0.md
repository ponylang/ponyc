## Add a pony primitive that exposes scheduler information

Previously, when users wanted to optimize the parallelizing work based on the maximum number of actors that can be run at a single time, we were pointing them to using the private runtime method `@ponyint_sched_cores()`.

This was problematic for two reasons:

1. We were pointing users at an internal implementation method that we hadn't promised not to change.
2. We were requiring users to muck about with FFI for something that has become somewhat common for users wanting to maximize performance want to know.

We've now exposed some information about scheduler threads via a new package `runtime_info` thereby resolving both of the problems mentioned above.

Pony users should expect to see additional information added to the `runtime_info` package moving forward.

Anyone who was using the internal `ponyint_sched_cores()` FFI call will need to update to using `runtime_info` as the internal function has been removed.

## Stop creating prebuilt ponyc releases for Ubuntu 21.04

Ubuntu 21.04 hit its end-of-life at the end of January 2022. We are no longer build prebuilt ponyc releases for it. If you are still using Ubuntu 21.04 and need ponyc for 21.04, you'll have to build from source.

The next Ubuntu LTS release is coming in April of 2022 and we'll be adding prebuilt ponyc for it. If have upgraded to a non-LTS Ubuntu after 21.04 that s still supported and would like us to do ponyc releases for it, please let us know via a [GitHub issue](https://github.com/ponylang/ponyc/issues) or via [the Ponylang Zulip](https://ponylang.zulipchat.com/).

## Fix runtime crash when tracing class iso containing struct val

Prior to this fix, you could crash the runtime if you sent a `val` struct wrapper in an `iso` class. For example:

```pony
use "collections"

struct val Foo

class Bar
  let f: Foo = Foo

actor Main
  new create(env: Env) =>
    for i in Range(0, 20000) do
      inspect(recover Bar end)
    end

  be inspect(wrap: Bar iso) =>
    None // Do something with wrap.f
```

## Revert "prevent non-opaque structs from being used as behaviour parameters"

The bug that [#3781](https://github.com/ponylang/ponyc/pull/3781) aka "prevent non-opaque structs from being used as behaviour parameters" was an attempt at preventing, as only partially solved by the change.

We've introduced a full fix in [#3993](https://github.com/ponylang/ponyc/pull/3993) that removes the need for [#3871](https://github.com/ponylang/ponyc/pull/3781).

## Update to LLVM 13.0.1

We've updated the LLVM used to build Pony to 13.0.1.

## Remove out parameter from `pony_os_stdin_read`

The signature of the `pony_os_stdin_read` function has been simplified to remove the `bool* out_again` parameter.

```diff
-PONY_API size_t pony_os_stdin_read(char* buffer, size_t space, bool* out_again)
+PONY_API size_t pony_os_stdin_read(char* buffer, size_t space)
```

It is permitted to call the `pony_os_stdin_read` function again in a loop if the return value is greater than zero, and the platform is not windows. Given that those two conditions are enough information to make a decision, the `out_again` parameter is not needed, and can be removed.

Technically this is a breaking change, because the function is prefixed with `pony_` and is thus a public API. But it is unlikely that any code out there is directly using the `pony_os_stdin_read` function, apart from the `Stdin` actor in the standard library, which has been updated in its internal implementation details to match the new signature.

## Expose additional scheduler information in the runtime_info package

Three additional methods have been added to `SchedulerInfo`.

- minimum_schedulers

Returns the minimum number of schedulers that will be active.

- scaling_is_active

Returns if scheduler scaling is on

- will_yield_cpu

Returns if scheduler threads are yielding the CPU to other processes when they have no work to do.

## Add additional methods to `itertools`

Previously there were methods for `Iter` within `itertools` which, while originally planned, where not implemented. These methods have now been implemented.

The added methods are:

+ `dedup` which removes local duplicates from consecutive identical elements via a provided hash function
+ `interleave` which alternates values between two iterators until both run out
+ `interleave_shortest` which alternates values between two iterators until one of them runs out
+ `intersperse` which yields a given value after every `n` elements of the iterator
+ `step_by` which yields every `n`th element of the iterator
+ `unique` which removes global duplicates of identical elements via a provided hash function

