## Fix IRegexpCompiler.parse crashing on deeply nested group patterns

Parsing an I-Regexp pattern with deeply nested groups — such as thousands of `(` followed by a character and matching `)` — crashed with a stack overflow. The parser now handles arbitrary nesting depth without crashing.
