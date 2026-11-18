## Fix the argument buffer leak when ponyc exits during option parsing

`ponyc` no longer leaks memory when it exits after printing help or version information, printing the grammar, or rejecting an unrecognized option.
