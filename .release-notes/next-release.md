## Don't include generated documentation "home file" in search index

We've updated the documentation generation to not include the "home file" that lists the packages in the search index. The "exclude from search engine" functionality is only available in the `mkdocs-material-insiders` theme.

The home file is extra noise in the index that provides no value. For anyone using the insiders theme, this will be an improvement in the search experience.

## Add support for erase left and erase line in term.ANSI

We've implemented [RFC #75](https://github.com/ponylang/rfcs/blob/main/text/0075-ansi-erase.md) that set out a means of updating `ANSI` in the `term` package to support not just ANSI erase right, but also erase left, and erase line.

Existing functionality continues to work as is. All existing erase right functionality will continue to work without breakage. The `erase` method has been augmented to take an option parameter for the direction to erase. `EraseRight` is the default, thus keeping existing behavior. `EraseLeft` and `EraseLine` options have been added as well.

To erase left, one would do:

```pony
ANSI.erase(EraseLeft)
```

To erase right, you could do either of the following:

```pony
ANSI.erase()
ANSI.erase(EraseRight)
```

And finally, to erase an entire line:

```pony
ANSI.erase(EraseLine)
```

## Improve TCP Backpressure on Windows

Our prior setting of backpressure for TCP writes on Windows was naive. It was  based purely on the number of buffers currently outstanding on an IOCP socket. The amount of data didn't matter at all. Whether more data could be accepted or not, also wasn't taken into consideration. We've enhanced the backpressure support at both the Pony level in `TCPConnection` and in the runtime API.

Two runtime API methods have been updated on Windows.

### pony_os_writev

The Windows version of `pony_os_writev` will now return the number of buffers accepted or zero if backpressure is encountered. All other errors still cause an error that must be handled on the Pony side of the API via a `try` block.

### pony_os_send

The Windows version of `pony_os_send` will now return the number of bytes accepted or zero if backpressure is encountered. All other errors still cause an error that must be handled on the Pony side of the API via a `try` block.

The changes are considered non-breaking as previously, the return values from both functions had no meaning.

## Fix multiple races within actor/cycle detector interactions

The Pony runtime includes an optional cycle detector that is on by default. The cycle detector runs and looks for groups of blocked actors that will have reference counts above 0 but are unable to do any more work as all members are blocked and don't have additional work to do.

Over time, we have made a number of changes to the cycle detector to improve it's performance and mitigate its impact on running Pony programs. In the process of improving the cycle detectors performance, it has become more and more complicated. That complication led to several race conditions that existed in the interaction between actors and the cycle detector. Each of these race conditions could lead to an actor getting freed more than once, causing an application crash or an attempt to access an actor after it had been deleted.

We've identified and fixed what we believe are all the existing race conditions in the current design.

## Update supported FreeBSD to 13.1

Prebuilt FreeBSD ponyc binaries are now built for FreeBSD 13.1. All CI is also done using FreeBSD 13.1. We will make a "best effort" attempt to not break FreeBSD 13.0, but the only supported version of FreeBSD going forward is 13.1.

Previously built ponyc versions targeting FreeBSD 13.0 will continue to be available.

## Drop Rocky 8 support

In July of 2021, we added prebuilt binaries for Rocky 8. We did this at the request of a ponyc user. However, after attempting an update to get Rocky 8 building again, we hit a problem where in the new CI environment we built, we are no longer able to build ponyc. We do not have time to investigate beyond the few hours we have already put in. If anyone in the community would like to get the Rocky 8 environment working again, we would be happy to add support back in.

The current issue involves libatomic not being found during linking. If you would like to assist, hop into the [CI stream](https://ponylang.zulipchat.com/#narrow/stream/190359-ci) on the ponylang Zulip.

## Fix compiler crash when calling (this.)create()

Fixed a crash when calling `.create()` (with an implicit `this`).

