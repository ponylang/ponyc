## Fix compiler crash when calling methods on invalid shift expressions

The compiler would crash with a segmentation fault when a method call was chained onto a bit-shift expression with an oversized shift amount. For example, `y.shr(33).string()` where `y` is a `U32` would crash instead of reporting the "shift amount greater than type width" error. The shift amount error was detected internally but the crash occurred before it could be reported. Standalone shift expressions like `y.shr(33)` were not affected and correctly produced an error message.
