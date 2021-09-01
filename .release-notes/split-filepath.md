## Split `FilePath` construction into two methods

`FilePath` previously had only one constructor, the default `create`, which used a union of `(AmbientAuth | FilePath)`. The first half of this union `AmbientAuth` could never `error`, while the second `FilePath` could `error`. By splitting construction into two methods, we now have a default `create` constructor which cannot `error` and a new `from` constructor which can `error`. The default `create` constructor now accepts a new "files" package root authority `FileAuth` as well as the global root authority `AmbientAuth`, while the new `from` constructor uses `FilePath`.

The result of this change is that three user changes are needed, namely around `create`, `from`, and `mkdtemp`. Any place where previously `AmbientAuth` was accepted now also accepts `FileAuth`.

Before `create` using `AmbientAuth` to now `create` using `AmbientAuth` or `FileAuth` -- note can no longer fail:

```pony
let ambient: AmbientAuth = ...
let filepath: FilePath = FilePath(ambient, path)?
```

becomes

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

Before `create` using `FilePath` to now `from` using `FilePath`:

```pony
let filepath: FilePath = ...
let subpath = FilePath(filepath, path)?
```

becomes

```pony
let filepath: FilePath = ...
let subpath = FilePath.from(filepath, path)?
```

---

Before `mkdtemp` using `AmbientAuth` to now `mkdtemp` using `AmbientAuth` or `FileAuth` -- note can still fail:

```pony
let ambient: AmbientAuth = ...
let tempdir = FilePath.mkdtemp(ambient, prefix)?
```

becomes

```pony
let ambient: AmbientAuth = ...
let tempdir = FilePath.mkdtemp(ambient, prefix)?
```

```pony
let fileauth: FileAuth = ...
let tempdir = FilePath.mkdtemp(fileauth, prefix)?
```