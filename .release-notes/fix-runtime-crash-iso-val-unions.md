## Fix runtime crash when sending objects through `iso | val` unions

Sending an object through a union that contains both `iso` and `val` variants of the same class could abort in the garbage collector when the sender consumed the object. These sends now complete without crashing.
