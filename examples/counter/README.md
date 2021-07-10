# counter

An asynchronous, shared counter. This example shows something subtle: a way to safely "share mutable data" by dedicating a data owner, then communicating with that owner.

## How to Compile

With a minimal Pony installation, in the same directory as this README file run `ponyc`. You should see content building the necessary packages, which ends with:

```console
...
Generating
 Reachability
 Selector painting
 Data prototypes
 Data types
 Function prototypes
 Functions
 Descriptors
Optimising
Writing ./counter.o
Linking ./counter
```

## How to Run

Once `counter` has been compiled, in the same directory as this README file run `./counter`. You should see your count:

```console
$ ./counter 100
100
```

If you want to prove to yourself that the counter is running in realtime, you can add the following behaviors to see more output.

```pony
actor Counter
...
  be is_running(main: Main) =>
    main.still_running()
...

actor Main
...
    for i in Range[U32](0, count) do
      counter.increment()
      counter.is_running(this)
    end
...
  be still_running() =>
    _env.out.print("Your message here")
```

Doing this will print "Your message here" after each `increment()`.

As an exercise, modify the program to have `Main` `display(...)` the current count after each `counter.increment()` -- hint: you need to add a parameter to the `increment()` behavior.
