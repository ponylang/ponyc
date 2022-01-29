##  Add "nodoc" annotation

The can be used to control the pony compilers documentation generation, any structure that can have a docstring (except packages) can be annotated with \nodoc\ to prevent documentation from being generated.

This replaces a hack that has existed in the documentation generation system for several years to filter out items that were "for testing" by looking for Test and _Test at the beginning of the name as well as providing UnitTest or TestList.

Note that the "should we include this package" hack to filter oupackages called "test" and "builtin_test" still exists for the timbeing until we have further discussion.
