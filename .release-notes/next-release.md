## Ubuntu 26.04 added as a supported platform

We've added arm64 and amd64 builds for Ubuntu 26.04. We'll be building ponyc releases for it until it stops receiving security updates in 2031. At that point, we'll stop building releases for it.

## Add LSP `textDocument/signatureHelp` support

The Pony language server now supports `textDocument/signatureHelp`, providing parameter hints when the cursor is inside a call expression. The popup shows the full method signature with the active parameter highlighted, and includes the method's docstring when present.

Triggered by `(` and `,` characters, consistent with standard LSP conventions.

Signature help is driven by the compiled AST and requires the file to be saved — it is not available mid-keystroke while the file has unsaved edits.

## Fix linking failures on Fedora and other RPM-based distributions

Starting with ponyc 0.61.1, attempting to link a Pony program on Fedora (and other RPM-based distributions such as RHEL, CentOS, Rocky, and openSUSE) failed with:

```
Error:
could not find libc CRT objects in sysroot ''
```

ponyc now locates the C runtime startup objects on these distributions, and linking succeeds.

## Remove binutils-gold from nightly and release Docker images

The `binutils-gold` package has been removed from the `ponylang/ponyc` nightly and release Docker images. Nothing in ponyc uses the gold linker, and gold is upstream-deprecated and being phased out of distro repositories.

If your build inside one of these images relies on `binutils-gold` being present, you will need to install it explicitly with `apk add binutils-gold`.

## Reject wrong-architecture libc startup objects on multilib hosts

Compiling a Pony program on a multilib Linux host could fail with a confusing arch-mismatch error from the embedded linker. For example, on an x86_64 Fedora system with `glibc-devel.i686` installed, ponyc would pick up the 32-bit `/usr/lib/crt1.o` and the link would fail with:

```
ld.lld: error: /usr/lib/crt1.o is incompatible with elf_x86_64
```

ponyc now validates the architecture of each candidate `crt1.o` and skips ones that don't match the target. If no matching `crt1.o` is found, the error message names the target architecture instead of falling through to the linker's lower-level error.

## Add pony-lsp inlay hints for function parameter types

pony-lsp now shows capability hints on function parameter type annotations where the capability is omitted from source.

```diff
-fun box greet(name: String, items: Array[String]): None val =>
+fun box greet(name: String val, items: Array[String val] ref): None val =>
   None
```

Hints appear after each type name (and after `]` for generic types). Parameters with explicit capabilities are unaffected — no hint is shown when the capability is already written out.

Type parameter references (e.g. `T` in `Array[T]`) have no fixed capability, so they produce no hint.

## Fix spurious pony-lsp inlay hints on primitive types

Primitive types were showing extra unexpected inlay hints alongside the hints for user-defined methods. This is now fixed.

## Fix match exhaustiveness for Bool value patterns in tuples

Previously, matching on a `Bool` inside a tuple required an `else` clause even when both `true` and `false` were covered:

```pony
primitive Foo
  fun apply(x: (String, Bool)): Bool =>
    match x
    | (_, true) => true
    | (_, false) => false
    end
```

```
Error:
main.pony:3:5: function body isn't the result type
```

This has been fixed. Bool value patterns inside tuples now participate in exhaustiveness checking, so `(_, true)` and `(_, false)` correctly cover `(String, Bool)`. This also works with nested tuples, multiple Bool elements, and Bool type aliases.

