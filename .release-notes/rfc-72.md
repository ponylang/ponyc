## Change standard library object capabilities pattern

We've changed standard library object capabilities pattern.

Object capabilities play an important role in Pony's security story. Developers can via a combination of disallowing the use of FFI within a given package and object capabilities be assured that 3rd-party libraries they use aren't doing unexpected things like opening network sockets.

Joe Eli McIlvain and Sean T. Allen spent some time reviewing the previous usage of object capabilities and came away concerned. We believed the old standard library pattern was:

- Error prone for beginning Pony programmers
- Encouraged passing around AmbientAuth everywhere in a way that negated the advantages of using object capabilities and only left the disadvantages

To learn more about the reasoning for the change, see the [RFC](https://github.com/ponylang/rfcs/blob/main/text/0072-change-stdlib-object-capabilities-pattern.md) that this change implements.

Note, this is a breaking changes and might require you to change your code that involves object capability authorization usage. Only a single "most specific" authorization is now allowed at call sites so previously where you could have been using `AmbientAuth` directly to authorization an action like creating a `TCPListener`:

```pony
TCPListener(env.root, Listener(env.out))
```

You'll now have to use the most specific authorization for the object:

```pony
TCPListener(TCPListenAuth(env.root), Listener(env.out))
```

### Updating authorization for `Backpressure`

Previously accepted:

- `AmbientAuth`
- `ApplyReleaseBackpressureAuth`
- the `BackpressureAuth` type union.

Now accepted:

- `ApplyReleaseBackpressureAuth`

### Updating authorization for `DNS`

Previously accepted:

- `AmbientAuth`
- `NetAuth`
- `DNSAuth`
- the `DNSLookupAuth` type union

Now accepted:

- `DNSAuth`

### Updating authorization for `FilePath`

Previously accepted:

- `AmbientAuth`
- `FileAuth`
- an existing `FilePath` instance

Now accepted:

- `FileAuth`
- an existing `FilePath` instance

### Updating authorization for `ProcessMonitor`

Previously accepted:

- `AmbientAuth`
- `StartProcessAuth`
- the `ProcessMonitorAuth` type union
Now accepted:

- `StartProcessAuth`

### Updating authorization for `RuntimeInfo`

Previously accepted:

- `AmbientAuth`
- `SchedulerInfoAuth`
- the `RuntimeInfoAuth` type union

Now accepted:

- `SchedulerInfoAuth`

### Updating authorization for `Serialised`

No changes are needed.

### Updating authorization for `TCPConnection`

Previously accepted:

- `AmbientAuth`
- `NetAuth`
- `TCPAuth`
- `TCPConnectAuth`
- the `TCPConnectionAuth` type union

Now accepted:

- `TCPConnectAuth`

### Updating authorization for `TCPListener`

Previously accepted:

- `AmbientAuth`
- `NetAuth`
- `TCPAuth`
- `TCPListenAuth`
- the `TCPListenerAuth` type union

Now accepted:

- `TCPListenAuth`

### Updating authorization for `UDPSocket`

Previously accepted:

- `AmbientAuth`
- `NetAuth`
- `UDPAuth`
- the `UDPSocketAuth` type union

Now accepted:

- `UDPAuth`
