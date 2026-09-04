## Fix compiler crash on unresolved type names in object literal bodies

The compiler crashed when invalid code used an undefined type name inside a lambda or object literal body and the source was split across multiple files. The same code in a single file correctly reported "can't find definition of 'X'". The compiler now reports that error in both cases.
