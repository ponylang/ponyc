## Remove `logger` package from the standard library

The `logger` package is intentionally limited in its functionality and leans heavily into reducing messaging overhead between actors to the point of intentionally sacrificing functionality.

Our stated belief as a core team has been that the standard library isn't "batteries included". We have also stated that libraries were we believe it would be "relatively easy" for multiple competing standards to appear in the community shouldn't be included in the standard library unless other factors make inclusion important.

Some other factor we have discussed in the past include:

- Not having a standard library version would make interop between different 3rd-party Pony libraries difficult.
- The functionality is required or called for by an interface in one or more standard library classes.
- We consider the having the functionality provided to be core to the "getting started with Pony experience".

We don't believe that any of above 3 points apply to the `logger` package. Therefore, we've removed `logger` from the standard library and turned it into it's own [stand alone library](https://github.com/ponylang/logger).

If you were using the `logger` package, visit it's new home and follow the "How to Use" directions.
