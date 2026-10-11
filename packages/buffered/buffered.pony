"""
# Buffered Package

The Buffered package provides two classes, `Writer` and `Reader`, for
writing and reading messages using common encodings. These classes are
useful when dealing with things like network data and binary file
formats.

## Example program

```pony
use "buffered"

actor Main
  new create(env: Env) =>
    let reader = Reader
    let writer = Writer

    writer.u32_be(42)
    writer.f32_be(3.14)

    let b = recover iso Array[U8] end

    for chunk in writer.done().values() do
      b.append(chunk)
    end

    reader.append(consume b)

    try
      env.out.print(reader.u32_be()?.string()) // prints 42
      env.out.print(reader.f32_be()?.string()) // prints 3.14
    end
```
"""
