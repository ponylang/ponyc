## Remove simplebuiltin compiler option

The ponyc option `simplebuiltin` was never useful to users but had been exposed for testing purposes for Pony developers. We've removed the need for the "simplebuiltin" package for testing and have remove both it and the compiler option.

Technically, this is a breaking change, but no end-users should be impacted.

## Fix return checking in behaviours and constructors

Our checks to make sure that the usage of return in behaviours and constructors was overly restrictive. It was checking for any usage of `return` that had a value including when the return wasn't from the method itself.

For example:

```pony
actor Main
  new create(env: Env) =>
    let f = {() => if true then return None end}
```

Would return an error despite the return being in a lambda and therefore not returning a value from the constructor.

## Fix issue that could lead to a muted actor being run

A small logical flaw was discovered in the Pony runtime backpressure system that could lead to an actor that has been muted to prevent it from overloading other actors to be run despite a rule that says muted actors shouldn't be run.

The series of events that need to happen are exceedingly unlikely but we do seem them from time to time in our Arm64 runtime stress tests. In the event that a muted actor was run, if an even more unlikely confluence of events was to occur, then "very bad things" could happen in the Pony runtime where "very bad things" means "we the Pony developers are unable to reason about what might happen".

## Remove library mode option from ponyc

From the very early days of the Pony runtime, a "library mode" has been around that allows you to compile a Pony program to a C compatible library that you can then start up using various runtime APIs to do things like initialize the runtime, create actors, send messages and more. We've made extensive changes to the runtime over the years and have no faith that library mode and its related functionality work.

Our lack of faith extends back many years and we've stated as the Pony core team that we don't consider library mode to be supported; until now, we haven't done anything about what "not supported" means.

With this release, library mode has been removed as an option.

## Don't allow interfaces to have private methods

When Pony allowed interfaces to have private methods, it was possible to use an interface to break encapsulation for objects and access private methods from outside their enclosing package.

The following code used to be legal Pony code but will now fail with a compiler error:

```pony
actor Main
  new create(env: Env) =>
    let x: String ref = "sailor".string()
    let y: Foo = x
    y._set(0, 'f')
    env.out.print("Hello, " + x)

interface Foo
  fun ref _set(i: USize, value: U8): U8
```

If you were previously using interfaces with private methods in your code, you'll need to switch them to being traits and then use nominal typing on all the implementers.

For example:

```pony
interface Foo
  fun ref _bar()
```

would become:

```pony
trait Foo
  fun ref _bar()
```

And any objects that need to provide `Foo` would be changed from:

```pony
class ProvidesFoo
```

to

```pony
class ProvidesFoo is Foo
```

We believe that the only impact of this change will primarily be a single interface in the standard library `AsioEventNotify` and updates that need to be done to deal with it changing from an interface to a trait.

If you are unable to make the changes above, it means that you were taking the encapsulation violation bug we've fixed and will need to rethink your design. If you need assistance with such a redesign, please stop by the [Pony Zulip](https://ponylang.zulipchat.com/) and we'll do what we can to help.

## Change `builtin/AsioEventNotify` from an interface to a trait

The "Don't allow interfaces to have private methods" change that is part of this release required that we change the `AsioEventNotify` interface to a trait.
If you are one of the few users outside of the standard library to be using the ASIO subsystem directly in your Pony code, you'll need to make an update to
conform to this change.

Where previously you had something like:

```pony
class AsioEventReceivingClass
  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    ...
```

You'll need to switch to using nominal typing rather than structural:

```pony
class AsioEventReceivingClass is AsioEventNotify
  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    ...
```

As far as we know, there are two codebases that will be impacted by this change:

- [Lori](https://github.com/seantallen-org/lori)
- [Wallaroo](https://github.com/wallaroolabs/wally)

## Fix compiler assertion failure when assigning error to a variable

This release fixes a compiler assertion failure being triggered when one attempted to assign an error expression surrounded by parenthesis to a variable.

##  Add "nodoc" annotation

The can be used to control the pony compilers documentation generation, any structure that can have a docstring (except packages) can be annotated with \nodoc\ to prevent documentation from being generated.

This replaces a hack that has existed in the documentation generation system for several years to filter out items that were "for testing" by looking for Test and _Test at the beginning of the name as well as providing UnitTest or TestList.

Note that the "should we include this package" hack to filter oupackages called "test" and "builtin_test" still exists for the timbeing until we have further discussion.

