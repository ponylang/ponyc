## Fix compiler crash when compiling very long expression sequences

The compiler could crash when compiling a very long sequence of expressions, such as a large array or tuple literal, a long argument list, or a long function or method body — the kind of thing code generators often produce. Sequences of this kind now compile without crashing, regardless of how many expressions they contain.
