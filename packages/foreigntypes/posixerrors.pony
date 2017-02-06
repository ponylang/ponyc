// This file defines the standard Posix error to make FFI calls easier
// to manage and more consistent.

primitive EBADF
  fun apply(): I32 => 9

primitive EEXIST
  fun apply(): I32 => 17

primitive EACCES
  fun apply(): I32 => 13

