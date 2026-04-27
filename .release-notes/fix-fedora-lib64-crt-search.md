## Fix linking failures on Fedora and other RPM-based distributions

Starting with ponyc 0.61.1, attempting to link a Pony program on Fedora (and other RPM-based distributions such as RHEL, CentOS, Rocky, and openSUSE) failed with:

```
Error:
could not find libc CRT objects in sysroot ''
```

ponyc now locates the C runtime startup objects on these distributions, and linking succeeds.
