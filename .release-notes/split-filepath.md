## Split `FilePath` construction into two methods

`FilePath` previously had only one constructor, the default `create`, which used a union of `(AmbientAuth | FilePath)`. The first half of this union `AmbientAuth` could never `error`, while the second `FilePath` could `error`. By splitting construction into two methods, we now have a default `create` constructor which cannot `error` and a new `from` constructor which can `error`. The default `create` constructor uses a new "files" package root authority `FileAuth` rather than global root authority `AmbientAuth`, while the new `from` constructor uses `FilePath`.

The result of this change is that three user changes are needed, namely around `create`, `from`, and `mkdtemp`.

Before `create` using `AmbientAuth` to now `create` using `FileAuth`:

```pony
let ambient: AmbientAuth = ...
let filepath: FilePath = FilePath(ambient, path)?
```

becomes

```pony
let ambient: AmbientAuth = ...
let filepath: FilePath = FilePath(FileAuth(ambient), path)
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

Before `mkdtemp` using `AmbientAuth` to now `mkdtemp` using `FileAuth`:

```pony
let ambient: AmbientAuth = ...
let tempdir = FilePath.mkdtemp(ambient, prefix)?
```

becomes

```pony
let ambient: AmbientAuth = ...
let tempdir = FilePath.mkdtemp(FileAuth(ambient), prefix)?
```
