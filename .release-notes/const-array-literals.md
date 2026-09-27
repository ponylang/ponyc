## Optimize constant val array literals

Array literals whose elements are all constants inside a `recover val` block are now compiled the same way string literals are -- as a single constant instead of one function call per element.

```pony
let png_header: Array[U8] val = recover val
  [as U8: 0x89; 0x50; 0x4E; 0x47]
end
```

This applies to arrays of any integer, float, or bool type. The elements must all be literals and the array must be `val`.

Programs that embed large binary data as byte-array literals now use far less memory during compilation. On 32-bit ARM targets, this was enough to prevent LTO from exhausting the address space during linking.
