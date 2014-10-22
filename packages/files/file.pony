class File
  var fd: I32

  // Open for read/write, creating if it doesn't exist.
  new create(path: String) ? =>
    error
