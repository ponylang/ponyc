
## Reminder: when adding/removing/renaming new examples

The `main.pony` source takes advantage of Pony's `use` statement
to compile all example source code in this directory into a single
executable.  The final app doesn't do much, but it does force the
compiler to check all example apps for syntax errors, data type
errors, reference capability errors, and all other compiler errors
that should not exist in this collection of Pony code.

Therefore, to avoid `make test-examples` or `test-ci` errors, please do
the following whenever a new example app is added (or an example
app is removed or renamed), please:


1. Update the `main.pony` source to add/remove/change the name of the app.
2. Run the `make test-examples` command to verify that all example
   apps can be compiled cleanly.

