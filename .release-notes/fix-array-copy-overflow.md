## Fix integer overflow in Array.copy_from and Array.copy_to bounds arithmetic

Passing a large index to `Array.copy_from` or `Array.copy_to` could write past the end of the destination array's allocation. `copy_from` could also read past the end of the source array.
