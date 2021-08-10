## Split `FilePath` construction into two methods

`FilePath` previously had only one constructor, the default `create`, which used a union of `(AmbientAuth | FilePath)`. The first half of this union `AmbientAuth` could never `error`, while the second `FilePath` could `error`. By splitting construction into two methods, we now have a default `create` which cannot `error` -- now using a "files" package root authority `FileAuth` rather than global root authority `AmbientAuth` -- as well as a new `from` constructor using `FilePath`.
