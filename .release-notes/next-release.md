## Fix compiler crash related to type parameter references

Previously, if a method signature in a trait or interface referenced a type
parameter before the type parameter itself was defined, the compiler would
crash. This is now fixed.

