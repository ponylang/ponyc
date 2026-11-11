## Fix integer overflow in Array.concat and Array.append bounds arithmetic

Passing a large `len` to `Array.concat` could write past the end of the array's allocation. `Array.append` had the same vulnerability when the array was already large.
