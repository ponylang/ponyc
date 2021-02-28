## Fix compiler crash related to type parameter references

Previously, if a method signature in a trait or interface referenced a type
parameter before the type parameter itself was defined, the compiler would
crash. This is now fixed.

## Fix early pipe shutdown with Windows' ProcessMonitor

Due to incorrect handling of a Windows pipe return value, the ProcessMonitor would sometimes shut down its pipe connections to external processes before it should have.

