## Fix memory safety problem with Array.from_cpointer

Previously, the `from_cpointer` method in arrays would trust the user to pass a valid pointer. This created an issue where the user could pass a null pointer using `Pointer.create`, leading to a situation where memory safety could be violated by trying to access an element of the array. This change makes it safe to pass a null pointer to `from_cpointer`, which will create a zero-length array.

