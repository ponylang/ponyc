## Stop creating Centos 8 releases

CentOS 8 has been end-of-lifed and will be receiving no further updates. As per our policy, we will no longer be building regular releases for CentOS 8. Existing releases can still be installed via ponyup, but no additional releases will be done.

Our CentOS 8 support has been replaced by support for Rocky 8.

## Hide C header implementation details related to actor pad size.

Previously, in the `pony.h` header that exposes the "public API" of the Pony runtime, information about the size of an actor struct was published as public information.

This change removes that from the public header into a private header, to hide
implementation details that may be subject to change in future versions of the Pony runtime.

This change does not impact Pony programs - only C programs using the `pony.h` header to use the Pony runtime in some other way than inside of a Pony program, or to create custom actors written in C.

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

