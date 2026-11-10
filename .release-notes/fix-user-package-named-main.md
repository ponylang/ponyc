## Fix user package named `main` colliding with program module at link

A program with a sub-package in a directory named `main` (or `Main`, `MAIN`, or any other casing) failed to link. The compiler assigned duplicate `.bc` filenames to the user package and the program's own module, producing a linker error. The compiler now detects the collision and disambiguates the user package's filename.
