## Fix assert_no_error returning true on failure

`TestHelper.assert_no_error` and `PropertyHelper.assert_no_error` returned `true` when the assertion failed. Code that branched on the return value took the success path after a failure:

```pony
if h.assert_no_error({()? => error}) then
  // this ran even though the assertion failed
end
```

Both methods now return `false` on failure, matching every other assertion method.
