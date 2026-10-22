## Fix JSONPath stack overflow on deeply nested queries

JSONPath queries with deeply nested expressions — thousands of nested parentheses, filter selectors, boolean operators, negations, or `length()` calls — crashed with a stack overflow. Common nesting patterns now parse and evaluate at arbitrary depth. Remaining nesting patterns that the parser handles recursively are rejected with a `JSONPathParseError` when nesting exceeds 1024 levels.
