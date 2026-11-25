## Add viewpoint adaptation hint to assignment errors

When accessing a field or calling a method through a non-`ref` receiver, viewpoint adaptation changes the result's capability. Previously, the error message showed only the adapted capability and pointed at the original type annotation, with no explanation of why the two differed.

```
let a: Array[String ref] val = ...
let s: String ref = a(0)?
```

```
String val is not a subtype of String ref^: val is not a subcap of ref^
```

The annotation says `ref`; the error says `val`. The `val` capability of `a` adapted `ref` to `val`, but nothing in the message showed that chain.

The compiler now adds an informational line identifying the receiver and its capability:

```
a val reference to this Array[String val] viewpoint-adapts field and method types
```
