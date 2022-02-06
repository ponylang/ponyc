## Remove out parameter from `pony_os_stdin_read`

The signature of the `pony_os_stdin_read` function has been simplified to remove the `bool* out_again` parameter.

```diff
-PONY_API size_t pony_os_stdin_read(char* buffer, size_t space, bool* out_again)
+PONY_API size_t pony_os_stdin_read(char* buffer, size_t space)
```

It is permitted to call the `pony_os_stdin_read` function again in a loop if the return value is greater than zero, and the platform is not windows. Given that those two conditions are enough information to make a decision, the `out_again` parameter is not needed, and can be removed.

Technically this is a breaking change, because the function is prefixed with `pony_` and is thus a public API. But it is unlikely that any code out there is directly using the `pony_os_stdin_read` function, apart from the `Stdin` actor in the standard library, which has been updated in its internal implementation details to match the new signature.