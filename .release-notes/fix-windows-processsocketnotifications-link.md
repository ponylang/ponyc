## Fix Windows link failure for `ProcessSocketNotifications`

On Windows, compiling a program could fail at link time with `undefined symbol: __declspec(dllimport) ProcessSocketNotifications`, even a program that does no networking. The runtime's socket backend calls that Winsock function, and some Windows SDKs do not provide it from the system libraries ponyc was linking. ponyc now also links `mswsock.lib`, the import library for that symbol, so it resolves.

`ProcessSocketNotifications` was introduced in the Windows SDK for build 20348 (Windows 11 / Windows Server 2022) and does not exist in older SDKs. The runtime requires it, so ponyc now checks the installed SDK version and, if it is older than 10.0.20348.0, stops with a clear message telling you to install a newer SDK — instead of failing later with a confusing linker error.
