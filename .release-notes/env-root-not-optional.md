## Change type of Env.root to AmbientAuth

Previously, Env.root currently had the type (AmbientAuth | None) to allow for creation of artificial Env instances without AmbientAuth. Yet, there was no observable usage of this feature. It required extra work to make use of, as one always needs a pattern match or an as expression with a surrounding try. We've changed Env.root to only be AmbientAuth, no longer requiring a pattern match or a try.

To adjust your code to take this breaking change into account, you'd do the following. Where you previously had code like:

```pony
    try
      TCPListener(env.root as AmbientAuth, Listener(env.out))
    else
      env.out.print("unable to use the network")
    end
```

You can now do:

```pony
    TCPListener(env.root, Listener(env.out))
```

The same change can be made if you were pattern matching on the type of env.root.
