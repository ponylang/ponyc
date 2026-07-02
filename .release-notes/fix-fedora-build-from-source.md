## Fix building ponyc from source on Fedora and other RPM-based distributions

Building ponyc from source on Fedora (and other RPM-based distributions such as RHEL, CentOS, Rocky, and openSUSE) failed during the link of `pony-lsp` and `pony-lint`:

```
Error:
unable to link: ld.lld: error: relocation R_X86_64_32 cannot be used against local symbol; recompile with -fPIC
>>> defined in build/release/libponyc-standalone.a(...)
```

ponyc now builds successfully on these platforms.
