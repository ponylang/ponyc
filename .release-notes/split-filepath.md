## Split `FilePath` construction into two methods

`FilePath` previously had only one constructor, the default `create`, which used a union of `(AmbientAuth | FilePath)`. The first half of this union `AmbientAuth` could never `error`, while the second `FilePath` could `error`. By splitting construction into two methods, we now have a default `create` constructor which cannot `error` and a new `from` constructor which can `error`. The default `create` constructor now accepts a new "files" package root authority `FileAuth` as well as the global root authority `AmbientAuth`, while the new `from` constructor uses `FilePath`.

The result of this change is that three user changes are needed, namely around `create`, `from`, and `mkdtemp`. Any place where previously `AmbientAuth` was accepted now also accepts `FileAuth`.

Prior to this change, `create` could be used with `AmbientAuth` as in:

```pony
let ambient: AmbientAuth = ...
let filepath: FilePath = FilePath(ambient, path)?
```

After this change, `create` can be used with `AmbientAuth` or `FileAuth` -- note that this can no longer fail:

```pony
let ambient: AmbientAuth = ...
let filepath: FilePath = FilePath(ambient, path)
```

or 

```pony
let fileauth: FileAuth = ...
let filepath: FilePath = FilePath(fileauth, path)
```

---

Prior to this change, `create` could be used with `FilePath` as in:

```pony
let filepath: FilePath = ...
let subpath = FilePath(filepath, path)?
```

After this change, construction with an existing `FilePath` must use `from`:

```pony
let filepath: FilePath = ...
let subpath = FilePath.from(filepath, path)?
```

---

Prior to this change, `mkdtemp` could be used with `AmbientAuth` or `FilePath` as in:

```pony
let ambient: AmbientAuth = ...
let tempdir = FilePath.mkdtemp(ambient, prefix)?
```

or

```pony
let filepath: FilePath = ...
let tempdir = FilePath.mkdtemp(filepath, prefix)?
```

After this change, `mkdtemp` can also use `FileAuth` -- note can still fail:

```pony
let fileauth: FileAuth = ...
let tempdir = FilePath.mkdtemp(fileauth, prefix)?
```