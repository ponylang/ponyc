## Reject dot segments in `URIBuilder.append_path_segment`

`URIBuilder.append_path_segment` now rejects `.` and `..` segments. `build()` returns `InvalidDotSegment` when either was appended.

No percent-encoding preserves `.` or `..` as literal path segments — RFC 3986 §6.2.2.2 normalizes `%2E` back to `.` before dot-segment removal runs. An appended `..` caused `NormalizeURI` and `ResolveURI` to remove the preceding segment, which is path traversal when segments come from untrusted input.

`set_path` is unaffected — it accepts the full path as-is, and dot segments in a complete path are the caller's responsibility.

`URIBuildError` gains the `InvalidDotSegment` variant. Exhaustive matches on `URIBuildError` must add a case for it:

Before:

```pony
match \exhaustive\ builder.build()
| let u: URI val => use(u)
| let e: URIBuildError val => handle(e)
end
```

After (if matching individual error types):

```pony
match \exhaustive\ builder.build()
| let u: URI val => use(u)
| let _: InvalidScheme val => // ...
| let _: InvalidDotSegment val => // ...
| let e: URIParseError val => // ...
end
```

Callers who match `URIBuildError` as a single arm are unaffected.
