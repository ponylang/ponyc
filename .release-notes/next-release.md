## Fix compiler crash related to type parameter references

Previously, if a method signature in a trait or interface referenced a type
parameter before the type parameter itself was defined, the compiler would
crash. This is now fixed.

## Fix early pipe shutdown with Windows' ProcessMonitor

Due to incorrect handling of a Windows pipe return value, the ProcessMonitor would sometimes shut down its pipe connections to external processes before it should have.

## Fix literal inference with partial functions

Before this change, code such as `1~add(2)` would hit an assertion error when the compiler tried to infer the type of the literal `2`. The compiler tries to find the type of the receiver of the function (in this case `1`), but before it lacked the ability to do so when using partial functions. In those cases, the compiler would try to look at the type of the `~` token, which is not a valid value literal, and as such it would fail.

