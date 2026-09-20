## Improve error messages for method lookup on anonymous types

When calling a nonexistent method on a lambda, object literal, or partial application, the error message showed the compiler's internal name for the type (e.g., `$1$0`) instead of something readable. The error now says "anonymous type" and lists the methods available on it, so you can see what you can actually call.

Before:

```
couldn't find 'string' in '$1$0'
```

After:

```
couldn't find 'string' in anonymous type
    it has a method named 'apply'
```
