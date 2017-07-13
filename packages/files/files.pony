"""
# Files package

The Files package provides classes for working with files and
directories.

Files are identified by `FilePath` objects, which represent both the
path to the file and the capabilites for accessing the file at that
path. `FilePath` objects can be used with the `CreateFile` and
`OpenFile` primitives and the `File` class to get a reference to a
file that can be used to write to and/or read from the file. It can
also be used with the `Directory` object to get a reference to a
directory object that can be used for directory operations.

The `FileLine` class allows a file to be accessed one line at a time.

The `FileStream` actor provides the ability to asynchronously write to
a file.

The `Path` primitive can be used to do path-related operations on
strings and characters.

# Example program

This program opens the files that are given as command line arguments
and prints their contents.

```pony
use "files"

actor Main
  new create(env: Env) =>
    try
      for file_name in env.args.slice(1).values() do
        let path = FilePath(env.root as AmbientAuth, file_name)?
        match OpenFile(path)
        | let file: File =>
          while file.errno() is FileOK do
            env.out.write(file.read(1024))
          end
        else
          env.err.print("Error opening file '" + file_name + "'")
        end
      end
    end
```
"""
