## Fix JSONPathParser rejecting whitespace between segments

`JSONPathParser` rejected valid JSONPath queries that had whitespace between segments, such as `$ .a`, `$.a .b`, and `$.a ['b']`. RFC 9535 section 2.1.1 allows optional whitespace before each segment. These queries now parse and evaluate correctly.
