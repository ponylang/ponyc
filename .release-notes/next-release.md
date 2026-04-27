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

